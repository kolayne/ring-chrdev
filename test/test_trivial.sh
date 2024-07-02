#!/bin/sh

test_read_empty() {
  dd if=/dev/ring of=/dev/full count=1
}
