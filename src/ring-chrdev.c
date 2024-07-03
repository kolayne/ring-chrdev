#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/mutex.h>


#ifdef DEBUG

#define MAX_WHILE_ITERATIONS 10

#else  // DEBUG

#define MAX_WHILE_ITERATIONS (10 * 1000)

// Disable `pr_debug` when not DEBUG
#undef pr_debug
#define pr_debug(...) do {} while (0)

#endif  // DEBUG


// Force upper boundary on the number of iterations of a `while` loop
// (just like in NASA :sunglasses:).
// Watch out: the number of iterations for DEBUG is really small.
#define While(cond) \
    for (int _i##__LINE__ = 0; (cond) && _i##__LINE__ < MAX_WHILE_ITERATIONS; ++_i##__LINE__)


#define RING_CHRDEV_NAME "ring"
static int major;

/// Ring buffer size
size_t ring_capacity = 10;

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


#define RETURN(val) {  \
    ret = (val);       \
    goto out;          \
}

#ifdef DEBUG

#define FASSERT(cond, retval)  do {                      \
    if (!(cond)) {                                       \
            pr_err("Assertion failed: %s at %s:%d\n",    \
                   #cond, __FILE__, __LINE__);           \
            RETURN(retval);                              \
    }                                                    \
} while (0);

#else  // DEBUG

#define FASSERT(cond, retval) do {} while(0)

#endif  // DEBUG


static ssize_t ring_read(struct file *filp, char __user *buf,
                         size_t length, loff_t *offset)
{
    ssize_t ret;
    // TODO: lock interruptible
    mutex_lock(&ring.lock);

    pr_debug("ring_read: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_read at the beginning: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    FASSERT(ring.read_pos < ring_capacity, -EIO);

    if (length <= 0) {
        // Nothing to be done
        RETURN(0);
    }

    int interrupted = 0;

    // If no content to read, either refuse or block.
    if (ring.size <= 0 && (filp->f_flags & O_NONBLOCK)) {
        RETURN(-EWOULDBLOCK);
    }
    While (ring.size <= 0) {
        pr_debug("ring_read: pausing on `ring.size > 0`\n");
        mutex_unlock(&ring.lock);  // Let another thread work
        interrupted = wait_event_interruptible(ring.wq, ring.size > 0);
        // TODO: lock interruptible!
        mutex_lock(&ring.lock);
        pr_debug("ring_read: woke up on `ring.size > 0`. interrupted=%d\n", interrupted);

        if (interrupted) {
            // `wait_event_interruptible` was interrupted, returning `-ERESTARTSYS`.
            // The return value is decided outside, depending on whether there was
            // anything read already.
            break;
        } else if (ring.size <= 0) {
            // Spurious wake-up
            continue;
        }
    }

    size_t to_read_now = min(length, ring.size);
    if (ring.read_pos + to_read_now > ring_capacity) {
        // Crossing the buffer boundary. Because we are lazy, cut it here and let
        // the userspace repeat the `read` call.
        to_read_now = ring_capacity - ring.read_pos;
        FASSERT(to_read_now > 0, -EIO);
    }

#ifdef DEBUG
    // 'err' rather than 'debug' for nicer output
    pr_err("buffer contents before read: %*pE\n", (int)ring_capacity, ring.buf);
#endif

    size_t read_now = to_read_now - copy_to_user(buf, &ring.buf[ring.read_pos], to_read_now);
    // `read_now` may be zero if `buf` is inaccessible   => EBADF
    ring.size -= read_now;
    ring.read_pos += read_now;
    if (ring.read_pos == ring_capacity) {
        // Wrap around
        ring.read_pos = 0;
    }
    wake_up(&ring.wq);
    FASSERT(ring.read_pos < ring_capacity, -EIO);

    pr_debug("ring_read at the end: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);

    if (read_now > 0) {
        RETURN(read_now);
    } else if (interrupted) {
        RETURN(-ERESTARTSYS)
    } else {
        // If not interrupted and nothing read, it's a buffer problem
        // (the 0-read request case was handled in the very beginning)
        RETURN(-EFAULT);
    }

out:
    mutex_unlock(&ring.lock);
    return ret;
}

static ssize_t ring_write(struct file *filp, const char __user *buf,
                          size_t length, loff_t *offset)
{
    ssize_t ret;
    // TODO: lock interruptible!
    mutex_lock(&ring.lock);

    pr_debug("ring_write: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_write at the beginning: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    FASSERT(ring.size <= ring_capacity, -EIO);

    if (!length) {
        RETURN(0);
    }

    int interrupted = 0;

    // If no room to write, either refuse or block.
    if (ring.size >= ring_capacity && (filp->f_flags & O_NONBLOCK)) {
        RETURN(-EWOULDBLOCK);
    }
    While (ring.size >= ring_capacity) {
        pr_debug("ring_write: pausing on `ring.size < ring_capacity`\n");
        mutex_unlock(&ring.lock);
        interrupted = wait_event_interruptible(ring.wq, ring.size < ring_capacity);
        // TODO: lock interruptible
        mutex_lock(&ring.lock);
        pr_debug("ring_write: woke up on `ring.size < ring_capacity`. interrupted=%d\n", interrupted);

        if (interrupted) {
            // Interrupted with a signal. The return value is decided outside,
            // depending on whether have already written anything.
            break;
        } else if (ring.size >= ring_capacity) {
            // Spurious wake-up
            continue;
        }
    }

    size_t to_write_now = min(length, ring_capacity - ring.size);
    size_t write_pos = (ring.read_pos + ring.size) % ring_capacity;
    if (write_pos + to_write_now > ring_capacity) {
        // Crossing the buffer boundary. Because we are lazy, cut it here and let
        // the userspace repeat the `write` call.
        to_write_now = ring_capacity - write_pos;
        FASSERT(to_write_now > 0, -EIO);
    }

    size_t wrote_now = to_write_now - copy_from_user(&ring.buf[write_pos], buf, to_write_now);

#ifdef DEBUG
    // 'warn' rather than 'debug' for nicer output
    pr_warn("buffer contents after write: %*pE\n", (int)ring_capacity, ring.buf);
#endif

    // `wrote_now` may be zero, meaning `buf` is inaccessible   => -EFAULT
    ring.size += wrote_now;
    wake_up(&ring.wq);
    FASSERT(ring.size <= ring_capacity, -EIO);

    pr_debug("ring_write at the end: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);

    if (wrote_now > 0) {
        RETURN(wrote_now);
    } else if (interrupted) {
        RETURN(-ERESTARTSYS);
    } else {
        // If not interrupted and never wrote anything, it's a buffer problem
        // (the 0-write request case was handled in the very beginning).
        RETURN(-EFAULT);
    }

out:
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
    // TODO: use `wq` to implement blocking I/O
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
