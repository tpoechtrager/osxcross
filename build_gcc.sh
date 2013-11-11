#!/usr/bin/env bash

export LC_ALL="C"
export LIBRARY_PATH=""

which osxcross-conf || {
    echo "you need to complete ./build.sh first, before you can start building gcc"
    exit 1
}

eval `osxcross-conf`

set -e

# How many concurrent jobs should be used for compiling?
JOBS=`grep -c ^processor /proc/cpuinfo`

# GCC version to build
GCC_VERSION=4.8.2

# GCC mirror
GCC_MIRROR="ftp://ftp.gwdg.de/pub/misc/gcc/releases"

function require
{
    which $1 &>/dev/null
    while [ $? -ne 0 ]
    do
        echo ""
        read -p "Install $1 then press enter"
        which $1 &>/dev/null
    done
}

require wget

BASE_DIR=`pwd`

pushd $OSXCROSS_BUILD_DIR

if [ ! -f "have_gcc_${GCC_VERSION}" ]; then

pushd $OSXCROSS_TARBALL_DIR
wget -c "$GCC_MIRROR/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.bz2"
popd

rm -rf gcc* 2>/dev/null

echo "extracting gcc ..."
tar xf "$OSXCROSS_TARBALL_DIR/gcc-$GCC_VERSION.tar.bz2"

pushd gcc*$GCC_VERSION*

patch -p0 < $OSXCROSS_PATCH_DIR/gcc-dsymutil.patch

mkdir -p build
pushd build

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
    --prefix=$OSXCROSS_TARGET_DIR

make -j$JOBS
make install -j$JOBS

popd #build
popd #gcc

touch "have_gcc_${GCC_VERSION}"

fi #have gcc

popd #build dir

cp ogcc/ogcc $OSXCROSS_TARGET_DIR/bin

pushd $OSXCROSS_TARGET_DIR/bin

if [ ! -f $OSXCROSS_TARGET_DIR/bin/i386-apple-$OSXCROSS_TARGET-base-gcc ]; then
    mv x86_64-apple-$OSXCROSS_TARGET-gcc x86_64-apple-$OSXCROSS_TARGET-base-gcc
    mv x86_64-apple-$OSXCROSS_TARGET-g++ x86_64-apple-$OSXCROSS_TARGET-base-g++

    ln -sf x86_64-apple-$OSXCROSS_TARGET-base-gcc i386-apple-$OSXCROSS_TARGET-base-gcc
    ln -sf x86_64-apple-$OSXCROSS_TARGET-base-g++ i386-apple-$OSXCROSS_TARGET-base-g++
fi

ln -sf ogcc o32-gcc
ln -sf ogcc o32-g++
ln -sf ogcc o64-gcc
ln -sf ogcc o64-g++

ln -sf ogcc i386-apple-$OSXCROSS_TARGET-gcc
ln -sf ogcc i386-apple-$OSXCROSS_TARGET-g++
ln -sf ogcc x86_64-apple-$OSXCROSS_TARGET-gcc
ln -sf ogcc x86_64-apple-$OSXCROSS_TARGET-g++

popd #target dir / bin

function test_compiler
{
    echo -ne "testing $1 ... "
    $1 $2 -O2 -Wall -o test && rm test
    echo "works"
}

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
