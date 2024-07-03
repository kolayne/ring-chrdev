#!/bin/sh

function test_reader_sequential_writers() {
  (
    echo -n 0 | dd if=/dev/stdin of=/dev/ring oflag=nonblock
    echo -n 1 > /dev/ring
    sleep 0.1
    echo -n 23 > /dev/ring
    sleep 0.2
    echo -n 1234567890abc > /dev/ring
  ) &

  # bs=1 because `dd` may attempt to collect input into too large blocks
  [ "$(timeout -k0 1 dd if=/dev/ring bs=1)" = "01231234567890abc" ]
  wait %1
}

function test_reader_parallel_writers() {
  for((i=0;i<100;++i)); do
    (sleep 0.1 && echo 1 >/dev/ring) &
  done

  output=$(! timeout -k0 .5 cat /dev/ring)
  ! echo "$output" | grep -v '^1$'
  [ "$(echo "$output" | wc -c)" = 200 ]
}
