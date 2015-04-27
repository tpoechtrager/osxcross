#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null

DESC=clang
USESYSTEMCOMPILER=1

source tools/tools.sh

TARBALL_DIR=$BASE_DIR/tarballs
BUILD_DIR=$BASE_DIR/build

if [ -z "$SKIP_GCC_CHECK" ]; then
if [ $PLATFORM != "Darwin" -a $PLATFORM != "FreeBSD" ]; then
  set +e
  which "g++${GCC_SUFFIX}" &>/dev/null && \
  {
    export CC="gcc${GCC_SUFFIX}"
    export CXX="g++${GCC_SUFFIX}"
    test="
    #define GCC_VERSION_AT_LEAST(major, minor, patch)                    \
      (defined(__GNUC__) &&                                              \
      (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= \
      (major * 10000 + minor * 100 + patch))

    #if !GCC_VERSION_AT_LEAST(4, 7, 0)
      not_gcc_47_or_later
    #endif"
    echo "$test" | $CXX -fsyntax-only -xc++ - &>/dev/null || \
    {
      echo "Your GCC installation is too old to build recent clang releases."
      echo "Building clang 3.4.2 instead."
      CLANG_VERSION=3.4
      CLANG_VERSION_PATCH=.2
    }
  } || \
  {
    echo "Can not detect GCC installation." 1>&2
    echo "You may want to try 'GCC_SUFFIX=<suffix> $0'" 1>&2
    echo "(i.e. GCC_SUFFIX=-4.7 $0)" 1>&2
    exit 1
  }
  set -e
fi
fi

source $BASE_DIR/tools/trap_exit.sh

MIRROR="http://llvm.org"
#MIRROR="http://archive.ubuntu.com"

if [ -z "$CLANG_VERSION" ]; then
  CLANG_VERSION=3.6
  CLANG_VERSION_PATCH=.0
fi

if [[ $MIRROR == *ubuntu* ]] && [ $CLANG_VERSION_PATCH == .0 ]; then
  CLANG_VERSION_PATCH=
fi

CLANG_VERSION_MMP="${CLANG_VERSION}${CLANG_VERSION_PATCH}"

if [ -z "$INSTALLPREFIX" ]; then
  INSTALLPREFIX="/usr/local"
fi

require wget

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
read -p "Press enter to start building."
echo ""

pushd $TARBALL_DIR &>/dev/null

if [[ $MIRROR == *ubuntu* ]]; then

  LLVM_PKG="$MIRROR/ubuntu/pool/main/l/llvm-toolchain-${CLANG_VERSION}/"
  LLVM_PKG+="llvm-toolchain-${CLANG_VERSION}_${CLANG_VERSION_MMP}"
  LLVM_PKG+=".orig.tar.bz2"

  CLANG_PKG="$MIRROR/ubuntu/pool/main/l/llvm-toolchain-${CLANG_VERSION}/"
  CLANG_PKG+="llvm-toolchain-${CLANG_VERSION}_${CLANG_VERSION_MMP}"
  CLANG_PKG+=".orig-clang.tar.bz2"

else

  if [ -z "$PKGCOMPRESSOR" ]; then
    PKGCOMPRESSOR="tar.xz"
    [ $CLANG_VERSION == "3.4" ] && PKGCOMPRESSOR="tar.gz"
  fi

  LLVM_PKG="$MIRROR/releases/${CLANG_VERSION_MMP}/"
  LLVM_PKG+="llvm-${CLANG_VERSION_MMP}.src.${PKGCOMPRESSOR}"
 
  CLANG_PKG="$MIRROR/releases/${CLANG_VERSION_MMP}/"
  CLANG_PKG+="cfe-${CLANG_VERSION_MMP}.src.${PKGCOMPRESSOR}"

fi

wget -c $LLVM_PKG
wget -c $CLANG_PKG

popd &>/dev/null

pushd $BUILD_DIR &>/dev/null

echo "cleaning up ..."

rm -rf llvm* 2>/dev/null

extract "$TARBALL_DIR/$(basename $LLVM_PKG)" 2 0

pushd llvm* &>/dev/null
pushd tools &>/dev/null

extract "$TARBALL_DIR/$(basename $CLANG_PKG)" 1
[ -e clang* ] && mv clang* clang
[ -e cfe* ] && mv cfe* clang

popd &>/dev/null

function build()
{
  stage=$1
  mkdir -p $stage
  pushd $stage &>/dev/null
  ../configure --prefix=$INSTALLPREFIX --enable-optimized --disable-assertions
  $MAKE $2 -j $JOBS VERBOSE=1
  popd &>/dev/null
}

if [ -n "$DISABLE_BOOTSTRAP" ]; then
  build build
else
  build build_stage1 clang-only

  export CC=$PWD/build_stage1/Release/bin/clang
  export CXX=$PWD/build_stage1/Release/bin/clang++

  if [ -z "$PORTABLE" ]; then
    export CFLAGS="-march=native"
    export CXXFLAGS="-march=native"
  fi

  build build_stage2

  if [ -n "$ENABLE_FULL_BOOTSTRAP" ]; then
    CC=$PWD/build_stage2/Release/bin/clang \
    CXX=$PWD/build_stage2/Release/bin/clang++ \
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
