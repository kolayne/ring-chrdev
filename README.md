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
