#!/usr/bin/env bash
#
# Build and install the cctools the SDK and macports.
#
# This script requires the OS X SDK and the Clang/LLVM compiler.
#

VERSION=2.0-llvm-based

pushd "${0%/*}" &>/dev/null

source tools/tools.sh

if [ $SDK_VERSION ]; then
  echo "SDK VERSION set in environment variable: $SDK_VERSION"
else
  guess_sdk_version
  SDK_VERSION=$guess_sdk_version_result
fi
set_and_verify_sdk_path

case $SDK_VERSION in
  10.4*|10.5*)
    echo ""
    echo "SDK <= 10.5 no longer supported. Use 'osxcross-1.1' branch instead."
    exit 1
      ;;
esac


case $SDK_VERSION in
  10.6*)  TARGET=darwin10; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.7*)  TARGET=darwin11; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.8*)  TARGET=darwin12; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.9*)  TARGET=darwin13; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.10*) TARGET=darwin14; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.11*) TARGET=darwin15; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.12*) TARGET=darwin16; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.13*) TARGET=darwin17; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.6;  ;;
  10.14*) TARGET=darwin18; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.9;  ;;
  10.15*) TARGET=darwin19; ARM_SUPPORTED=0; OSX_VERSION_MIN_INT=10.9;  ;;
  10.16*) TARGET=darwin20; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11|11.0*)  TARGET=darwin20.1; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11.1*)  TARGET=darwin20.2; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11.2*)  TARGET=darwin20.3; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  11.3*)  TARGET=darwin20.4; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12|12.0*)  TARGET=darwin21.1; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.1*)  TARGET=darwin21.2; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.2*)  TARGET=darwin21.3; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.3*)  TARGET=darwin21.4; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  12.4*)  TARGET=darwin21.5; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  13|13.0*)  TARGET=darwin22.1; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  13.1*)  TARGET=darwin22.2; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  13.2*)  TARGET=darwin22.3; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  13.3*)  TARGET=darwin22.4; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.9;  ;;
  14|14.0*)  TARGET=darwin23; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  14.1*)  TARGET=darwin23.1; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  14.2*)  TARGET=darwin23.2; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  14.3*)  TARGET=darwin23.3; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  14.4*)  TARGET=darwin23.4; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  14.5*)  TARGET=darwin23.5; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  14.6*)  TARGET=darwin23.6; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  15|15.0*)  TARGET=darwin24; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  15.1*)  TARGET=darwin24.1; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  15.2*)  TARGET=darwin24.2; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  15.3*)  TARGET=darwin24.3; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  15.4*)  TARGET=darwin24.4; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  15.5*)  TARGET=darwin24.5; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
  26|26.0*)  TARGET=darwin25; ARM_SUPPORTED=1; OSX_VERSION_MIN_INT=10.13; ;;
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
mkdir -p $TARGET_DIR/bin
mkdir -p $SDK_DIR

source $BASE_DIR/tools/trap_exit.sh

pushd $BUILD_DIR &>/dev/null

## cctools-port lipo

if [ -z "$UNATTENDED" ]; then
  message=$'Use lipo from cctools instead of llvm-lipo to improve compatibility?\n'
  message+=$'You can still use llvm-lipo afterwards by setting the env. variable OSXCROSS_FORCE_LLVM_LIPO to 1.'
  if prompt "$message"; then
    echo "Enabling cctools lipo ..."
    ENABLE_REPLACEMENT_LIPO=1
  else
    echo "Using llvm-lipo ..."
  fi
else
  ENABLE_REPLACEMENT_LIPO=1
fi


if [ -n "$ENABLE_REPLACEMENT_LIPO" ]; then

get_sources \
  https://github.com/tpoechtrager/cctools-port.git \
  lipo-1010.6

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME/lipo &>/dev/null
  echo ""
  ./configure $CONFFLAGS
  $MAKE -j$JOBS
  cp misc/lipo $TARGET_DIR/bin/osxcross-replacement-lipo
  popd &>/dev/null
fi

fi

## Extract SDK and move it to $SDK_DIR ##

echo ""
extract $SDK

