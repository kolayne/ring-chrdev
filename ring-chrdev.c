#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>

#include "helpers.h"


#define RING_CHRDEV_NAME "ring"
static int major;

// TODO: synchronization!
static char *ring_buf;
static size_t ring_capacity = 10;
static size_t ring_size = 0;
// `ring_pos` points at where to read
static size_t ring_pos = 0;


static int ring_open(struct inode *inode, struct file *filp) {
    pr_debug("ring_open: major=%u, minor=%u\n", imajor(inode), iminor(inode));

    return 0;
}

static int ring_release(struct inode *inode, struct file *filp) {
    pr_debug("ring_release: major=%u, minor=%u\n", imajor(inode), iminor(inode));

    return 0;
}

static ssize_t ring_read(struct file *filp, char __user *buf,
                         size_t length, loff_t *offset)
{
    pr_debug("ring_read: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_read at the beginning: ring_size=%ld, ring_pos=%ld\n", ring_size, ring_pos);
    FASSERT(ring_pos < ring_capacity, -EIO);

    size_t toread = min(length, ring_size);
    if (!toread) {
        return 0;
    }

    if (ring_pos + toread > ring_capacity) {
        /*
         * Wrapping around the buffer. Stop at the end of it,
         * let the userspace call `read` again.
         */
        toread = ring_capacity - ring_pos;
        FASSERT(toread > 0, -EIO);
    }

    size_t read = toread - copy_to_user(buf, &ring_buf[ring_pos], toread);
    if (!read) {
        return -EFAULT;
    }

    ring_size -= read;
    ring_pos += read;
    // Wrap around
    if (ring_pos == ring_capacity) {
        ring_pos = 0;
    }
    FASSERT(ring_pos < ring_capacity, -EIO);

    pr_debug("ring_read at the end: ring_size=%ld, ring_pos=%ld\n", ring_size, ring_pos);
    return read;
}

static ssize_t ring_write(struct file *filp, const char __user *buf,
                          size_t length, loff_t *offset)
{
    pr_debug("ring_write: offset=%lld, len=%ld\n", *offset, length);
    pr_debug("ring_write at the beginning: ring_size=%ld, ring_pos=%ld\n", ring_size, ring_pos);
    FASSERT(ring_size <= ring_capacity, -EIO);

    size_t towrite = min(length, ring_capacity - ring_size);
    if (!towrite) {
        return -ENOSPC;
    }

    size_t write_pos = (ring_pos + ring_size) % ring_capacity;

    if (write_pos + towrite > ring_capacity) {
        /*
         * Wrapping around the buffer. Stop at the end of it,
         * let the userspace call `write` again.
         */
        towrite = ring_capacity - write_pos;
    }

    size_t wrote = towrite - copy_from_user(&ring_buf[write_pos], buf, towrite);
    if (!wrote) {
        return -EFAULT;
    }

    ring_size += wrote;
    FASSERT(ring_size < ring_capacity, -EIO);

    pr_debug("ring_write at the end: ring_size=%ld, ring_pos=%ld\n", ring_size, ring_pos);
    return wrote;
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
    if (ring_capacity <= 0) {
        pr_err("ring-chrdev: ring buffer capacity must be positive");
        return E_INVALID_CAPACITY;
    }

    if (ring_capacity > PAGE_SIZE) {
        pr_warn("ring-chrdev: the ring buffer capacity %lu is greater than the page size %lu\n",
                ring_capacity, PAGE_SIZE);
    }

    // FIXME: allocating kernel memory without zeroing it out and letting
    // user read it is a security threat.
    ring_buf = kmalloc(ring_capacity, GFP_KERNEL);
    if (!ring_buf) {
        pr_err("ring-chrdev: memory allocation failed\n");
        return E_NOMEM;
    }

    major = register_chrdev(0, RING_CHRDEV_NAME, &chardev_fops);
    if (major < 0) {
        pr_err("ring-chrdev: failed to `register_chrdev`: %d\n", major);
        return E_REGISTRATION_FAILED;
    }

    pr_info("ring-chrdev: registered with major=%d\n", major);

    return 0;
}

static void __exit ring_cleanup(void) {
    pr_debug("ring-chrdev: cleaning up\n");

    unregister_chrdev(major, RING_CHRDEV_NAME);

    kfree(ring_buf);
}

module_init(ring_init);
module_exit(ring_cleanup);

MODULE_AUTHOR("Nikolay/i Nechaev");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ring buffer character device");
