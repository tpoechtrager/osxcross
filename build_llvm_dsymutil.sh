#!/usr/bin/env bash
#
# Build and install the `llvm-dsymutil` tool required for debugging.
#
# Please refer to README.DEBUGGING.md for details.
#

pushd "${0%/*}" &>/dev/null

DESC="llvm-dsymutil"
source tools/tools.sh
eval $(tools/osxcross_conf.sh)

require git
require cmake

pushd $OSXCROSS_BUILD_DIR &>/dev/null

get_sources https://github.com/tpoechtrager/llvm-dsymutil.git master

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null

  mkdir build
  pushd build &>/dev/null

  $CMAKE .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64" \
    -DLLVM_ENABLE_ASSERTIONS=Off

  $MAKE -f tools/dsymutil/Makefile -j$JOBS
  cp bin/llvm-dsymutil $OSXCROSS_TARGET_DIR/bin/osxcross-llvm-dsymutil
  echo "installed llvm-dsymutil to $OSXCROSS_TARGET_DIR/bin/osxcross-llvm-dsymutil"

  build_success
fi
