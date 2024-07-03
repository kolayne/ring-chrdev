#!/bin/sh

test_read_empty() {
  # Must time out
  ! timeout -k0 .1 dd if=/dev/ring of=/dev/full count=1
}

test_123_456() {
  echo -n 123 > /dev/ring
  echo -n 456 > /dev/ring
  [ "$(timeout -k0 .1 dd if=/dev/ring bs=2)" = "123456" ]
}

test_wr_wrr() {
  echo -n abc > /dev/ring
  [ "$(dd if=/dev/ring bs=1 count=1)" = a ]
  echo -n def > /dev/ring
  [ "$(dd if=/dev/ring bs=3 count=1)" = bcd ]
  [ "$(timeout -k0 .1 dd if=/dev/ring bs=1 count=3 || true)" = ef ]
}

test_overfill() {
  # Assuming buffer size 10
  timeout -k0 .1 dd if=<(echo abc1234567; head -c 100 /dev/urandom) \
                   of=/dev/ring || true
  # Must fail with blocking I/O
  [ "$(timeout -k0 .1 dd if=/dev/ring bs=5)" = abc1234567 ]
}
