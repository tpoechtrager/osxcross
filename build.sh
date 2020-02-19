#!/usr/bin/env bash
#
# Build and install the cctools the SDK and macports.
#
# This script requires the OS X SDK and the Clang/LLVM compiler.
#

VERSION=1.1

pushd "${0%/*}" &>/dev/null

source tools/tools.sh

if [ $SDK_VERSION ]; then
  echo 'SDK VERSION set in environment variable:' $SDK_VERSION
  test $SDK_VERSION = 10.4 && SDK_VERSION=10.4u
else
  guess_sdk_version
  SDK_VERSION=$guess_sdk_version_result
fi
verify_sdk_version $SDK_VERSION

case $SDK_VERSION in
  10.4*)  TARGET=darwin8;  X86_64H_SUPPORTED=0; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.4;  ;;
  10.5*)  TARGET=darwin9;  X86_64H_SUPPORTED=0; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.5;  ;;
  10.6*)  TARGET=darwin10; X86_64H_SUPPORTED=0; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.5;  ;;
  10.7*)  TARGET=darwin11; X86_64H_SUPPORTED=0; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.5;  ;;
  10.8*)  TARGET=darwin12; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.5;  ;;
  10.9*)  TARGET=darwin13; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.5;  ;;
  10.10*) TARGET=darwin14; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.5;  ;;
  10.11*) TARGET=darwin15; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.5;  ;;
  10.12*) TARGET=darwin16; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.5;  ;;
  10.13*) TARGET=darwin17; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.5;  ;;
  10.14*) TARGET=darwin18; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  10.15*) TARGET=darwin19; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
*) echo "Unsupported SDK"; exit 1 ;;
esac


# Minimum targeted OS X version
# Must be <= SDK_VERSION
if [ -n "$OSX_VERSION_MIN_INT" -a -z "$OSX_VERSION_MIN" ]; then
  OSX_VERSION_MIN=$OSX_VERSION_MIN_INT
fi


export TARGET

echo ""
echo "Building OSXCross toolchain, Version: $VERSION"
echo ""
echo "OS X SDK Version: $SDK_VERSION, Target: $TARGET"
echo "Minimum targeted OS X Version: $OSX_VERSION_MIN"
echo "Tarball Directory: $TARBALL_DIR"
echo "Build Directory: $BUILD_DIR"
echo "Install Directory: $TARGET_DIR"
echo "SDK Install Directory: $SDK_DIR"
if [ -z "$UNATTENDED" ]; then
  echo ""
  read -p "Press enter to start building"
fi
echo ""

export PATH=$TARGET_DIR/bin:$PATH

mkdir -p $BUILD_DIR
mkdir -p $TARGET_DIR
mkdir -p $SDK_DIR

source $BASE_DIR/tools/trap_exit.sh

pushd $BUILD_DIR &>/dev/null



OLD_SDK_VERSION=$(cat .oc_sdk_version 2>/dev/null || echo "")
echo -n "$SDK_VERSION" > .oc_sdk_version

if [ "$SDK_VERSION" != "$OLD_SDK_VERSION" ]; then
  # SDK Version has changed. -> Rebuild everything.
  rm -f .*_build_complete
fi

# XAR

build_xar

# XAR END


## Apple TAPI Library ##

if [ $NEED_TAPI_SUPPORT -eq 1 ]; then
  get_sources https://github.com/tpoechtrager/apple-libtapi.git 1000.10.8

  if [ $f_res -eq 1 ]; then
    pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
    INSTALLPREFIX=$TARGET_DIR ./build.sh
    ./install.sh
    popd &>/dev/null
    build_success
  fi
fi

## cctools and ld64 ##

CCTOOLS_VERSION=927.0.2
LINKER_VERSION=450.3

