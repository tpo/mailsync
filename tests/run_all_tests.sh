#!/bin/bash

help() {
  echo "usage: run_all_tests.sh [--quiet] [--help]"
  echo
  echo "    run all test_*.sh tests found in scenario_* directories"
  echo
  echo "    --help  - show this help"
  echo "    --quiet - don't show test output, only whether it succeede"
  echo
  exit 1
}

[ "$1" == "--help"  ] && help
[ "$1" == "--quiet" ] && QUIET=true

echo

for scenario in scenario_* ; do
  echo "*************************** Running tests in scenario '$scenario'"
  for tst in "$scenario/test_*.sh"; do
    echo "================= Running tests '$tst'"

    if [ "$QUIET" == "true" ]; then
      $tst > /dev/null
    else
      $tst
    fi

    if [ "$?" == "0" ]; then
      echo "================= Success: '$tst'"
    else
      echo "================= Failed: '$tst'"
    fi
  done
done
