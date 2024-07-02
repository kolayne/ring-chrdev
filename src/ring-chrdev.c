#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>


#ifndef DEBUG

// Disable `pr_debug` when not DEBUG
#undef pr_debug
#define pr_debug(...) do {} while (0)

#endif  // DEBUG


#define RING_CHRDEV_NAME "ring"
static int major;

/// Ring buffer size
size_t ring_capacity = 10;

struct ring {
    // Lock protecting all of the ring fields
    spinlock_t lock;
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
    spin_lock(&ring.lock);

    pr_debug("ring_read: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_read at the beginning: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    FASSERT(ring.read_pos < ring_capacity, -EIO);

    size_t toread = min(length, ring.size);
    if (!toread) {
        RETURN(0);
    }

    if (ring.read_pos + toread > ring_capacity) {
        /*
         * Wrapping around the buffer. Stop at the end of it,
         * let the userspace call `read` again.
         */
        toread = ring_capacity - ring.read_pos;
        FASSERT(toread > 0, -EIO);
    }

    size_t read = toread - copy_to_user(buf, &ring.buf[ring.read_pos], toread);
    if (!read) {
        RETURN(-EFAULT)
    }

    ring.size -= read;
    ring.read_pos += read;
    // Wrap around
    if (ring.read_pos == ring_capacity) {
        ring.read_pos = 0;
    }
    FASSERT(ring.read_pos < ring_capacity, -EIO);

    pr_debug("ring_read at the end: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    RETURN(read);

out:
    spin_unlock(&ring.lock);
    return ret;
}

static ssize_t ring_write(struct file *filp, const char __user *buf,
                          size_t length, loff_t *offset)
{
    ssize_t ret;
    spin_lock(&ring.lock);

    pr_debug("ring_write: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_write at the beginning: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    FASSERT(ring.size <= ring_capacity, -EIO);

    size_t towrite = min(length, ring_capacity - ring.size);
    if (!towrite) {
        RETURN(-ENOSPC);
    }

    size_t write_pos = (ring.read_pos + ring.size) % ring_capacity;

    if (write_pos + towrite > ring_capacity) {
        /*
         * Wrapping around the buffer. Stop at the end of it,
         * let the userspace call `write` again.
         */
        towrite = ring_capacity - write_pos;
    }

    size_t wrote = towrite - copy_from_user(&ring.buf[write_pos], buf, towrite);
    if (!wrote) {
        RETURN(-EFAULT);
    }

    ring.size += wrote;
    FASSERT(ring.size <= ring_capacity, -EIO);

    pr_debug("ring_write at the end: ring.size=%ld, ring.read_pos=%ld\n", ring.size, ring.read_pos);
    RETURN(wrote);


out:
    spin_unlock(&ring.lock);
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
        pr_err("ring-chrdev: ring buffer capacity must be positive");
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

    spin_lock_init(&ring.lock);
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
