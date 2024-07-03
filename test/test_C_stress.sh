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
  for((i=0;i<220;++i)); do
    # Using `$i % 11` to get lines of different lengths
    (sleep 0.1 && echo $(($i % 11)) >/dev/ring) &
  done

  output=$(! timeout -k0 .5 cat /dev/ring)
  [ "$(echo "$output" | grep '^0$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^1$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^2$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^3$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^4$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^5$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^6$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^7$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^8$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^9$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | grep '^10$' | wc -l | tee /dev/stderr )" = 20 ]
  [ "$(echo "$output" | wc -l | tee /dev/stderr)" = 220 ]
}
