#!/usr/bin/env bash

pushd "${0%/*}/.." &>/dev/null
source tools/tools.sh

require cpio

if [ $PLATFORM == "Darwin" ]; then
  echo "Use gen_sdk_package.sh on Mac OS X" 1>&2
  exit 1
fi

if [ $# -eq 0 ]; then
  echo "Usage: $0 <xcode.xip>" 1>&2
  exit 1
fi

XCODE=$(make_absolute_path $1 $(get_exec_dir))

mkdir -p $BUILD_DIR
pushd $BUILD_DIR &>/dev/null

build_xar

get_sources https://github.com/tpoechtrager/pbzx.git master

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
  mkdir -p $TARGET_DIR_SDK_TOOLS/bin
  verbose_cmd $CC -I $TARGET_DIR/include -L $TARGET_DIR/lib pbzx.c \
              -o $TARGET_DIR_SDK_TOOLS/bin/pbzx -llzma -lxar \
              -Wl,-rpath,$TARGET_DIR/lib
  build_success
  popd &>/dev/null
fi

create_tmp_dir

pushd $TMP_DIR &>/dev/null

echo "Extracting $XCODE (this may take several minutes) ..."

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TARGET_DIR/lib \
  verbose_cmd "$TARGET_DIR/bin/xar -xf $XCODE -C $TMP_DIR"

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TARGET_DIR/lib \
  verbose_cmd "$TARGET_DIR/SDK/tools/bin/pbzx -n Content | cpio -i"

popd &>/dev/null # TMP_DIR
popd &>/dev/null # BUILD_DIR

echo ""

XCODEDIR=$TMP_DIR \
  ./tools/gen_sdk_package.sh