get_sources \
  https://github.com/tpoechtrager/cctools-port.git \
  $CCTOOLS_VERSION-ld64-$LINKER_VERSION

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME/cctools &>/dev/null
  echo ""

  CONFFLAGS="--prefix=$TARGET_DIR --target=x86_64-apple-$TARGET "
  if [ $NEED_TAPI_SUPPORT -eq 1 ]; then
    CONFFLAGS+="--with-libtapi=$TARGET_DIR "
  fi
  CONFFLAGS+="--with-libxar=$TARGET_DIR "
  [ -n "$DISABLE_CLANG_AS" ] && CONFFLAGS+="--disable-clang-as "
  [ -n "$DISABLE_LTO_SUPPORT" ] && CONFFLAGS+="--disable-lto-support "
  # https://github.com/tpoechtrager/osxcross/issues/156
  CXX="$CXX -DNDEBUG" ./configure $CONFFLAGS
  $MAKE -j$JOBS
  $MAKE install -j$JOBS
  popd &>/dev/null

  pushd $TARGET_DIR/bin &>/dev/null
  CCTOOLS=$(find . -name "x86_64-apple-darwin*")
  CCTOOLS=($CCTOOLS)
  if [ $X86_64H_SUPPORTED -eq 1 ]; then
    for CCTOOL in ${CCTOOLS[@]}; do
      CCTOOL_X86_64H=$(echo "$CCTOOL" | $SED 's/x86_64/x86_64h/g')
      create_symlink $CCTOOL $CCTOOL_X86_64H
    done
  fi
  if [ $I386_SUPPORTED -eq 1 ]; then
    for CCTOOL in ${CCTOOLS[@]}; do
      CCTOOL_I386=$(echo "$CCTOOL" | $SED 's/x86_64/i386/g')
      create_symlink $CCTOOL $CCTOOL_I386
    done
  fi
  # For unpatched dsymutil. There is currently no way around it.
  create_symlink x86_64-apple-$TARGET-lipo lipo
  popd &>/dev/null
fi


## MacPorts ##

pushd $TARGET_DIR/bin &>/dev/null
rm -f osxcross-macports
cp $BASE_DIR/tools/osxcross-macports osxcross-macports
create_symlink osxcross-macports osxcross-mp
create_symlink osxcross-macports omp
popd &>/dev/null

## Extract SDK and move it to $SDK_DIR ##

SDK=$(ls $TARBALL_DIR/MacOSX$SDK_VERSION*)
echo ""
extract $SDK

rm -rf $SDK_DIR/MacOSX$SDK_VERSION* 2>/dev/null
if [ "$(ls -l SDKs/*$SDK_VERSION* 2>/dev/null | wc -l | tr -d ' ')" != "0" ]; then
  mv -f SDKs/*$SDK_VERSION* $SDK_DIR
else
  mv -f *OSX*$SDK_VERSION*sdk* $SDK_DIR
fi

## Android/Termux is broken

if [ $OPERATING_SYSTEM == "Android" ]; then
  pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk &>/dev/null
  mkdir -p data/data/com.termux/files/usr
  create_symlink $PWD/usr/include data/data/com.termux/files/usr
  popd &>/dev/null
fi

## Fix broken SDKs ##

pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk &>/dev/null
set +e
if [ $PLATFORM == "FreeBSD" ] || [ $PLATFORM == "OpenBSD" ]; then
  files=$(echo $SDK_DIR/MacOSX$SDK_VERSION.sdk/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/std*.h)
  for file in $files; do
    rm -f usr/include/$(basename $file)
    cp $file usr/include
  done
  cp -f $BASE_DIR/oclang/quirks/tgmath.h usr/include
fi
if [ ! -f "usr/include/float.h" ]; then
  cp -f $BASE_DIR/oclang/quirks/float.h usr/include
fi
set -e
popd &>/dev/null

popd &>/dev/null

## Wrapper ##

build_msg "wrapper"

OSXCROSS_CONF="$TARGET_DIR/bin/osxcross-conf"
OSXCROSS_ENV="$TARGET_DIR/bin/osxcross-env"
rm -f $OSXCROSS_CONF $OSXCROSS_ENV

if [ "$PLATFORM" != "Darwin" ]; then
  # libLTO.so
  set +e
  eval $(cat $BUILD_DIR/cctools*/cctools/config.log | grep LLVM_LIB_DIR | head -n1)
  set -e
  export LIBLTO_PATH=$LLVM_LIB_DIR
fi

export VERSION
export TARGET
export BUILD_DIR
export OSX_VERSION_MIN
export LIBLTO_PATH
export LINKER_VERSION
export X86_64H_SUPPORTED
export I386_SUPPORTED
export TOP_BUILD_SCRIPT=1

$BASE_DIR/wrapper/build_wrapper.sh

echo ""

