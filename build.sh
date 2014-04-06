#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null

source tools/tools.sh

# find sdk version to use
function guess_sdk_version()
{
  tmp1=
  tmp2=
  tmp3=
  file=
  sdk=
  guess_sdk_version_result=
  sdkcount=`find tarballs/ -type f | grep MacOSX | wc -l`
  if [ $sdkcount -eq 0 ]; then
    echo no SDK found in 'tarballs/'. please see README.md
    exit 1
  elif [ $sdkcount -gt 1 ]; then
    sdks=`find tarballs/ | grep MacOSX`
    for sdk in $sdks; do echo $sdk; done
    echo 'more than one MacOSX SDK tarball found. please set'
    echo 'SDK_VERSION environment variable for the one you want'
    echo '(for example: SDK_VERSION=10.x [OSX_VERSION_MIN=10.x] ./build.sh)'
    exit 1
  else
    sdk=`find tarballs/ | grep MacOSX`
    tmp2=`echo ${sdk/bz2/} | sed s/[^0-9.]//g`
    tmp3=`echo $tmp2 | sed s/\\\.*$//g`
    guess_sdk_version_result=$tmp3
    echo 'found SDK version' $guess_sdk_version_result 'at tarballs/'`basename $sdk`
  fi
  if [ $guess_sdk_version_result ]; then
    if [ $guess_sdk_version_result = 10.4 ]; then
      guess_sdk_version_result=10.4u
    fi
  fi
  export guess_sdk_version_result
}

