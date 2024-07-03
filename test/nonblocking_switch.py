#!/usr/bin/env python3

# Switches FD 3 to non-blocking mode, writes `argv[1]`
# (or an empty string, if `argv[1]` is not given) to
# FD 3, exits successfully regardless of I/O errors
# (even if FD 3 was not open).

from sys import argv
import os


# If `argv[1]` is given, write that, otherwise write
# an empty string.
s = (argv[1:] or [''])[0]

try:
    f = os.fdopen(3, mode='wb')
    os.set_blocking(f.fileno(), False)
    f.write(s.encode('utf-8'))
    f.flush()
except OSError:
    pass
