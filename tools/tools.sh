#!/usr/bin/env bash

BASE_DIR=`pwd`

export LC_ALL="C"
export CC=clang
export CXX=clang++

# enable debug messages
test -n "$OCDEBUG" && set -x

# how many concurrent jobs should be used for compiling?
JOBS=`tools/get_cpu_count.sh`

if [ "`basename $0`" != "build.sh" ]; then
    `tools/osxcross_conf.sh`

    if [ $? -ne 0 ]; then
        echo "you need to complete ./build.sh first, before you can start building $DESC"
        exit 1
    fi
fi

function require
{
    which $1 &>/dev/null
    while [ $? -ne 0 ]
    do
        echo ""
        read -p "Please install $1 then press enter"
        which $1 &>/dev/null
    done
}

function test_compiler
{
    echo -ne "testing $1 ... "
    $1 $2 -O2 -Wall -o test
    rm test
    echo "works"
}

# exit on error
set -e
