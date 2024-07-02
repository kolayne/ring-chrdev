#!/bin/sh

test_nonblock_read_empty() {
  # Must fail due to EWOULDBLOCK
  dd if=/dev/ring iflag=nonblock 2>&1 | grep "Resource temporarily unavailable"
}

test_nonblock_overfill() {
  # Assuming buffer size 10
  ! echo abc1234567890 | dd if=/dev/stdin of=/dev/ring oflag=nonblock
  [ "$(dd iflag=nonblock if=/dev/ring bs=3)" = abc1234567 ]
}
