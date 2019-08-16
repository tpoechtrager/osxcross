#!/usr/bin/env bash
#
# Mount an Xcode .dmg (using fuse) and run gen_sdk_package.sh.
#
# Works up to Xcode 7.3
#
# This script uses darling-dmg and fuse to mount the .dmg, thus
# avoiding to actually unpack it.
# darling-dmg will be downloaded and compiled if missing.
#

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

XCODEDMG=$(make_absolute_path $1 $(get_exec_dir))

mkdir -p $BUILD_DIR

require modinfo
require fusermount

set +e

command -v lsb_release 2>&1 > /dev/null

if [[ $? -eq 0 ]] && [[ -n $(lsb_release -a 2>&1 | grep -i ubuntu) ]]; then
  echo "Using ubuntu, skipping fuse module check"
else
  modinfo fuse &>/dev/null
fi

if [ $? -ne 0 ]; then
  echo "Required kernel module 'fuse' not loaded" 1>&2
  echo "Please run 'insmod fuse' as root" 1>&2
  exit 1
fi

set -e

pushd $BUILD_DIR &>/dev/null

FULL_CLONE=1 \
  get_sources https://github.com/LubosD/darling-dmg.git master

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
  git reset --hard 5f64bc9a3795e0a1c307e9beb099f9035fdd864f
  mkdir -p build
  pushd build &>/dev/null
  $CMAKE .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$TARGET_DIR_SDK_TOOLS
  $MAKE -j $JOBS install
  popd &>/dev/null
  popd &>/dev/null
  build_success
fi

popd &>/dev/null # build dir

TMP=$(mktemp -d /tmp/XXXXXXXXX)

function cleanup()
{
  fusermount -u $TMP || true
  rm -rf $TMP
}

trap cleanup EXIT

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TARGET_DIR_SDK_TOOLS/lib \
  $TARGET_DIR/SDK/tools/bin/darling-dmg $XCODEDMG $TMP

XCODEDIR=$TMP ./tools/gen_sdk_package.sh
