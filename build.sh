#!/usr/bin/env bash
#
# Build and install the cctools the SDK and macports.
#
# This script requires the OS X SDK and the Clang/LLVM compiler.
#

VERSION=1.4

pushd "${0%/*}" &>/dev/null

source tools/tools.sh

if [ $SDK_VERSION ]; then
  echo 'SDK VERSION set in environment variable:' $SDK_VERSION
else
  guess_sdk_version
  SDK_VERSION=$guess_sdk_version_result
fi
verify_sdk_version $SDK_VERSION

case $SDK_VERSION in
  10.4*|10.5*)
    echo ""
    echo "SDK <= 10.5 no longer supported. Use 'osxcross-1.1' branch instead."
    exit 1
      ;;
esac


case $SDK_VERSION in
  10.6*)  TARGET=darwin10; X86_64H_SUPPORTED=0; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.7*)  TARGET=darwin11; X86_64H_SUPPORTED=0; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.8*)  TARGET=darwin12; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.9*)  TARGET=darwin13; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.10*) TARGET=darwin14; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.11*) TARGET=darwin15; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6;  ;;
  10.12*) TARGET=darwin16; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6;  ;;
  10.13*) TARGET=darwin17; X86_64H_SUPPORTED=1; I386_SUPPORTED=1; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6;  ;;
  10.14*) TARGET=darwin18; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  10.15*) TARGET=darwin19; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=0; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  10.16*) TARGET=darwin20; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11.0*)  TARGET=darwin20.1; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11.1*)  TARGET=darwin20.2; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11.2*)  TARGET=darwin20.3; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11.3*)  TARGET=darwin20.4; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.0*)  TARGET=darwin21.1; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.1*)  TARGET=darwin21.2; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.2*)  TARGET=darwin21.3; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.3*)  TARGET=darwin21.4; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.4*)  TARGET=darwin21.5; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  13.0*)  TARGET=darwin22; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
  13.1*)  TARGET=darwin22.2; X86_64H_SUPPORTED=1; I386_SUPPORTED=0; ARM_SUPPORTED=1; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9;  ;;
 *) echo "Unsupported SDK"; exit 1 ;;
esac

# Minimum targeted macOS version
# Must be <= SDK_VERSION
if [ -n "$OSX_VERSION_MIN_INT" -a -z "$OSX_VERSION_MIN" ]; then
  OSX_VERSION_MIN=$OSX_VERSION_MIN_INT
fi

export TARGET

echo ""
echo "Building OSXCross toolchain, Version: $VERSION"
echo ""
echo "macOS SDK Version: $SDK_VERSION, Target: $TARGET"
echo "Minimum targeted macOS Version: $OSX_VERSION_MIN"
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
  get_sources https://github.com/tpoechtrager/apple-libtapi.git 1100.0.11

  if [ $f_res -eq 1 ]; then
    pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
    INSTALLPREFIX=$TARGET_DIR ./build.sh
    ./install.sh
    popd &>/dev/null
    build_success
  fi
fi

## cctools and ld64 ##

CCTOOLS_VERSION=973.0.1
LINKER_VERSION=609

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
  ./configure $CONFFLAGS
  $MAKE -j$JOBS
  $MAKE install -j$JOBS
  popd &>/dev/null
fi

## Create Arch Symlinks ##

pushd $TARGET_DIR/bin &>/dev/null
CCTOOLS=($(find . -name "x86_64-apple-${TARGET}*"))
function create_arch_symlinks()
{
  local arch=$1
  for CCTOOL in ${CCTOOLS[@]}; do
    verbose_cmd create_symlink $CCTOOL $(echo "$CCTOOL" | $SED "s/x86_64/$arch/g")
  done
}
if [ $X86_64H_SUPPORTED -eq 1 ]; then
  create_arch_symlinks "x86_64h"
fi
if [ $I386_SUPPORTED -eq 1 ]; then
  create_arch_symlinks "i386"
fi

if [ $ARM_SUPPORTED -eq 1 ]; then
  create_arch_symlinks "aarch64"
  create_arch_symlinks "arm64"
  create_arch_symlinks "arm64e"
fi
# For unpatched dsymutil. There is currently no way around it.
create_symlink x86_64-apple-$TARGET-lipo lipo
popd &>/dev/null


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

## Fix broken SDKs ##

pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk &>/dev/null
set +e
files=$(echo $BASE_DIR/oclang/quirks/*.h)
for file in $files; do
  filename=$(basename $file)
  if [ ! -f "usr/include/$filename" ]; then
    rm -f usr/include/$filename # Broken symlink
    cp $file usr/include
  fi
done
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
export ARM_SUPPORTED
export TOP_BUILD_SCRIPT=1

$BASE_DIR/wrapper/build_wrapper.sh

echo ""

if [ $(osxcross-cmp $SDK_VERSION "<" $OSX_VERSION_MIN) -eq 1 ]; then
  echo "OSX_VERSION_MIN must be <= SDK_VERSION"
  trap "" EXIT
  exit 1
elif [ $(osxcross-cmp $OSX_VERSION_MIN "<" 10.6) -eq 1  ]; then
  echo "OSX_VERSION_MIN must be >= 10.6"
  trap "" EXIT
  exit 1
fi

## CMake ##

cp -f "$BASE_DIR/tools/toolchain.cmake" "$TARGET_DIR/"
cp -f "$BASE_DIR/tools/osxcross-cmake" "$TARGET_DIR/bin/"

chmod 755 "$TARGET_DIR/bin/osxcross-cmake"

if [ $I386_SUPPORTED -eq 1 ]; then
  create_symlink osxcross-cmake "$TARGET_DIR/bin/i386-apple-$TARGET-cmake"
fi

create_symlink osxcross-cmake "$TARGET_DIR/bin/x86_64-apple-$TARGET-cmake"

if [ $X86_64H_SUPPORTED -eq 1 ]; then
  create_symlink osxcross-cmake "$TARGET_DIR/bin/x86_64h-apple-$TARGET-cmake"
fi

if [ $ARM_SUPPORTED -eq 1 ]; then
  create_symlink osxcross-cmake "$TARGET_DIR/bin/aarch64-apple-$TARGET-cmake"
  create_symlink osxcross-cmake "$TARGET_DIR/bin/arm64-apple-$TARGET-cmake"
  create_symlink osxcross-cmake "$TARGET_DIR/bin/arm64e-apple-$TARGET-cmake"
fi


## Compiler test ##

unset MACOSX_DEPLOYMENT_TARGET

if [ $(osxcross-cmp $SDK_VERSION ">=" 10.7) -eq 1 ]; then
  pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk &>/dev/null
  if [ ! -f "usr/include/c++/v1/vector" ]; then
    echo ""
    echo -n "Given SDK does not contain libc++ headers "
    echo "(-stdlib=libc++ test may fail)"
    echo -n "You may want to re-package your SDK using "
    echo "'tools/gen_sdk_package.sh' on macOS"
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
    test_compiler_cxx11 i386-apple-$TARGET-clang++ $BASE_DIR/oclang/test_libcxx.cpp
  fi
  test_compiler_cxx11 x86_64-apple-$TARGET-clang++ $BASE_DIR/oclang/test_libcxx.cpp
  echo ""
fi

if [ $I386_SUPPORTED -eq 1 ]; then
  test_compiler i386-apple-$TARGET-clang $BASE_DIR/oclang/test.c "required"
  test_compiler i386-apple-$TARGET-clang++ $BASE_DIR/oclang/test.cpp "required"
  echo ""
fi

if [ $X86_64H_SUPPORTED -eq 1 ]; then
  test_compiler x86_64h-apple-$TARGET-clang $BASE_DIR/oclang/test.c
  test_compiler x86_64h-apple-$TARGET-clang++ $BASE_DIR/oclang/test.cpp
  echo ""
fi

if [ $ARM_SUPPORTED -eq 1 ]; then
  test_compiler arm64-apple-$TARGET-clang $BASE_DIR/oclang/test.c
  test_compiler arm64-apple-$TARGET-clang++ $BASE_DIR/oclang/test.cpp
  echo ""

  test_compiler arm64e-apple-$TARGET-clang $BASE_DIR/oclang/test.c
  test_compiler arm64e-apple-$TARGET-clang++ $BASE_DIR/oclang/test.cpp
  echo ""
fi

test_compiler x86_64-apple-$TARGET-clang $BASE_DIR/oclang/test.c "required"
test_compiler x86_64-apple-$TARGET-clang++ $BASE_DIR/oclang/test.cpp "required"

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

if [ $ARM_SUPPORTED -eq 1 ]; then
  echo "!!! Use aarch64-apple-$TARGET-* instead of arm64-* when dealing with Automake !!!"
  echo "!!! CC=aarch64-apple-$TARGET-clang ./configure --host=aarch64-apple-$TARGET !!!"
  echo "!!! CC=\"aarch64-apple-$TARGET-clang -arch arm64e\" ./configure --host=aarch64-apple-$TARGET !!!"
  echo ""
fi


if [ $I386_SUPPORTED -eq 0 ]; then
  echo "Your SDK does not support i386 anymore."
  echo "Use <= 10.13 SDK if you rely on i386 support."
  echo ""
fi

if [ $(osxcross-cmp $SDK_VERSION ">=" 10.14) -eq 1 ]; then
  echo "Your SDK does not support libstdc++ anymore."
  echo "Use <= 10.13 SDK if you rely on libstdc++ support."
  echo ""
fi
