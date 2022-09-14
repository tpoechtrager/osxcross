#!/usr/bin/env bash
#
# Build and install the GNU binutils and the GNU Debugger (gdb) for
# target macOS.
#
# You may want to run this script if you want to build software using
# gcc. Please refer to the README.md for details.
#

pushd "${0%/*}" &>/dev/null

DESC=binutils
USESYSTEMCOMPILER=1
source tools/tools.sh

eval $(tools/osxcross_conf.sh)

# binutils version to build
if [ -z "$BINUTILS_VERSION" ]; then
  BINUTILS_VERSION=2.32
fi

# gdb version to build
if [ -z "$GDB_VERSION" ]; then
  GDB_VERSION=8.3
fi

# architecture to target
if [ -z "$TARGET_ARCH" ]; then
  TARGET_ARCH=x86_64
fi

# mirror
MIRROR="https://ftp.gnu.org/gnu"

pushd $BUILD_DIR &>/dev/null

function remove_locks()
{
  rm -rf $BUILD_DIR/have_binutils*
}

function build_and_install()
{
  if [ ! -f "have_$1_$2_${TARGET}_${TARGET_ARCH}" ]; then
    pushd $TARBALL_DIR &>/dev/null
    download "$MIRROR/$1/$1-$2.tar.gz"
    popd &>/dev/null

    echo "cleaning up ..."
    rm -rf $1* 2>/dev/null

    extract "$TARBALL_DIR/$1-$2.tar.gz" 1

    pushd $1*$2* &>/dev/null
    mkdir -p build
    pushd build &>/dev/null

    ../configure \
      --target=$TARGET_ARCH-apple-$TARGET \
      --program-prefix=$TARGET_ARCH-apple-$TARGET- \
      --prefix=$TARGET_DIR/binutils \
      --disable-nls \
      --disable-werror

    $MAKE -j$JOBS
    $MAKE install

    popd &>/dev/null
    popd &>/dev/null
    touch "have_$1_$2_${TARGET}_${TARGET_ARCH}"
  fi
}

source $BASE_DIR/tools/trap_exit.sh

build_and_install binutils $BINUTILS_VERSION
build_and_install gdb $GDB_VERSION

echo ""
echo "installed binutils and gdb to $TARGET_DIR/binutils"
echo ""
