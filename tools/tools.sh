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

function require()
{
    set +e
    which $1 &>/dev/null
    while [ $? -ne 0 ]
    do
        echo ""
        read -p "Please install $1 then press enter"
        which $1 &>/dev/null
    done
    set -e
}

if [[ "`uname -s`" == *BSD ]]; then
    MAKE=gmake
else
    MAKE=make
fi

require $MAKE

function extract()
{
    test $# -ge 2 -a $# -lt 4 && test $2 -eq 2 && echo ""
    echo "extracting `basename $1` ..."

    local tarflags

    tarflags="xf"
    test -n "$OCDEBUG" && tarflags+="v"

    case $1 in
        *.pkg)
            which xar &>/dev/null || exit 1
            xar -xf $1
            cat Payload | gunzip -dc | cpio -i 2>/dev/null && rm Payload
            ;;
        *.tar.xz)
            xz -dc $1 | tar $tarflags -
            ;;
        *.tar.gz)
            gunzip -dc $1 | tar $tarflags -
            ;;
        *.tar.bz2)
            bzip2 -dc $1 | tar $tarflags -
            ;;
        *)
            echo "Unhandled archive type"
            exit 1
            ;;
    esac

    if [ $# -eq 2 -o $# -eq 4 ]; then
        echo ""
    fi
}

function test_compiler()
{
    echo -ne "testing $1 ... "
    $1 $2 -O2 -Wall -o test
    rm test
    echo "works"
}

# exit on error
set -e
