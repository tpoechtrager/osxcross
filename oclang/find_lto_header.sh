#!/usr/bin/env bash

function try()
{
    LLVM_CONFIG="llvm-config$1"
    which $LLVM_CONFIG &>/dev/null

    if [ $? -eq 0 ]; then
        set -e
        LLVM_INC_DIR=`$LLVM_CONFIG --includedir`
        LLVM_LIB_DIR=`$LLVM_CONFIG --libdir`
        ln -sf "$LLVM_INC_DIR/llvm-c/lto.h" "include/llvm-c/lto.h"
        echo -n "export LDFLAGS+=\" -L$LLVM_LIB_DIR -lLTO \" "
        echo -n "export CFLAGS+=\" -DLTO_SUPPORT=1 \" "
        echo -n "export CXXFLAGS+=\" -DLTO_SUPPORT=1 \""
        exit 0
    fi
}

try ""
try "-3.2"
try "-3.3"
try "-3.4"
try "-3.5"

echo "echo \"can not find lto.h - make sure llvm-devel is installed on your system\""
exit 1