rm -rf $SDK_DIR/MacOSX$SDK_VERSION* 2>/dev/null
if [ "$(ls -l SDKs/*$SDK_VERSION* 2>/dev/null | wc -l | tr -d ' ')" != "0" ]; then
  mv -f SDKs/*$SDK_VERSION* $SDK_DIR
else
  mv -f *OSX*$SDK_VERSION*sdk* $SDK_DIR
fi

if [ ! -d "$SDK_DIR/MacOSX$SDK_VERSION.sdk" ]; then
  echo "Broken SDK! '$SDK_DIR/MacOSX$SDK_VERSION.sdk' does not exist!"
  exit 1
fi

## Fix broken SDKs ##

pushd $SDK_DIR/MacOSX$SDK_VERSION*.sdk &>/dev/null
# Remove troublesome libc++ IWYU mapping file that may cause compiler errors
# https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUMappings.md
rm -f usr/include/c++/v1/libcxx.imp
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

create_symlink osxcross-cmake "$TARGET_DIR/bin/x86_64-apple-$TARGET-cmake"

if [ $ARM_SUPPORTED -eq 1 ]; then
  create_symlink osxcross-cmake "$TARGET_DIR/bin/aarch64-apple-$TARGET-cmake"
  create_symlink osxcross-cmake "$TARGET_DIR/bin/arm64-apple-$TARGET-cmake"
  create_symlink osxcross-cmake "$TARGET_DIR/bin/arm64e-apple-$TARGET-cmake"
fi

## MacPorts ##

pushd $TARGET_DIR/bin &>/dev/null
rm -f osxcross-macports
cp $BASE_DIR/tools/osxcross-macports osxcross-macports
create_symlink osxcross-macports osxcross-mp
create_symlink osxcross-macports omp
popd &>/dev/null

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
      echo "SDK needs patching for libc++ hash table issue ..."
      patch -N -p1 -r /dev/null < $PATCH_DIR/libcxx__hash_table.patch || true
    fi
    fi
  fi
  if [ -f "usr/include/c++/v1/typeinfo" ]; then
    if [ $(osxcross-cmp "$SDK_VERSION" ">=" 10.7) -eq 1 ]; then
    if [ $(osxcross-cmp "$SDK_VERSION" "<=" 10.8) -eq 1 ]; then
      echo "SDK needs patching for libc++ typeinfo issue ..."
      sed_expr='s/_ATTRIBUTE(noreturn) friend void rethrow_exception(exception_ptr);/'
      sed_expr+='friend void rethrow_exception(exception_ptr);/g'
      $SED -i "$sed_expr" usr/include/c++/v1/exception
    fi
    fi
  fi
  if [ -f "usr/include/Availability.h" ]; then
    if [ $(osxcross-cmp $SDK_VERSION "==" 10.15) -eq 1 ]; then
      # 10.15 comes with a broken Availability.h header file
      # which breaks building GCC
      cat $PATCH_DIR/gcc_availability.h >> usr/include/Availability.h || true
    fi
  fi
  popd &>/dev/null
  echo ""
  test_compiler_cxx11 x86_64-apple-$TARGET-clang++ $BASE_DIR/oclang/test_libcxx.cpp
  echo ""
fi

if [ $(osxcross-cmp $SDK_VERSION ">=" 13.3) -eq 1 ]; then
  CLANG_VERSION=$(echo "__clang_major__ __clang_minor__ __clang_patchlevel__" | \
                  xcrun clang -xc -E - | tail -n1 | tr ' ' '.')

  if [ $(osxcross-cmp $CLANG_VERSION ">=" 13.0) -eq 1 ]; then
    echo "Performing complex c++20 test ..."
    test_compiler_cxx2b x86_64-apple-$TARGET-clang++ $BASE_DIR/oclang/test_libcxx_complex.cpp
    if [ $ARM_SUPPORTED -eq 1 ]; then
      test_compiler_cxx2b arm64-apple-$TARGET-clang++ $BASE_DIR/oclang/test_libcxx_complex.cpp
    fi
    echo ""
  else
    echo "Skipping complex c++20 test. Requires clang >= 13.0."
  fi
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

echo "All done! Now you can use o64-clang(++) like a normal compiler."
echo ""
echo "Example usage:"
echo ""
echo "Example 1: CC=o64-clang ./configure --host=x86_64-apple-$TARGET"
echo "Example 2: CC=x86_64-apple-$TARGET-clang ./configure --host=x86_64-apple-$TARGET"
echo "Example 3: o64-clang -Wall test.c -o test"
echo "Example 4: x86_64-apple-$TARGET-strip -x test"
echo ""

if [ $ARM_SUPPORTED -eq 1 ]; then
  echo "!!! Use aarch64-apple-$TARGET-* instead of arm64-* when dealing with Automake !!!"
  echo "!!! CC=aarch64-apple-$TARGET-clang ./configure --host=aarch64-apple-$TARGET !!!"
  echo "!!! CC=\"aarch64-apple-$TARGET-clang -arch arm64e\" ./configure --host=aarch64-apple-$TARGET !!!"
  echo ""
fi

if [ $(osxcross-cmp $SDK_VERSION ">=" 10.14) -eq 1 ]; then
  echo "Your SDK does not support libstdc++."
  echo "Use <= 10.13 SDK if need libstdc++ support."
  echo ""
fi
