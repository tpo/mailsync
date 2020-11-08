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

RED='\033[0;31m'
GRN='\033[0;32m'
NC='\033[0m' # No Color

echo

for scenario in scenario_* ; do
  echo "*************************** Running tests in scenario '$scenario'"
  for tst in "$scenario"/test_*.sh; do
    echo "================= Running tests '$tst'"

    if [ "$QUIET" == "true" ]; then
      $tst > /dev/null
    else
      $tst
    fi

    if [ "$?" == "0" ]; then
      printf "================= ${GRN}Success${NC}: '$tst'\n"
    else
      printf "================= ${RED}Failed${NC}: '$tst'\n"
    fi
  done
done
