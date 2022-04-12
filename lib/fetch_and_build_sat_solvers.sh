#!/bin/bash

set -e

# fetching is done via git submodules by now!
# 
# git submodule update --init --recursive


if [ ! -f mergesat/libmergesat.a ]; then
    # Make MergeSat
    cd mergesat
    make all -j $(nproc)
    cp build/release/lib/libmergesat.a .
    cd ..
else
    echo "Assuming that a correct installation of MergeSat is present."
fi

if [ ! -f yalsat/libyals.a ]; then
    echo "Building YalSAT ..."
    
    cd yalsat
    ./configure.sh
    make
    cd ..
else
    echo "Assuming that a correct installation of YalSAT is present."
fi

if [ ! -f lingeling/liblgl.a ]; then
    
    echo "Building lingeling ..."

    cd lingeling
    ./configure.sh
    make
    cd ..
else
    echo "Assuming that a correct installation of lingeling is present."
fi

if [ ! -f cadical/libcadical.a ]; then
    echo "Building CaDiCaL ..."

    cd cadical
    ./configure
    make
    cp build/libcadical.a .
    cd ..

else
    echo "Assuming that a correct installation of cadical is present."
fi
