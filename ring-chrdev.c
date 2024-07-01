#include <linux/module.h>
#include <linux/printk.h>

static int __init ring_init(void) {
#ifdef DEBUG
    pr_debug("Initializing ring-chrdev\n");
#endif

    return 0;
}

static void __exit ring_cleanup(void) {
#ifdef DEBUG
    pr_debug("Cleaning up ring-chrdev\n");
#endif
}

module_init(ring_init);
module_exit(ring_cleanup);

MODULE_AUTHOR("Nikolay/i Nechaev");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Ring buffer character device");
