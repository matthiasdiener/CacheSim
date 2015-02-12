#!/bin/bash

set -o errexit -o nounset -o pipefail -o errtrace -o posix


pin -xyzzy -enable_vsm 0 -t obj-*/cache_sim.so -- ${@}
