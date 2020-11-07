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

# usage: parse_args "$@"
#
#  will set these global variables:
#
#  * DEBUG
#  * STRACE
#  * GDB
#  * KEEP
#  * NO_CHECK
#
parse_args() {
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
      --keep)   KEEP="keep"         ;                shift ;;
      --help)   help                ;;
      *)        help                ;;
    esac
  done
  
  [ "$STRACE" != "" -a "$GDB" != "" ] && echo "ERROR: cant't use both --strace and --gdb" && help || true
}

# usage: set_up_test_state
#
# will set the following global variables:
#
#  TMP_DIR
#  HOME_DURING_TEST
#  blackbox_dir
#  mailsync_conf
#  mailsync
#
set_up_test_state() {

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
}

# usage: run_mailsync "$@"
#
# "$@" contains additional arguments for mailsync
#
run_mailsync() {
  HOME="$HOME_DURING_TEST" $GDB $STRACE $mailsync $DEBUG -f "$mailsync_conf" "$@" | tee "$TMP_DIR/output"
}

# cleanup everything that the test generated
#
# usage: cleanup "$KEEP" "$TMP_DIR
#
clean_up() {
  local KEEP="$1"
  local TMP_DIR="$2"

  if [ "$KEEP" != "keep" ]; then
    rm -r "$TMP_DIR"
  fi
}

