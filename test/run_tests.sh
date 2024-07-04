#!/bin/sh


# Exported so that they can be used by test functions
export CRED=$'\033[0;31m'
export CGREEN=$'\033[0;32m'
export CPURPLE=$'\033[0;35m'
export CNONE=$'\033[0m'


function main() {
  if ! (lsmod | grep '^ring_chrdev ' >/dev/null) || ! [ -c /dev/ring ]; then
    echo Make sure you have loaded the module with '`insmod`' \
      and created /dev/ring with '`mknod`' and made the current user its owner >&2
    exit 1
  fi

  if ! [ -O /dev/ring ] && [ "$(id -u)" != 0 ]; then
    echo /dev/ring must be owned by the current user for the noatime test \
      to complete. Use '`chown`' >&2
    exit 1
  fi


  cd "$(dirname "$0")"

  failed=0

  for test_file in ./test_*.sh; do
    source "$test_file" || exit $?
    TEST_FUNCS=$(compgen -A function | grep '^test_')

    for test_func in $TEST_FUNCS; do
      echo TEST "$CPURPLE$test_func$CNONE":

      (set -e; "$test_func")
      ret=$?
      if [ "$ret" -eq 0 ]; then
        echo TEST "$CPURPLE$test_func$CNONE": ${CGREEN}PASSED$CNONE!
      else
        echo TEST "$CPURPLE$test_func$CNONE": ${CRED}FAILED$CNONE! "($ret)"
        failed=1
      fi

      echo -ne '\n\n'
    done

    unset -f $(compgen -A function | grep '^test_')
  done

  exit "$failed"
}


main
