#!/bin/bash

# from where this script is ...
source "$( dirname "$( realpath $0 )" )"/../lib/lib.sh

pre_run

# list contents of store_a
run_mailsync store_a

post_run
