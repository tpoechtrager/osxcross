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

if [ -z "$CLANG_VERSION" ]; then
  CLANG_VERSION=10.0.1
fi

if [ -z "$INSTALLPREFIX" ]; then
  INSTALLPREFIX="/usr/local"
fi

require cmake

LLVM_PKG=""
CLANG_PKG=""

function set_package_link()
{
  pushd $BUILD_DIR &>/dev/null

  DOWNLOAD_PAGE=llvmorg-$CLANG_VERSION
  download https://api.github.com/repos/llvm/llvm-project/releases/tags/$DOWNLOAD_PAGE &> /dev/null

  if [[ $(file $DOWNLOAD_PAGE) == *gzip* ]]; then
    mv $DOWNLOAD_PAGE $DOWNLOAD_PAGE.gz
    require gzip
    gzip -d $DOWNLOAD_PAGE.gz
  fi
  links=$(cat $DOWNLOAD_PAGE | grep 'browser_download_url')
  rm -f $DOWNLOAD_PAGE
  LLVM_PKG=$(echo "$links" | grep "llvm-$CLANG_VERSION.src" | head -n 1 || true)
  CLANG_PKG=$(echo "$links" | grep -E "(clang|cfe)-$CLANG_VERSION.src" | head -n 1 || true)
  if [ -n "$LLVM_PKG" ] && [[ $LLVM_PKG != https* ]]; then
      LLVM_PKG=$(echo $LLVM_PKG | cut -d\: -f 2- | tr -d \")
      CLANG_PKG=$(echo $CLANG_PKG | cut -d\: -f 2- | tr -d \")
  fi
  popd &>/dev/null
}

set_package_link

if [ -z "$LLVM_PKG" ] || [ -z "$CLANG_PKG" ]; then
  echo "Release $CLANG_VERSION not found!" 1>&2
  exit 1
fi


function warn_if_installed()
{
  set +e
  command -v $1 &>/dev/null && \
  {
    echo ""
    echo "It is highly recommended to uninstall previous $2 versions first:"
    echo "-> $(command -v $1 2>/dev/null)"
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

download $LLVM_PKG
download $CLANG_PKG

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

if [ -z "$ENABLE_CLANG_INSTALL" ]; then
  echo ""
  echo "Done!"
  echo ""
  echo -n "cd into '$PWD/$stage' and type 'make install' to install "
  echo "clang/llvm to '$INSTALLPREFIX'"
  echo ""
else
  pushd $stage &>/dev/null
  $MAKE install -j $JOBS VERBOSE=1
  popd &>/dev/null
  echo ""
  echo "Done!"
  echo ""
fi

popd &>/dev/null # llvm
popd &>/dev/null