# make sure there is actually a file with the given SDK_VERSION
function verify_sdk_version()
{
  sdkv=$1
  for file in tarballs/*; do
    if [ `echo $file | grep OSX.*$sdkv` ]; then
      echo "verified at "$file
      sdk=$file
    fi
  done
  if [ ! $sdk ] ; then
    echo cant find SDK for OSX $sdkv in tarballs. exiting
    exit
  fi
}

if [ $SDK_VERSION ]; then
  echo 'SDK VERSION set in environment variable:' $SDK_VERSION
  test $SDK_VERSION = 10.4 && SDK_VERSION=10.4u
else
  guess_sdk_version
  SDK_VERSION=$guess_sdk_version_result
fi
verify_sdk_version $SDK_VERSION

# Minimum targeted OS X version
# Must be <= SDK_VERSION
# You can comment this variable out,
# if you want to use the compiler's default value
if [ -z "$OSX_VERSION_MIN" ]; then
  if [ $SDK_VERSION = 10.4u ]; then
    OSX_VERSION_MIN=10.4
  else
    OSX_VERSION_MIN=10.5
  fi
fi

# ld version
LINKER_VERSION=134.9

# Don't change this
OSXCROSS_VERSION=0.6

TARBALL_DIR=$BASE_DIR/tarballs
BUILD_DIR=$BASE_DIR/build
TARGET_DIR=$BASE_DIR/target
PATCH_DIR=$BASE_DIR/patches
SDK_DIR=$TARGET_DIR/SDK

if [ -z "$OSX_VERSION_MIN" ]; then
  OSX_VERSION_MIN="default"
fi

case $SDK_VERSION in
  10.4*) TARGET=darwin8 ;;
  10.5*) TARGET=darwin9 ;;
  10.6*) TARGET=darwin10 ;;
  10.7*) TARGET=darwin11 ;;
  10.8*) TARGET=darwin12 ;;
  10.9*) TARGET=darwin13 ;;
  *) echo "Invalid SDK Version" && exit 1 ;;
esac

echo ""
echo "Building OSXCross toolchain, Version: $OSXCROSS_VERSION"
echo ""
echo "OS X SDK Version: $SDK_VERSION, Target: $TARGET"
echo "Minimum targeted OS X Version: $OSX_VERSION_MIN"
echo "Tarball Directory: $TARBALL_DIR"
echo "Build Directory: $BUILD_DIR"
echo "Install Directory: $TARGET_DIR"
echo "SDK Install Directory: $SDK_DIR"
echo ""
read -p "Press enter to start building"
echo ""

export PATH=$TARGET_DIR/bin:$PATH

mkdir -p $BUILD_DIR
mkdir -p $TARGET_DIR
mkdir -p $SDK_DIR

require $CC
require $CXX
require clang
require sed
require patch
require gunzip
require cpio
require autogen
require automake
require libtool

CLANG_TARGET_OPTION=`./oclang/check_target_option.sh`

pushd $BUILD_DIR &>/dev/null

function remove_locks()
{
  rm -rf $BUILD_DIR/have_cctools*
}

source $BASE_DIR/tools/trap_exit.sh

if [ "`ls $TARBALL_DIR/cctools*.tar.* | wc -l | tr -d ' '`" != "1" ]; then
  echo ""
  echo "There should only be one cctools*.tar.* archive in the tarballs directory"
  echo ""
  exit 1
fi

CCTOOLS_REVHASH=`ls $TARBALL_DIR/cctools*.tar.* | tr '_' ' ' | tr '.' ' ' | awk '{print $3}'`

# CCTOOLS
if [ ! -f "have_cctools_${CCTOOLS_REVHASH}_$TARGET" ]; then

rm -rf cctools*
rm -rf xar*
rm -rf bc*

extract $TARBALL_DIR/cctools*.tar.xz 1 1 1

pushd cctools*/cctools &>/dev/null
pushd .. &>/dev/null
patch -p0 -l < $PATCH_DIR/cctools-63f6742.patch
popd &>/dev/null
patch -p0 < $PATCH_DIR/cctools-ld64-1.patch
patch -p0 < $PATCH_DIR/cctools-ld64-2.patch
patch -p0 < $PATCH_DIR/cctools-ld64-3.patch
echo ""
./autogen.sh
echo ""
echo "if you see automake warnings, ignore them"
echo "automake 1.14+ is supposed to print a lot of warnings"
echo ""
./configure --prefix=$TARGET_DIR --target=x86_64-apple-$TARGET
$MAKE -j$JOBS
$MAKE install -j$JOBS
popd &>/dev/null

pushd $TARGET_DIR/bin &>/dev/null
CCTOOLS=`find . -name "x86_64-apple-darwin*"`
CCTOOLS=($CCTOOLS)
for CCTOOL in ${CCTOOLS[@]}; do
  CCTOOL_I386=`echo "$CCTOOL" | sed 's/x86_64/i386/g'`
  ln -sf $CCTOOL $CCTOOL_I386
done
popd &>/dev/null

fi
# CCTOOLS END

# BC
set +e
which bc &>/dev/null
NEED_BC=$?
set -e

if [ $NEED_BC -ne 0 ]; then

extract $TARBALL_DIR/bc*.tar.bz2 2

pushd bc* &>/dev/null
CFLAGS="-w" ./configure --prefix=$TARGET_DIR --without-flex
$MAKE -j$JOBS
$MAKE install -j$JOBS
popd &>/dev/null

fi
# BC END

SDK=`ls $TARBALL_DIR/MacOSX$SDK_VERSION*`

# XAR
if [[ $SDK == *.pkg ]]; then

set +e
which xar &>/dev/null
NEED_XAR=$?
set -e

if [ $NEED_XAR -ne 0 ]; then

extract $TARBALL_DIR/xar*.tar.gz 2

pushd xar* &>/dev/null
test "`uname -s`" = "NetBSD" && patch -p0 -l < $PATCH_DIR/xar-netbsd.patch
CFLAGS+=" -w" ./configure --prefix=$TARGET_DIR
$MAKE -j$JOBS
$MAKE install -j$JOBS
popd &>/dev/null

fi
fi
# XAR END

if [ ! -f "have_cctools_$TARGET" ]; then

function check_cctools()
{
  [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-lipo" ] || exit 1
  [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-ld" ] || exit 1
  [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-nm" ] || exit 1
  [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-ar" ] || exit 1
  [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-ranlib" ] || exit 1
  [ -f "/$TARGET_DIR/bin/$1-apple-$TARGET-strip" ] || exit 1
}

check_cctools i386
check_cctools x86_64

touch "have_cctools_${CCTOOLS_REVHASH}_$TARGET"

echo ""

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

extract $SDK 1 1

rm -rf $SDK_DIR/MacOSX$SDK_VERSION* 2>/dev/null

if [ "`ls -l SDKs/*$SDK_VERSION* 2>/dev/null | wc -l | tr -d ' '`" != "0" ]; then
  mv -f SDKs/*$SDK_VERSION* $SDK_DIR
else
  mv -f *OSX*$SDK_VERSION*sdk* $SDK_DIR
fi

pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk &>/dev/null
set +e
ln -s \
  $SDK_DIR/MacOSX$SDK_VERSION.sdk/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/std*.h \
  usr/include 2>/dev/null
$BASE_DIR/oclang/find_intrinsic_headers.sh $SDK_DIR/MacOSX$SDK_VERSION.sdk
test ! -f "usr/include/float.h" && cp -f $BASE_DIR/oclang/quirks/float.h usr/include
set -e
popd &>/dev/null

popd &>/dev/null

cp -f oclang/dsymutil $TARGET_DIR/bin

WRAPPER=$TARGET_DIR/bin/x86_64-apple-$TARGET-oclang
cp -f oclang/oclang $WRAPPER

WRAPPER_SCRIPT=`basename $WRAPPER`
WRAPPER_DIR=`dirname $WRAPPER`

pushd $WRAPPER_DIR &>/dev/null

ln -sf $WRAPPER_SCRIPT o32-clang
ln -sf $WRAPPER_SCRIPT o32-clang++
ln -sf $WRAPPER_SCRIPT o32-clang++-libc++

ln -sf $WRAPPER_SCRIPT o64-clang
ln -sf $WRAPPER_SCRIPT o64-clang++
ln -sf $WRAPPER_SCRIPT o64-clang++-libc++

ln -sf $WRAPPER_SCRIPT i386-apple-$TARGET-clang
ln -sf $WRAPPER_SCRIPT i386-apple-$TARGET-clang++
ln -sf $WRAPPER_SCRIPT i386-apple-$TARGET-clang++-libc++

ln -sf $WRAPPER_SCRIPT x86_64-apple-$TARGET-clang
ln -sf $WRAPPER_SCRIPT x86_64-apple-$TARGET-clang++
ln -sf $WRAPPER_SCRIPT x86_64-apple-$TARGET-clang++-libc++

popd &>/dev/null

OSXCROSS_CONF="$TARGET_DIR/bin/osxcross-conf"
OSXCROSS_ENV="$TARGET_DIR/bin/osxcross-env"

rm -f $OSXCROSS_CONF $ENV_CONF

echo "#!/usr/bin/env bash"                                                                > $OSXCROSS_CONF
echo ""                                                                                  >> $OSXCROSS_CONF
echo "pushd \"\${0%/*}\" &>/dev/null"                                                    >> $OSXCROSS_CONF
echo ""                                                                                  >> $OSXCROSS_CONF
echo "DIR=\`pwd\`"                                                                       >> $OSXCROSS_CONF
echo "OSXCROSS_ROOT=\$DIR/../.."                                                         >> $OSXCROSS_CONF
echo ""                                                                                  >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_VERSION=$OSXCROSS_VERSION\""                                >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_OSX_VERSION_MIN=$OSX_VERSION_MIN\""                         >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARGET=$TARGET\""                                           >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_SDK_VERSION=$SDK_VERSION\""                                 >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_SDK=\$DIR/../`basename $SDK_DIR`/MacOSX$SDK_VERSION.sdk\""  >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARBALL_DIR=\$OSXCROSS_ROOT/`basename $TARBALL_DIR`\""      >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_PATCH_DIR=\$OSXCROSS_ROOT/`basename $PATCH_DIR`\""          >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARGET_DIR=\$OSXCROSS_ROOT/`basename $TARGET_DIR`\""        >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_BUILD_DIR=\$OSXCROSS_ROOT/`basename $BUILD_DIR`\""          >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_CCTOOLS_PATH=\$DIR\""                                       >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_TARGET_OPTION=$CLANG_TARGET_OPTION\""                       >> $OSXCROSS_CONF
echo "echo \"export OSXCROSS_LINKER_VERSION=$LINKER_VERSION\""                           >> $OSXCROSS_CONF
echo ""                                                                                  >> $OSXCROSS_CONF
echo "popd &>/dev/null"                                                                  >> $OSXCROSS_CONF
echo ""                                                                                  >> $OSXCROSS_CONF

if [ -f $BUILD_DIR/cctools*/cctools/tmp/ldpath ]; then
    LIB_PATH=:`cat $BUILD_DIR/cctools*/cctools/tmp/ldpath`
else
    LIB_PATH=""
fi

echo "#!/bin/sh"                                                                    > $OSXCROSS_ENV
echo ""                                                                            >> $OSXCROSS_ENV
echo "BDIR=\`readlink -f \\\`dirname \$0\\\`\`"                                    >> $OSXCROSS_ENV
echo ""                                                                            >> $OSXCROSS_ENV
echo "echo \"export PATH=\$PATH:\$BDIR\""                                          >> $OSXCROSS_ENV
echo "echo \"export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:\$BDIR/../lib${LIB_PATH}\""  >> $OSXCROSS_ENV


chmod +x $OSXCROSS_CONF $OSXCROSS_ENV

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:`cat $BUILD_DIR/cctools*/cctools/tmp/ldpath`" # libLTO.so

echo ""

if [ -n $OSX_VERSION_MIN ]; then
if [ `echo "${SDK_VERSION/u/}<$OSX_VERSION_MIN" | bc -l` -eq 1 ]; then
  echo "OSX_VERSION_MIN must be <= SDK_VERSION"
  trap "" EXIT
  exit 1
elif [ `echo "$OSX_VERSION_MIN<10.4" | bc -l` -eq 1  ]; then
  echo "OSX_VERSION_MIN must be >= 10.4"
  trap "" EXIT
  exit 1
fi

if [ `echo "${SDK_VERSION/u/}>=10.9" | bc -l` -eq 1 ] && ( [ $OSX_VERSION_MIN == "default" ] ||
   [ `echo "$OSX_VERSION_MIN>=10.9" | bc -l` -eq 1 ] );
then
  export SCRIPT=`basename $0`
  ./build_libcxx.sh || exit 0
fi
fi # OSX_VERSION_MIN set

test_compiler o32-clang $BASE_DIR/oclang/test.c
test_compiler o64-clang $BASE_DIR/oclang/test.c

test_compiler o32-clang++ $BASE_DIR/oclang/test.cpp
test_compiler o64-clang++ $BASE_DIR/oclang/test.cpp

if [ `echo "${SDK_VERSION/u/}>=10.7" | bc -l` -eq 1 ]; then
  if [ ! -d "$SDK_DIR/MacOSX$SDK_VERSION.sdk/usr/include/c++/v1" ]; then
    echo ""
    echo -n "Given SDK does not contain libc++ headers "
    echo "(-stdlib=libc++ test may fail)"
    echo -n "You may want to re-package your SDK using "
    echo "'tools/gen_sdk_package.sh' on OS X"
  fi
  echo ""
  test_compiler_cxx11 o32-clang++ $BASE_DIR/oclang/test_libcxx.cpp
  test_compiler_cxx11 o64-clang++ $BASE_DIR/oclang/test_libcxx.cpp
fi

echo ""
echo "Now add"
echo ""
echo -e "\e[32m\`$OSXCROSS_ENV\`\e[0m"
echo ""
echo "to your ~/.bashrc or ~/.profile (including the '\`')"
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
