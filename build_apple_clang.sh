#!/usr/bin/env bash

# Builds the Apple version of Clang/LLVM

pushd "${0%/*}" &>/dev/null

if [ -z "$CLANG_VERSION" ]; then
  CLANG_VERSION=17
fi

GITPROJECT=apple CLANG_VERSION=$CLANG_VERSION \
  ./build_clang.sh
