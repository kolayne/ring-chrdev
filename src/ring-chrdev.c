#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#include "../include/ring_ioctl.h"


#ifdef DEBUG

#define MAX_WHILE_ITERATIONS 10

#else  // DEBUG

#define MAX_WHILE_ITERATIONS (10 * 1000)

// Disable `pr_debug` when not DEBUG
#undef pr_debug
#define pr_debug(...) do {} while (0)

#endif  // DEBUG


#define RING_CHRDEV_NAME "ring"
static int major;

/// Ring buffer size
size_t ring_capacity = 10;
module_param(ring_capacity, ulong, 0);
MODULE_PARM_DESC(ring_capacity, "Max buffer size of the ring");

struct ring {
    // Lock protecting all of the ring fields
    struct mutex lock;
    // Wait queue for synchronization between reads and writes.
    wait_queue_head_t wq;

    /// Buffer with the ring content
    char *buf;
    /// Ready-to-read data (bytes)
    size_t size;
    /// `buf[read_pos]` is the next character to read
    size_t read_pos;

    /// Last writer to the file (0 if never written to)
    struct last_acc lastw;
    /// Last reader of the file (0 if never read from)
    struct last_acc lastr;
};

// TODO: support multiple rings!
static struct ring ring;


static int ring_open(struct inode *inode, struct file *filp) {
    pr_debug("ring_open: major=%u, minor=%u\n", imajor(inode), iminor(inode));

    return 0;
}

static int ring_release(struct inode *inode, struct file *filp) {
    pr_debug("ring_release: major=%u, minor=%u\n", imajor(inode), iminor(inode));

    return 0;
}


#define CLEANRET(val) do {  \
    ret = (val);            \
    goto clean_ret;         \
} while (0)

#ifdef DEBUG

#define ASSERTORCLEANRET(cond, retval)  do {           \
    if (!(cond)) {                                     \
            pr_err("Assertion failed: %s at %s:%d\n",  \
                   #cond, __FILE__, __LINE__);         \
            CLEANRET(retval);                          \
    }                                                  \
} while (0);

#else  // DEBUG

#define ASSERTORCLEANRET(cond, retval) do {} while(0)

#endif  // DEBUG


