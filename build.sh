#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null

export LC_ALL="C"
export CC=clang
export CXX=clang++

# How many concurrent jobs should be used for compiling?
JOBS=`grep -c ^processor /proc/cpuinfo`

# SDK version to use
SDK_VERSION=10.8

# Minimum targeted OS X version
# Must be <= SDK_VERSION
# You can comment this variable out,
# if you want to use the compilers default value
OSX_VERSION_MIN=10.5

# ld version
LINKER_VERSION=134.9

# Don't change this
OSXCROSS_VERSION=0.5

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

BASE_DIR=`pwd`
TARBALL_DIR=$BASE_DIR/tarballs
BUILD_DIR=$BASE_DIR/build
TARGET_DIR=$BASE_DIR/target
PATCH_DIR=$BASE_DIR/patches
SDK_DIR=$TARGET_DIR/SDK

JOBSSTR="jobs"
if [ $JOBS -eq 1 ]; then
    JOBSSTR="job"
fi

if [ "$OSX_VERSION_MIN" == "" ]; then
    OSX_VERSION_MIN="default"
fi

case $SDK_VERSION in
    10.4*) TARGET=darwin8 ;;
    10.5*) TARGET=darwin9 ;;
    10.6*) TARGET=darwin10 ;;
    10.7*) TARGET=darwin11 ;;
    10.8*) TARGET=darwin12 ;;
    10.9*) TARGET=darwin13 ;;
    *) echo "Invalid SDK Version"; exit 1 ;;
esac

set -e

echo ""
echo "Building OSXCross toolchain, Version: $OSXCROSS_VERSION"
echo ""
echo "OS X SDK Version: $SDK_VERSION, Target: $TARGET"
echo "Minimum targeted OS X Version: $OSX_VERSION_MIN"
echo "Tarball Directory: $TARBALL_DIR"
echo "Build Directory: $BUILD_DIR"
echo "Install Directory: $TARGET_DIR"
echo "SDK Install Directory: $SDK_DIR"
echo "Compile with $JOBS concurrent $JOBSSTR"
echo ""
read -p "Press enter to start building"
echo ""

export PATH=$TARGET_DIR/bin:$PATH

mkdir -p $BUILD_DIR
mkdir -p $TARGET_DIR
mkdir -p $SDK_DIR

set +e
require $CC
require $CXX
require clang
require make
require sed
require patch
require gunzip
require cpio
require autogen
require automake
require libtool
set -e

CLANG_TARGET_OPTION=`./oclang/check_clang_target_option.sh`

pushd $BUILD_DIR

if [ ! -f "have_cctools_$TARGET" ]; then

tar xzfv $TARBALL_DIR/cctools-*.tar.gz
tar xzfv $TARBALL_DIR/xar*.tar.gz

pushd cctools*
patch -p0 < $PATCH_DIR/cctools-ld64-1.patch
patch -p0 < $PATCH_DIR/cctools-ld64-2.patch
patch -p0 < $PATCH_DIR/cctools-ld64-3.patch
patch -p0 < $PATCH_DIR/cctools-conf-1.patch
patch -p0 < $PATCH_DIR/cctools-llvm-lto.patch
CF=$CFLAGS CXXF=$CXXFLAGS LF=$LDFLAGS
eval `$BASE_DIR/oclang/find_lto_header.sh`
set +e
grep -n "__block," /usr/include/unistd.h &>/dev/null
if [ $? -eq 0 ]; then
    echo "applying workaround for buggy unistd.h"
    $PATCH_DIR/fix-unistd-issue.sh
fi
set -e
./autogen.sh
./configure --prefix=$TARGET_DIR --target=x86_64-apple-$TARGET
make -j$JOBS
make install -j$JOBS
export CFLAGS=$CF CXXFLAGS=$CXXF LDFLAGS=$LF

pushd $TARGET_DIR/bin
CCTOOLS=`find . -name "x86_64-apple-darwin*"`
CCTOOLS=($CCTOOLS)
for CCTOOL in ${CCTOOLS[@]}; do
    CCTOOL_I386=`echo "$CCTOOL" | sed 's/x86_64/i386/g'`
    ln -sf $CCTOOL $CCTOOL_I386
done
popd

popd

pushd xar*
./configure --prefix=$TARGET_DIR
make -j$JOBS
make install -j$JOBS
popd