if [ $(osxcross-cmp ${SDK_VERSION/u/} "<" $OSX_VERSION_MIN) -eq 1 ]; then
  echo "OSX_VERSION_MIN must be <= SDK_VERSION"
  trap "" EXIT
  exit 1
elif [ $(osxcross-cmp $OSX_VERSION_MIN "<" 10.4) -eq 1  ]; then
  echo "OSX_VERSION_MIN must be >= 10.4"
  trap "" EXIT
  exit 1
fi

## CMake ##

cp -f "$BASE_DIR/tools/toolchain.cmake" "$TARGET_DIR/"
cp -f "$BASE_DIR/tools/osxcross-cmake" "$TARGET_DIR/bin/"
chmod 755 "$TARGET_DIR/bin/osxcross-cmake"
create_symlink osxcross-cmake "$TARGET_DIR/bin/i386-apple-$TARGET-cmake"
create_symlink osxcross-cmake "$TARGET_DIR/bin/x86_64-apple-$TARGET-cmake"

## Compiler test ##

unset MACOSX_DEPLOYMENT_TARGET

if [ $(osxcross-cmp ${SDK_VERSION/u/} ">=" 10.7) -eq 1 ]; then
  pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk &>/dev/null
  if [ ! -f "usr/include/c++/v1/vector" ]; then
    echo ""
    echo -n "Given SDK does not contain libc++ headers "
    echo "(-stdlib=libc++ test may fail)"
    echo -n "You may want to re-package your SDK using "
    echo "'tools/gen_sdk_package.sh' on OS X"
  fi
  if [ -f "usr/include/c++/v1/__hash_table" ]; then
    if [ $(osxcross-cmp $SDK_VERSION ">=" 10.7) -eq 1 ]; then
    if [ $(osxcross-cmp $SDK_VERSION "<=" 10.12) -eq 1 ]; then
      # https://github.com/tpoechtrager/osxcross/issues/171
      set +e
      patch -N -p1 -r /dev/null < $PATCH_DIR/libcxx__hash_table.patch
      set -e
    fi
    fi
  fi
  if [ -f "usr/include/Availability.h" ]; then
    if [ $(osxcross-cmp $SDK_VERSION "==" 10.15) -eq 1 ]; then
      # 10.15 comes with a broken Availability.h header file
      # which breaks building GCC
      set +e
      cat $PATCH_DIR/gcc_availability.h >> usr/include/Availability.h
      set -e
    fi
  fi
  popd &>/dev/null
  echo ""
  if [ $I386_SUPPORTED -eq 1 ]; then
    test_compiler_cxx11 o32-clang++ $BASE_DIR/oclang/test_libcxx.cpp
  fi
  test_compiler_cxx11 o64-clang++ $BASE_DIR/oclang/test_libcxx.cpp
  echo ""
fi

if [ $I386_SUPPORTED -eq 1 ]; then
  test_compiler o32-clang $BASE_DIR/oclang/test.c
  test_compiler o32-clang++ $BASE_DIR/oclang/test.cpp
  echo ""
fi

test_compiler o64-clang $BASE_DIR/oclang/test.c
test_compiler o64-clang++ $BASE_DIR/oclang/test.cpp

echo ""
echo "Do not forget to add"
echo ""
echo -e "\x1B[32m${TARGET_DIR}/bin\x1B[0m"
echo ""
echo "to your PATH variable."
echo ""

echo "All done! Now you can use o32-clang(++) and o64-clang(++) like a normal compiler."
echo ""
echo "Example usage:"
echo ""
echo "Example 1: CC=o32-clang ./configure --host=i386-apple-$TARGET"
echo "Example 2: CC=i386-apple-$TARGET-clang ./configure --host=i386-apple-$TARGET"
echo "Example 3: o64-clang -Wall test.c -o test"
echo "Example 4: x86_64-apple-$TARGET-strip -x test"
echo ""

if [ $I386_SUPPORTED -eq 0 ]; then
  echo "Your SDK does not support i386 anymore."
  echo "Use <= 10.13 SDK if you rely on i386 support."
  echo ""
fi

if [ $(osxcross-cmp ${SDK_VERSION/u/} ">=" 10.14) -eq 1 ]; then
  echo "Your SDK does not support libstdc++ anymore."
  echo "Use <= 10.13 SDK if you rely on libstdc++ support."
  echo ""
fi
