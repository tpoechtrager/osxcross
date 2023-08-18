#!/usr/bin/env bash
#
# Extract required files from a Xcode .dmg using p7zip and run
# gen_sdk_package.sh.
#
# Works up to Xcode 7.2
#
# p7zip will be downloaded and compiled if missing.
#

pushd "${0%/*}/.." &>/dev/null
source tools/tools.sh

if [ $PLATFORM == "Darwin" ]; then
  echo "Use gen_sdk_package.sh on macOS" 1>&2
  exit 1
fi

if [ $# -eq 0 ]; then
  echo "Usage: $0 <xcode.dmg>" 1>&2
  exit 1
fi

XCODEDMG=$(make_absolute_path $1 $(get_exec_dir))

mkdir -p $BUILD_DIR

require git
require $MAKE

[ -n "$CC" ] && require $CC
[ -n "$CXX" ] && require $CXX

pushd $BUILD_DIR &>/dev/null
build_p7zip
popd &>/dev/null

create_tmp_dir

pushd $TMP_DIR &>/dev/null

set +e

$TARGET_DIR_SDK_TOOLS/bin/7z x \
  $XCODEDMG \
  "*/Xcode*.app/Contents/Developer/Platforms/MacOSX.platform" \
  "*/Xcode*.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain"

[ $? -ne 0 -a $? -ne 2 ] && exit 1

if [ -z "$(ls -A)" ]; then
  $TARGET_DIR_SDK_TOOLS/bin/7z x $XCODEDMG "*/Packages/MacOSX*.pkg"
  [ $? -ne 0 -a $? -ne 2 ] && exit 1
fi

[ -z "$(ls -A)" ] && exit 1

set -e

popd &>/dev/null

XCODEDIR="$TMP_DIR/$(ls $TMP_DIR | grep "code" | head -n1)" \
  ./tools/gen_sdk_package.sh