function check
{
    [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-lipo" ] || exit 1
    [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-ld" ] || exit 1
    [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-nm" ] || exit 1
    [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-ar" ] || exit 1
    [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-ranlib" ] || exit 1
    [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-strip" ] || exit 1
}

check i386
check x86_64

touch "have_cctools_$TARGET"

fi # HAVE_CCTOOLS

set +e
ls $TARBALL_DIR/MacOSX$SDK_VERSION* &>/dev/null
while [ $? -ne 0 ]
do
    echo ""
    echo "Get the MacOSX$SDK_VERSION SDK and move it into $TARBALL_DIR"
    echo "(see README for SDK download links)"
    echo ""
    echo "You can press ctrl-c to break the build process,"
    echo "if you restart ./build.sh then we will continue from here"
    echo ""
    read -p "Press enter to continue"
    ls $TARBALL_DIR/MacOSX$SDK_VERSION* &>/dev/null
done
set -e

SDK=`ls $TARBALL_DIR/MacOSX$SDK_VERSION*`
SDK_FILENAME=`basename $SDK`

echo "extracting $SDK_FILENAME ..."

case $SDK in
    *.pkg)
        xar -xf $SDK
        cat Payload | gunzip -dc | cpio -i 2>/dev/null
        ;;
    *.tar.xz)
        tar xJf $SDK
        ;;
    *.tar.gz)
        tar xzf $SDK
        ;;
esac

rm -rf $SDK_DIR/MacOSX$SDK_VERSION* 2>/dev/null
mv -f SDKs/*$SDK_VERSION* $SDK_DIR

pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk
set +e
ln -s $SDK_DIR/MacOSX$SDK_VERSION.sdk/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/std*.h usr/include 2>/dev/null
$BASE_DIR/oclang/find_intrinsic_headers.sh $SDK_DIR/MacOSX$SDK_VERSION.sdk
set -e
popd

popd

cp -f oclang/oclang $TARGET_DIR/bin

ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/o32-clang
ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/o32-clang++
ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/o64-clang
ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/o64-clang++

ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/i386-apple-$TARGET-clang
ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/i386-apple-$TARGET-clang++
ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/x86_64-apple-$TARGET-clang
ln -sf $TARGET_DIR/bin/oclang $TARGET_DIR/bin/x86_64-apple-$TARGET-clang++

OSXCROSS_CONF="$TARGET_DIR/bin/osxcross-conf"

rm -f $OSXCROSS_CONF 2>/dev/null

echo "#!/usr/bin/env bash" > $OSXCROSS_CONF
echo "echo \"export OSXCROSS_VERSION=$OSXCROSS_VERSION\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_OSX_VERSION_MIN=$OSX_VERSION_MIN\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARGET=$TARGET\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_SDK_VERSION=$SDK_VERSION\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_SDK=$SDK_DIR/MacOSX$SDK_VERSION.sdk\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARBALL_DIR=$TARBALL_DIR\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_PATCH_DIR=$PATCH_DIR\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARGET_DIR=$TARGET_DIR\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_BUILD_DIR=$BUILD_DIR\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_CCTOOLS_PATH=$TARGET_DIR/bin\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARGET_OPTION=$CLANG_TARGET_OPTION\"" >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_LINKER_VERSION=$LINKER_VERSION\"" >> $OSXCROSS_CONF
chmod +x $OSXCROSS_CONF

function test_compiler
{
    echo -ne "testing $1 ... "
    $1 $2 -O2 -Wall -o test && rm test
    echo "works"
}

echo ""

test_compiler o32-clang $BASE_DIR/oclang/test.c
test_compiler o64-clang $BASE_DIR/oclang/test.c

test_compiler o32-clang++ $BASE_DIR/oclang/test.cpp
test_compiler o64-clang++ $BASE_DIR/oclang/test.cpp

echo ""
echo "Now add"
echo ""
echo "export PATH=\$PATH:$TARGET_DIR/bin"
echo ""
echo "to your ~/.bashrc or ~/.profile"
echo ""

echo "Done! Now you can use o32-clang(++) and o64-clang(++) like a normal compiler"
echo ""
echo "Example usage:"
echo ""
echo "Example 1: CC=o32-clang ./configure --host=i386-apple-$TARGET"
echo "Example 2: CC=i386-apple-$TARGET-clang ./configure --host=i386-apple-$TARGET"
echo "Example 3: o64-clang -Wall test.c -o test"
echo "Example 4: x86_64-apple-$TARGET-strip -x test"
echo ""
