#!/bin/sh

function test_CA_reader_sequential_writers() {
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

function test_CB_reader_parallel_writers() {
  # Kind is set to 10 such that the length of each print (including the newline)
  # is of the same length that is a multiple of the ring buffer size (assuming 10).
  #
  # If that's not the case, the `write` system call may complete partially, leading
  # to the userspace repeating the call. So, eventually all the data will get there
  # but, if the ring is under high load, the content may be broken apart by other
  # writes.
  KIND=10
  COUNT=200
  TOTAL=$(("$KIND" * "$COUNT"))

  for((i=0;i<"$TOTAL";++i)); do
    (echo $(($i % $KIND)) >/dev/ring) &
  done

  output=$(timeout -k0 1 cat /dev/ring || true)
  for((i=0;i<"$KIND";++i)); do
    [ "$(echo "$output" | grep "^$i\$" | wc -l | tee /dev/stderr )" = "$COUNT" ]
  done
  [ "$(echo "$output" | wc -l | tee /dev/stderr)" = "$TOTAL" ]
}
