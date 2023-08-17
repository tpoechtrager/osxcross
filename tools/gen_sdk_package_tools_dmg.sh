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

# build xar
build_xar

# build pbzx
PBZX_REVISION="${PBZX_REVISION:-"master"}"
get_sources https://github.com/tpoechtrager/pbzx.git "$PBZX_REVISION"

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
  mkdir -p $TARGET_DIR_SDK_TOOLS/bin
  verbose_cmd $CC -I $TARGET_DIR/include -L $TARGET_DIR/lib pbzx.c \
              -o $TARGET_DIR_SDK_TOOLS/bin/pbzx -llzma -lxar \
              -Wl,-rpath,$TARGET_DIR/lib
  build_success
  popd &>/dev/null
fi

# build 7z
P7ZIP_REVISION="${P7ZIP_REVISION:-"master"}"
get_sources https://github.com/tpoechtrager/p7zip.git "$P7ZIP_REVISION"

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null

  if [ -n "$CC" ] && [ -n "$CXX" ]; then
    [[ $CC == *clang* ]] && CC="$CC -Qunused-arguments"
    [[ $CXX == *clang* ]] && CXX="$CXX -Qunused-arguments"
    $MAKE 7z -j $JOBS CC="$CC" CXX="$CXX -std=gnu++98"
  else
    $MAKE 7z -j $JOBS CXX="c++ -std=gnu++98"
  fi

  $MAKE install DEST_HOME=$TARGET_DIR_SDK_TOOLS
  find $TARGET_DIR_SDK_TOOLS/share -type f -exec chmod 0664 {} \;
  find $TARGET_DIR_SDK_TOOLS/share -type d -exec chmod 0775 {} \;
  popd &>/dev/null
  build_success
fi

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
    verbose_cmd "$TARGET_DIR/SDK/tools/bin/pbzx -n \"$PKG/Payload\" | cpio -i -D $TMP_DIR/out"
done


popd &>/dev/null # TMP_DIR
popd &>/dev/null # BUILD_DIR

echo ""

XCODE_TOOLS_DIR="$TMP_DIR/out/Library/Developer/CommandLineTools" \
  ./tools/gen_sdk_package_tools.sh
