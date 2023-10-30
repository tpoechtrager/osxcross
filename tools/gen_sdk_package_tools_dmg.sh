#!/usr/bin/env bash
#
# Extract Xcode Command Line Tools .dmg, process "Command Line Tools.pkg"
# and its sub-archives/packages, and run gen_sdk_package_tools.sh.
#
# Tested with XCode Command Line Tools 12.x
#

pushd "${0%/*}/.." &>/dev/null
source tools/tools.sh

require cpio

if [ $PLATFORM == "Darwin" ]; then
  echo "Use gen_sdk_package_tools.sh on macOS" 1>&2
  exit 1
fi

if [ $# -eq 0 ]; then
  echo "Usage: $0 <Command_Line_Tools_for_Xcode.dmg>" 1>&2
  exit 1
fi

XCODE_TOOLS_DMG=$(make_absolute_path "$1" $(get_exec_dir))

mkdir -p $BUILD_DIR
pushd $BUILD_DIR &>/dev/null

require git
require $MAKE
require cpio

[ -n "$CC" ] && require $CC
[ -n "$CXX" ] && require $CXX


build_xar
build_pbxz
build_p7zip

create_tmp_dir

pushd $TMP_DIR &>/dev/null

echo ""
echo "Unpacking $XCODE_TOOLS_DMG ..."
XCODE_TOOLS_PKG="Command Line Developer Tools/Command Line Tools*.pkg"
XCODE_TOOLS_PKG_TMP="Command Line Tools.pkg"
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TARGET_DIR/lib \
  verbose_cmd "$TARGET_DIR_SDK_TOOLS/bin/7z e -so \"$XCODE_TOOLS_DMG\"  \"$XCODE_TOOLS_PKG\" > \"$XCODE_TOOLS_PKG_TMP\""

echo ""
echo "Unpacking $XCODE_TOOLS_PKG_TMP ..."
mkdir "$TMP_DIR/pkg_data"
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TARGET_DIR/lib \
  verbose_cmd "$TARGET_DIR/bin/xar -xf \"$XCODE_TOOLS_PKG_TMP\" -C $TMP_DIR/pkg_data"

echo ""
echo "Processing packages ..."
mkdir "$TMP_DIR/out"
for PKG in $TMP_DIR/pkg_data/*.pkg; do
  LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$TARGET_DIR/lib \
    verbose_cmd "$TARGET_DIR/SDK/tools/bin/pbzx -n \"$PKG/Payload\" | (cd $TMP_DIR/out && cpio -i)"
done


popd &>/dev/null # TMP_DIR
popd &>/dev/null # BUILD_DIR

echo ""

XCODE_TOOLS_DIR="$TMP_DIR/out/Library/Developer/CommandLineTools" \
  ./tools/gen_sdk_package_tools.sh
