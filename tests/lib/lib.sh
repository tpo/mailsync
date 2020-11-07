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
  
  for arg in "${script_args[@]}"; do
    case "$arg" in
      --debug)  DEBUG=-d            ; NO_CHECK=true ;;
      --strace) STRACE="strace -ff" ; NO_CHECK=true ;;
      --gdb)    GDB="gdb --args"    ; NO_CHECK=true ;;
      --keep)   KEEP="keep"                         ;;
      --help)   help                                ;;
      *)        help                                ;;
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
  mkdir "$blackbox_dir"
  cp -a dir_with_mails_a "$blackbox_dir"/
  cp -a dir_with_mails_b "$blackbox_dir"/
  
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

verify_output_matches() {
  local test_name="$( basename "$0" )"

  verify_output_matches_this "${test_name}.output"
}

# usage: verify_output_matches_this NAME_OF_FILE
#
# FILE has to be inside `reference_output/`
#
verify_output_matches_this() {
  local output_file="$1"
  echo
  
  if [ "$NO_CHECK" == "true" ]; then
    echo "Not verifying test results because of given command line switch"
  
  else
  
    if diff -u "reference_output/$output_file" "$TMP_DIR/output"; then
      echo "Test successful"
    else
      echo "Test failed"
    fi
  fi
}

# cleanup everything that the test generated
#
# usage: clean_up
#
clean_up() {
  if [ "$KEEP" != "keep" ]; then
    rm -r "$TMP_DIR"
  fi
}

# do before running mailsync
#
pre_run() {
  set -e          # stop on error
  set -o pipefail # stop part of pipeline failing
  set -u          # stop on undefined variable
  
  # cd into where this script is
  cd "$( dirname "$( realpath $0 )" )"

  parse_args "$@"
  set_up_test_state
}

# do after running mailsync
#
post_run() {
  verify_output_matches
  clean_up
}
