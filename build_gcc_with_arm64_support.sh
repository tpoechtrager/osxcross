#!/usr/bin/env bash

# Build the patched GCC variant with ARM64 support.

pushd "${0%/*}" &>/dev/null
BUILD_ARM64_GCC=1 ./build_gcc.sh "$@"
popd &>/dev/null
