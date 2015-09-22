#!/bin/bash

set -o errexit -o nounset -o pipefail -o errtrace -o posix

# directory of this script
DIR="$( cd -P "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# recompile pintool if necessary
(cd $DIR; make -q || make)

time -p pin -xyzzy -enable_vsm 0 -t $DIR/obj-*/cache_sim.so ${@}
