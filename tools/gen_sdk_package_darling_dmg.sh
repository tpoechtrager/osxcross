#!/usr/bin/env bash

pushd "${0%/*}/.." &>/dev/null
source tools/tools.sh

if [ $PLATFORM == "Darwin" ]; then
  echo "Use gen_sdk_package.sh on Mac OS X" 1>&2
  exit 1
fi

if [ $# -eq 0 ]; then
  echo "Usage: $0 <xcode.dmg>" 1>&2
  exit 1
fi

mkdir -p $BUILD_DIR

require git
require cmake
require modinfo
require fusermount

set +e

modinfo fuse &>/dev/null

if [ $? -ne 0 ]; then
  echo "required kernel module 'fuse' not loaded" 1>&2
  echo "please run 'insmod fuse' as root" 1>&2
  exit 1
fi

set -e

pushd $BUILD_DIR &>/dev/null

if [ ! -f $TARGET_DIR/bin/darling-dmg ]; then
  rm -f have_darling_dmg
fi

if [ ! -f "have_darling_dmg" ]; then

rm -rf darling-dmg*
git clone https://github.com/LubosD/darling-dmg.git
pushd darling-dmg &>/dev/null
mkdir -p build
pushd build &>/dev/null
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$TARGET_DIR
make -j $JOBS install
popd &>/dev/null
popd &>/dev/null

touch "have_darling_dmg"

fi

popd &>/dev/null # build dir

TMP=$(mktemp -d /tmp/XXXXXXXXX)

function cleanup() {
  fusermount -u $TMP || true
  rm -rf $TMP
}

trap cleanup EXIT

$TARGET_DIR/bin/darling-dmg $1 $TMP
XCODEDIR=$TMP ./tools/gen_sdk_package.sh
