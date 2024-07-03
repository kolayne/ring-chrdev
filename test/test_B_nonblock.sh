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

test_switch_to_nonblock() {
  # Assuming buffer size 10

  # When the ring is empty, should successfully write three bytes
  ./nonblocking_switch.py 123 3>/dev/ring
  [ "$(dd if=/dev/ring iflag=nonblock)" = 123 ]

  # When there's too much data, should write only the first portion
  ./nonblocking_switch.py abcdefghijklmnopqrstuvwxyz 3>/dev/ring
  # Attempt to write even more should just terminate
  ./nonblocking_switch.py abcdefghijklmnopqrstuvwxyz 3>/dev/ring
  [ "$(dd if=/dev/ring iflag=nonblock)" = abcdefghij ]
}
