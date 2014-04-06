#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null

unset LIBRARY_PATH

DESC=gcc
source tools/tools.sh

`tools/osxcross_conf.sh`

# GCC version to build
# (<4.7 will not work properly with libc++)
GCC_VERSION=4.8.2

# GCC mirror
GCC_MIRROR="ftp://ftp.gwdg.de/pub/misc/gcc/releases"

require wget

pushd $OSXCROSS_BUILD_DIR &>/dev/null

function remove_locks()
{
  rm -rf $OSXCROSS_BUILD_DIR/have_gcc*
}

source $BASE_DIR/tools/trap_exit.sh

if [ ! -f "have_gcc_${GCC_VERSION}_${OSXCROSS_TARGET}" ]; then

pushd $OSXCROSS_TARBALL_DIR &>/dev/null
wget -c "$GCC_MIRROR/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.bz2"
popd &>/dev/null

echo "cleaning up ..."
rm -rf gcc* 2>/dev/null

extract "$OSXCROSS_TARBALL_DIR/gcc-$GCC_VERSION.tar.bz2" 1
echo ""

pushd gcc*$GCC_VERSION* &>/dev/null

rm -f $OSXCROSS_TARGET_DIR/bin/*-gcc*
rm -f $OSXCROSS_TARGET_DIR/bin/*-g++*

mkdir -p build
pushd build &>/dev/null

if [[ "`uname -s`" == *BSD ]]; then
  export CPATH="/usr/local/include:/usr/pkg/include:$CPATH"
  export LDFLAGS="-L/usr/local/lib -L/usr/pkg/lib $LDFLAGS"
  export LD_LIBRARY_PATH="/usr/local/lib:/usr/pkg/lib:$LD_LIBRARY_PATH"
fi

../configure \
  --target=x86_64-apple-$OSXCROSS_TARGET \
  --with-ld=$OSXCROSS_TARGET_DIR/bin/x86_64-apple-$OSXCROSS_TARGET-ld \
  --with-as=$OSXCROSS_TARGET_DIR/bin/x86_64-apple-$OSXCROSS_TARGET-as \
  --with-sysroot=$OSXCROSS_SDK \
  --disable-nls \
  --enable-languages=c,c++,objc,obj-c++ \
  --without-headers \
  --enable-multilib \
  --enable-lto \
  --enable-checking=release \
  --prefix=$OSXCROSS_TARGET_DIR

$MAKE -j$JOBS
$MAKE install -j$JOBS

popd &>/dev/null # build
popd &>/dev/null # gcc

touch "have_gcc_${GCC_VERSION}_${OSXCROSS_TARGET}"

fi # have gcc

popd &>/dev/null # build dir

pushd $OSXCROSS_TARGET_DIR/bin &>/dev/null

if [ ! -f i386-apple-$OSXCROSS_TARGET-base-gcc ]; then
  mv x86_64-apple-$OSXCROSS_TARGET-gcc x86_64-apple-$OSXCROSS_TARGET-base-gcc
  mv x86_64-apple-$OSXCROSS_TARGET-g++ x86_64-apple-$OSXCROSS_TARGET-base-g++

  ln -sf x86_64-apple-$OSXCROSS_TARGET-base-gcc i386-apple-$OSXCROSS_TARGET-base-gcc
  ln -sf x86_64-apple-$OSXCROSS_TARGET-base-g++ i386-apple-$OSXCROSS_TARGET-base-g++
fi

echo "compiling wrapper ..."

export TARGET=$OSXCROSS_TARGET
export OSX_VERSION_MIN=$OSXCROSS_OSX_VERSION_MIN
export LINKER_VERSION=$OSXCROSS_LINKER_VERSION

TARGETCOMPILER=gcc \
  $BASE_DIR/wrapper/build.sh 1>/dev/null

popd &>/dev/null # wrapper dir

echo ""

test_compiler o32-gcc $BASE_DIR/oclang/test.c
test_compiler o64-gcc $BASE_DIR/oclang/test.c

test_compiler o32-g++ $BASE_DIR/oclang/test.cpp
test_compiler o64-g++ $BASE_DIR/oclang/test.cpp

echo ""

echo "Done! Now you can use o32-gcc/o32-g++ and o64-gcc/o64-g++ as compiler"
echo ""
echo "Example usage:"
echo ""
echo "Example 1: CC=o32-gcc ./configure --host=i386-apple-$OSXCROSS_TARGET"
echo "Example 2: CC=i386-apple-$OSXCROSS_TARGET-gcc ./configure --host=i386-apple-$OSXCROSS_TARGET"
echo "Example 3: o64-gcc -Wall test.c -o test"
echo "Example 4: x86_64-apple-$OSXCROSS_TARGET-strip -x test"
echo ""
