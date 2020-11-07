#!/bin/bash

help() {
  echo "usage: $0 [--gdb|--strace] [--debug] [--keep]"
  echo "       $0 [--help]"
  echo
  echo "    run this test"
  echo
  echo "    --debug    - run mailsync in debug mode"
  echo "    --strace   - strace mailsync"
  echo "    --gdb      - run mailsync in gdb"
  echo "    --keep     - keep the test environmane after the test"
  echo
  echo "    --gdb and --strace are mutually exclusive"
  echo
  exit 1
}

set -e          # stop on error
set -o pipefail # stop part of pipeline failing
set -u          # stop on undefined variable

DEBUG=
STRACE=
GDB=
KEEP=
NO_CHECK=

# iterate as long as there are params (due to set -u)
while ARGV=${1:-} && [ "$ARGV" != "" ]; do
  case "$1" in
    --debug)  DEBUG=-d            ; NO_CHECK=true; shift ;;
    --strace) STRACE="strace -ff" ; NO_CHECK=true; shift ;;
    --gdb)    GDB="gdb --args"    ; NO_CHECK=true; shift ;;
    --keep)   KEEP="yes"          ;                shift ;;
    --help)   help                ;;
    *)        help                ;;
  esac
done

[ "$STRACE" != "" -a "$GDB" != "" ] && echo "ERROR: cant't use both --strace and --gdb" && help


############################
# set up state
############################

# execute this test from the directory of the script
FULLPATH_OF_THIS_SCRIPT=`realpath "$0"`
DIR_OF_THIS_SCRIPT=`dirname "$FULLPATH_OF_THIS_SCRIPT"`
cd "$DIR_OF_THIS_SCRIPT"

TMP_DIR=$( mktemp -d )

echo "Creating test environment in '$TMP_DIR'"
echo

HOME_DURING_TEST="$TMP_DIR"

blackbox_dir="$HOME_DURING_TEST/mail"
cp -a mail.orig "$blackbox_dir"

mailsync_conf="$TMP_DIR/mailsync.conf"
cp -a mailsync.conf "$mailsync_conf"

# current working directory is tests/1, therefore:
mailsync=../../src/mailsync


############################
# run mailsync
############################

# list contents of store_a

# $GDB and $STRACE can not be set at the same time!
HOME="$HOME_DURING_TEST" $GDB $STRACE $mailsync $DEBUG -f "$mailsync_conf" store_a | tee "$TMP_DIR/output"


############################
# verify everything is right
############################

echo

if [ "$NO_CHECK" == "true" ]; then
  echo "Not verifying test results because of given command line switch"

else

  if diff -u reference_output/test_list_store.sh.output "$TMP_DIR/output"; then
    echo "Test successful"
  else
    echo "Test failed"
  fi
fi


############################
# clean up
############################
if [ "$KEEP" != "yes" ]; then
  rm -r "$TMP_DIR"
fi

