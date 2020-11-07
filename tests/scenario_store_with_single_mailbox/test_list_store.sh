#!/bin/bash

set -e          # stop on error
set -o pipefail # stop part of pipeline failing
set -u          # stop on undefined variable

# cd into where this script is
cd "$( dirname "$( realpath $0 )" )"

# list contents of store_a
run_mailsync store_a

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

clean_up "$KEEP" "$TMP_DIR"