static ssize_t ring_read(struct file *filp, char __user *buf,
                         size_t length, loff_t *offset)
{
    ring.lastr.tgid = current->tgid;
    ring.lastr.uid = current_uid().val;

    int mutex_intr = 0;
    ssize_t ret;

    mutex_intr = mutex_lock_interruptible(&ring.lock);
    if (mutex_intr)
        CLEANRET(mutex_intr);

    pr_debug("ring_read: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_read at the beginning: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    ASSERTORCLEANRET(ring.read_pos < ring_capacity, -EIO);

    if (length <= 0) {
        // Nothing to be done
        CLEANRET(0);
    }

    int interrupted = 0;

    // If no content to read, either refuse or block.
    if (ring.size <= 0 && (filp->f_flags & O_NONBLOCK)) {
        CLEANRET(-EWOULDBLOCK);
    }
    while (ring.size <= 0) {
        pr_debug("ring_read: pausing on `ring.size > 0`\n");
        mutex_unlock(&ring.lock);  // Let another thread work
        interrupted = wait_event_interruptible(ring.wq, ring.size > 0);
        pr_debug("ring_read: woke up on `ring.size > 0`. interrupted=%d\n", interrupted);
        mutex_intr = mutex_lock_interruptible(&ring.lock);
        if (mutex_intr)
            CLEANRET(mutex_intr);

        if (interrupted) {
            // `wait_event_interruptible` was interrupted, returning `-ERESTARTSYS`.
            // The return value is decided outside, depending on whether there was
            // anything read already.
            CLEANRET(interrupted);
        } else if (ring.size <= 0) {
            // Spurious wake-up
            continue;
        }
    }

#ifdef DEBUG
    // 'err' rather than 'debug' for nicer output
    pr_err("buffer contents before read: %*pE\n", (int)ring_capacity, ring.buf);
#endif

    // Don't read more than there's in the buffer
    length = min(length, ring.size);
    ASSERTORCLEANRET(length > 0, -EIO);
    size_t total_read = 0;
    for (int i = 0; i < 2; ++i) {
        // As the read may cross the buffer boundary, perform two copies: at the buffer tail
        // and (maybe) at the buffer head.
        // If there's no crossing or the first iteration fails, the second one is a no-op.
        size_t to_read = min(length, ring_capacity - ring.read_pos);
        ASSERTORCLEANRET((i != 0) || to_read > 0, -EIO);
        size_t read = to_read - copy_to_user(buf, &ring.buf[ring.read_pos], to_read);
        total_read += read;
        ring.read_pos = (ring.read_pos + read) % ring_capacity;
        ring.size -= read;
        buf += read;
        length -= read;
    }

    wake_up(&ring.wq);
    ASSERTORCLEANRET(ring.read_pos < ring_capacity, -EIO);

    pr_debug("ring_read at the end: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);

    if (total_read > 0) {
        inode_set_atime_to_ts(filp->f_inode, current_time(filp->f_inode));
        CLEANRET(total_read);
    } else if (interrupted) {
        CLEANRET(-ERESTARTSYS);
    } else {
        // If not interrupted and nothing read, it's a buffer problem
        // (the 0-read request case was handled in the very beginning)
        CLEANRET(-EFAULT);
    }

clean_ret:
    if (!mutex_intr)
        mutex_unlock(&ring.lock);
    return ret;
}

static ssize_t ring_write(struct file *filp, const char __user *buf,
                          size_t length, loff_t *offset)
{
    ring.lastw.tgid = current->tgid;
    ring.lastw.uid = current_uid().val;

    int mutex_intr = 0;
    ssize_t ret;

    mutex_intr = mutex_lock_interruptible(&ring.lock);
    if (mutex_intr)
        CLEANRET(mutex_intr);

    pr_debug("ring_write: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_write at the beginning: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    ASSERTORCLEANRET(ring.size <= ring_capacity, -EIO);

    if (!length) {
        CLEANRET(0);
    }

    int interrupted = 0;

    // If no room to write, either refuse or block.
    if (ring.size >= ring_capacity && (filp->f_flags & O_NONBLOCK)) {
        CLEANRET(-EWOULDBLOCK);
    }
    while (ring.size >= ring_capacity) {
        pr_debug("ring_write: pausing on `ring.size < ring_capacity`\n");
        mutex_unlock(&ring.lock);
        interrupted = wait_event_interruptible(ring.wq, ring.size < ring_capacity);
        pr_debug("ring_write: woke up on `ring.size < ring_capacity`. interrupted=%d\n", interrupted);
        mutex_intr = mutex_lock_interruptible(&ring.lock);
        if (mutex_intr)
            CLEANRET(mutex_intr);

        if (interrupted) {
            // Interrupted with a signal. The return value is decided outside,
            // depending on whether have already written anything.
           CLEANRET(interrupted);
        } else if (ring.size >= ring_capacity) {
            // Spurious wake-up
            continue;
        }
    }

    // Don't attempt to write more than there's room in the buffer
    length = min(length, ring_capacity - ring.size);
    ASSERTORCLEANRET(length > 0, -EIO);
    size_t total_written = 0;
    for (int i = 0; i < 2; ++i) {
        // As the read may cross the buffer boundary, perform two copies: at the buffer tail
        // and (maybe) at the buffer head.
        // If there's no crossing or the first iteration fails, the second one is a no-op.
        size_t write_pos = (ring.read_pos + ring.size) % ring_capacity;
        size_t to_write = min(length, ring_capacity - write_pos);
        ASSERTORCLEANRET((i != 0) || to_write > 0, -EIO);
        size_t wrote = to_write - copy_from_user(&ring.buf[write_pos], buf, to_write);
        ring.size += wrote;
        total_written += wrote;
        buf += wrote;
        length -= wrote;
    }

#ifdef DEBUG
    // 'warn' rather than 'debug' for nicer output
    pr_warn("buffer contents after write: %*pE\n", (int)ring_capacity, ring.buf);
#endif

    wake_up(&ring.wq);
    ASSERTORCLEANRET(ring.size <= ring_capacity, -EIO);

    pr_debug("ring_write at the end: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);

    if (total_written > 0) {
        inode_set_mtime_to_ts(filp->f_inode, current_time(filp->f_inode));
        CLEANRET(total_written);
    } else if (interrupted) {
        CLEANRET(-ERESTARTSYS);
    } else {
        // If not interrupted and never wrote anything, it's a buffer problem
        // (the 0-write request case was handled in the very beginning).
        CLEANRET(-EFAULT);
    }

clean_ret:
    if (!mutex_intr)
        mutex_unlock(&ring.lock);
    return ret;
}

static long ring_ioctl(struct file *filp, unsigned cmd, unsigned long arg) {
    struct last_acc __user *acc = (void __user *)arg;
    long ret;

    int mutex_intr = mutex_lock_interruptible(&ring.lock);
    if (mutex_intr)
        CLEANRET(mutex_intr);

    switch (cmd) {
    case IR_LAST_WRITER:
        if (0 != copy_to_user(acc, &ring.lastw, sizeof ring.lastw)) {
            CLEANRET(-EFAULT);;
        }
        break;

    case IR_LAST_READER:
        if (0 != copy_to_user(acc, &ring.lastr, sizeof ring.lastr)) {
            CLEANRET(-EFAULT);;
        }
        break;

    default:
        CLEANRET(-EINVAL);
    }

    // Success
    CLEANRET(0);


clean_ret:
    if (!mutex_intr)
        mutex_unlock(&ring.lock);
    return ret;
}


static const struct file_operations chardev_fops = {
    // Make sure this module isn't unloaded while a file is open.
    .owner = THIS_MODULE,

    .read = ring_read,
    .write = ring_write,
    .open = ring_open,
    .release = ring_release,
    .unlocked_ioctl = ring_ioctl,
};


static int __init ring_init(void) {
    int err = 0;

    if (ring_capacity <= 0) {
        pr_err("ring-chrdev: ring buffer capacity must be positive\n");
        err = -EINVAL;
        goto err_validation;
    }

    if (ring_capacity > PAGE_SIZE) {
        pr_warn("ring-chrdev: the ring buffer capacity %lu is greater than the page size %lu\n",
                ring_capacity, PAGE_SIZE);
    }

    // User cannot read from `ring.buf` before writing into it, so it's ok
    // to allocate memory without zeroing it out.
    ring.buf = kmalloc(ring_capacity, GFP_KERNEL);
    if (!ring.buf) {
        pr_err("ring-chrdev: memory allocation failed\n");
        err = -ENOMEM;
        goto err_kmalloc;
    }

    major = register_chrdev(0, RING_CHRDEV_NAME, &chardev_fops);
    if (major < 0) {
        pr_err("ring-chrdev: failed to `register_chrdev`: %d\n", major);
        err = major;
        goto err_register_chrdev;
    }

    mutex_init(&ring.lock);
    init_waitqueue_head(&ring.wq);

    pr_info("ring-chrdev: registered with major=%d\n", major);

    return 0;


    // For failed init, undo stuff:

    unregister_chrdev(major, RING_CHRDEV_NAME);
err_register_chrdev:
    kfree(ring.buf);
err_kmalloc:
err_validation:
    return err;
}

static void __exit ring_cleanup(void) {
    pr_debug("ring-chrdev: cleaning up\n");

    unregister_chrdev(major, RING_CHRDEV_NAME);

    kfree(ring.buf);
}

module_init(ring_init);
module_exit(ring_cleanup);

MODULE_AUTHOR("Nikolay/i Nechaev");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ring buffer character device");
