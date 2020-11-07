#!/bin/bash

script_args=("$@")
# from where this script is ...
source "$( dirname "$( realpath $0 )" )"/../lib/lib.sh

pre_run

# sync store_a and store_b
run_mailsync channel_store_a_store_b
verify_output_matches_this test_sync_stores.sh.first_sync.output

run_mailsync channel_store_a_store_b
verify_output_matches_this test_sync_stores.sh.second_sync.output

clean_up
