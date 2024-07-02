# ring-chrdev

Kernel module for a character device implementing a ring buffer

# Build & run

```sh
$ make DEBUG=1  # Build in debug mode
...
$ make DEBUG=0  # Build in release mode
...
$ insmod ring-chrdev.ko  # Load built module
...
$ rmmod ring-chrdev.ko  # Unload loaded module
...
$
```

To generate `compile_commands.json`, use [Bear](https://github.com/rizsotto/Bear):
instead of `make ...` run `bear -- make ...`

# Test

To test, first build the module, then, as root, run:
```sh
$ insmod ring-chrdev.ko
$ # Extract the major chrdev number
$ MAJOR=$(dmesg --level info | grep 'ring-chrdev: registered with major=' | tail -1 | cut -d= -f2)
$ mknod /dev/ring c "$MAJOR" 1  # Create the device file
$ chmod 666 /dev/ring  # If tests are to be run by non-root, give apporpriate permissions
```

Then, run `make test`.
