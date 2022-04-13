#!/bin/bash

set -e

# Get latest state of submodule solvers
git submodule update --init --recursive

# Actually build the external solvers
cd lib
./fetch_and_build_sat_solvers.sh
cd ..

# Build mallob
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE ..
VERBOSE=1 make -j $(nproc)
cd ..
