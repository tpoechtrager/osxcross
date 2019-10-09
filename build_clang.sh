#!/usr/bin/env bash
#
# Build and install Clang/LLVM, using `gcc`.
#
# You only need to run this if your distribution does not provide
# clang - or if you want to build your own version from a recent
# source tree.
#

pushd "${0%/*}" &>/dev/null

DESC=clang
USESYSTEMCOMPILER=1

source tools/tools.sh

mkdir -p $BUILD_DIR

source $BASE_DIR/tools/trap_exit.sh

MIRROR="https://releases.llvm.org"

if [ -z "$CLANG_VERSION" ]; then
  CLANG_VERSION=9.0.0
fi

if [ -z "$INSTALLPREFIX" ]; then
  INSTALLPREFIX="/usr/local"
fi

require wget
require cmake

function warn_if_installed()
{
  set +e
  which $1 &>/dev/null && \
  {
    echo ""
    echo "It is highly recommended to uninstall previous $2 versions first:"
    echo "-> $(which $1 2>/dev/null)"
    echo ""
  }
  set -e
}

if [ $PLATFORM != "Darwin" -a $PLATFORM != "FreeBSD" ]; then
  warn_if_installed clang clang
  warn_if_installed llvm-config llvm
fi

echo "Building Clang/LLVM $CLANG_VERSION may take a long time."
echo "Installation Prefix: $INSTALLPREFIX"

if [ -z "$UNATTENDED" ]; then
  echo ""
  read -p "Press enter to start building."
  echo ""
fi

pushd $TARBALL_DIR &>/dev/null

if [ -z "$PKGCOMPRESSOR" ]; then
  PKGCOMPRESSOR="tar.xz"
  [ $CLANG_VERSION == "3.4.2" ] && PKGCOMPRESSOR="tar.gz"
fi

LLVM_PKG="$MIRROR/${CLANG_VERSION}/"
LLVM_PKG+="llvm-${CLANG_VERSION}.src.${PKGCOMPRESSOR}"
 
CLANG_PKG="$MIRROR/${CLANG_VERSION}/"
CLANG_PKG+="cfe-${CLANG_VERSION}.src.${PKGCOMPRESSOR}"

wget -c $LLVM_PKG
wget -c $CLANG_PKG

popd &>/dev/null

pushd $BUILD_DIR &>/dev/null

echo "cleaning up ..."

rm -rf llvm* 2>/dev/null

extract "$TARBALL_DIR/$(basename $LLVM_PKG)"

pushd llvm* &>/dev/null
pushd tools &>/dev/null

extract "$TARBALL_DIR/$(basename $CLANG_PKG)"
echo ""

[ -e clang* ] && mv clang* clang
[ -e cfe* ] && mv cfe* clang

popd &>/dev/null

function build()
{
  stage=$1
  mkdir -p $stage
  pushd $stage &>/dev/null
  cmake .. \
    -DCMAKE_INSTALL_PREFIX=$INSTALLPREFIX \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=1
  $MAKE $2 -j $JOBS VERBOSE=1
  popd &>/dev/null
}

if [ -n "$DISABLE_BOOTSTRAP" ]; then
  build build
else
  build build_stage1 clang

  export CC=$PWD/build_stage1/bin/clang
  export CXX=$PWD/build_stage1/bin/clang++

  if [ -z "$PORTABLE" ]; then
    export CFLAGS="-march=native"
    export CXXFLAGS="-march=native"
  fi

  build build_stage2

  if [ -n "$ENABLE_FULL_BOOTSTRAP" ]; then
    CC=$PWD/build_stage2/bin/clang \
    CXX=$PWD/build_stage2/bin/clang++ \
    build build_stage3
  fi
fi

echo ""
echo "Done!"
echo ""
echo -n "cd into '$PWD/$stage' and type 'make install' to install "
echo "clang/llvm to '$INSTALLPREFIX'"
echo ""

popd &>/dev/null # llvm
popd &>/dev/null
