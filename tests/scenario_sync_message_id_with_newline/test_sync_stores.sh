#!/bin/bash

script_args=("$@")
# from where this script is ...
source "$( dirname "$( realpath $0 )" )"/../lib/lib.sh

pre_run

# sync store_a and store_b

# this should *NOT* sync anything since the two message IDs,
# one folded over two lines and the other not, should be
# considered identical
#
run_mailsync channel_store_a_store_b

post_run
