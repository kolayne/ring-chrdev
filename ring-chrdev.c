#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>

// Disable `pr_debug` when not `DEBUG`
#ifndef DEBUG
#undef pr_debug
#define pr_debug(...)
#endif

static int ring_open(struct inode *inode, struct file *filp) {
    pr_debug("ring_open: major=%u, minor=%u\n", imajor(inode), iminor(inode));

    return 0;
}

static int ring_release(struct inode *inode, struct file *filp) {
    pr_debug("ring_release: major=%u, minor=%u\n", imajor(inode), iminor(inode));

    return 0;
}

static ssize_t ring_read(struct file *filp, char __user *buffer,
                         size_t length, loff_t *offset)
{
    pr_debug("ring_read: offset=%lld, len=%ld\n", *offset, length);

    // Nothing to be read
    return 0;
}

static ssize_t ring_write(struct file *filp, const char __user *buff,
                          size_t length, loff_t *offset)
{
    pr_debug("ring_write: offset=%lld, len=%ld\n", *offset, length);

    // Operation not supported
    return -ENOSYS;
}


static const struct file_operations chardev_fops = {
    // Make sure this module isn't unloaded while a file is open.
    .owner = THIS_MODULE,

    .read = ring_read,
    .write = ring_write,
    .open = ring_open,
    .release = ring_release,
};


#define RING_CHRDEV_NAME "ring"
static int major;

static int __init ring_init(void) {
    major = register_chrdev(0, RING_CHRDEV_NAME, &chardev_fops);
    if (major < 0) {
        pr_err("ring-chrdev: failed to `register_chrdev`: %d\n", major);
        return major;
    }

    pr_info("ring-chrdev: registered with major=%d\n", major);

    return 0;
}

static void __exit ring_cleanup(void) {
    pr_debug("ring-chrdev: cleaning up\n");

    unregister_chrdev(major, RING_CHRDEV_NAME);
}

module_init(ring_init);
module_exit(ring_cleanup);

MODULE_AUTHOR("Nikolay/i Nechaev");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ring buffer character device");
