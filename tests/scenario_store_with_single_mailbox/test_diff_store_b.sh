#!/bin/bash

script_args=("$@")
# from where this script is ...
source "$( dirname "$( realpath $0 )" )"/../lib/lib.sh

pre_run

# sync store_a and store_b
run_mailsync -vw -vp -m channel_store_a_store_b store_b

post_run
