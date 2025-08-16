#!/usr/bin/env bash
#
# Build and install the cctools the SDK and macports.
#
# This script requires the OS X SDK and the Clang/LLVM compiler.
#

VERSION=1.5

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
  10.6*)   TARGET=darwin10;   SUPPORTED_ARCHS="i386 x86_64"; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.7*)   TARGET=darwin11;   SUPPORTED_ARCHS="i386 x86_64"; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.8*)   TARGET=darwin12;   SUPPORTED_ARCHS="i386 x86_64 x86_64h"; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.9*)   TARGET=darwin13;   SUPPORTED_ARCHS="i386 x86_64 x86_64h"; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.10*)  TARGET=darwin14;   SUPPORTED_ARCHS="i386 x86_64 x86_64h"; NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.11*)  TARGET=darwin15;   SUPPORTED_ARCHS="i386 x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6 ;;
  10.12*)  TARGET=darwin16;   SUPPORTED_ARCHS="i386 x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6 ;;
  10.13*)  TARGET=darwin17;   SUPPORTED_ARCHS="i386 x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6 ;;
  10.14*)  TARGET=darwin18;   SUPPORTED_ARCHS="x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  10.15*)  TARGET=darwin19;   SUPPORTED_ARCHS="x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  10.16*)  TARGET=darwin20;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  11|11.0*) TARGET=darwin20.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  11.1*)   TARGET=darwin20.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  11.2*)   TARGET=darwin20.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  11.3*)   TARGET=darwin20.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12|12.0*) TARGET=darwin21.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.1*)   TARGET=darwin21.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.2*)   TARGET=darwin21.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.3*)   TARGET=darwin21.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.4*)   TARGET=darwin21.5; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  13|13.0*) TARGET=darwin22.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  13.1*)   TARGET=darwin22.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  13.2*)   TARGET=darwin22.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  13.3*)   TARGET=darwin22.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  14|14.0*) TARGET=darwin23;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.1*)   TARGET=darwin23.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.2*)   TARGET=darwin23.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.3*)   TARGET=darwin23.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.4*)   TARGET=darwin23.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.5*)   TARGET=darwin23.5; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.6*)   TARGET=darwin23.6; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15|15.0*) TARGET=darwin24;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.1*)   TARGET=darwin24.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.2*)   TARGET=darwin24.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.3*)   TARGET=darwin24.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.4*)   TARGET=darwin24.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.5*)   TARGET=darwin24.5; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  26|26.0*) TARGET=darwin25;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  *) echo "Unsupported SDK"; exit 1 ;;
esac

if [ -n "$ENABLE_ARCHS" ]; then
  for arch in $ENABLE_ARCHS; do
    if ! arch_supported "$arch"; then
      echo "ENABLE_ARCHS: Architecture '$arch' not supported by selected SDK '$SDK_VERSION'" >&2
      exit 1
    fi
  done
  # trim + normalize whitespace
  SUPPORTED_ARCHS="$(printf '%s\n' $ENABLE_ARCHS)"
fi

# Minimum targeted macOS version
# Must be <= SDK_VERSION
if [ -n "$OSX_VERSION_MIN_INT" -a -z "$OSX_VERSION_MIN" ]; then
  OSX_VERSION_MIN=$OSX_VERSION_MIN_INT
fi

export TARGET

echo ""
echo "Building OSXCross toolchain, Version: $VERSION"
echo ""
echo "MacOS SDK Version: $SDK_VERSION, Target: $TARGET"
echo "Minimum targeted macOS Version: $OSX_VERSION_MIN"
echo "Enabled/Supported Archs: ${SUPPORTED_ARCHS// /, }"
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
  get_sources https://github.com/tpoechtrager/apple-libtapi.git 1300.6.5

  if [ $f_res -eq 1 ]; then
    pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
    INSTALLPREFIX=$TARGET_DIR ./build.sh
    ./install.sh
    popd &>/dev/null
    build_success
  fi
fi

## cctools and ld64 ##

CCTOOLS_VERSION=986
LINKER_VERSION=711

get_sources \
  https://github.com/tpoechtrager/cctools-port.git \
  $CCTOOLS_VERSION-ld64-$LINKER_VERSION

if [ $f_res -eq 1 ]; then
  pushd $CURRENT_BUILD_PROJECT_NAME/cctools &>/dev/null
  echo ""

  CONFFLAGS="--prefix=$TARGET_DIR --target=$(first_supported_arch)-apple-$TARGET "
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
TOOLS=($(find . -name "$(first_supported_arch)-apple-${TARGET}*"))
function create_arch_symlinks()
{
  local arch=$1
  local default_arch=$(first_supported_arch)
  # Target arch must not be the source arch. 
  if [ "$arch" = "$default_arch" ]; then
    return
  fi
  for TOOL in ${TOOLS[@]}; do
    verbose_cmd create_symlink $TOOL $(echo "$TOOL" | $SED "s/$(first_supported_arch)/$arch/g")
  done
}

if arch_supported x86_64; then
  create_arch_symlinks "x86_64"
fi

if arch_supported x86_64h; then
  create_arch_symlinks "x86_64h"
fi

if arch_supported i386; then
  create_arch_symlinks "i386"
fi

if arch_supported arm64; then
  create_arch_symlinks "aarch64"
  create_arch_symlinks "arm64"
fi

if arch_supported arm64e; then
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
export SUPPORTED_ARCHS
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

for ARCH in $SUPPORTED_ARCHS; do
  create_symlink osxcross-cmake "$TARGET_DIR/bin/$ARCH-apple-$TARGET-cmake"
done

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
  for ARCH in $SUPPORTED_ARCHS; do
    test_compiler_cxx11 $ARCH-apple-$TARGET-clang++ $BASE_DIR/oclang/test_libcxx.cpp
  done
fi

if [ $(osxcross-cmp $SDK_VERSION ">=" 13.3) -eq 1 ]; then
  CLANG_VERSION=$(echo "__clang_major__ __clang_minor__ __clang_patchlevel__" | \
                  xcrun clang -xc -E - | tail -n1 | tr ' ' '.')

  if [ $(osxcross-cmp $CLANG_VERSION ">=" 13.0) -eq 1 ]; then
    for ARCH in $SUPPORTED_ARCHS; do
      test_compiler_cxx2b $ARCH-apple-$TARGET-clang++ $BASE_DIR/oclang/test_libcxx_complex.cpp
    done
  else
    echo "Skipping complex c++20 test. Requires clang >= 13.0."
  fi
fi

# Loop through all supported architectures and test the compiler
# The first architecture in SUPPORTED_ARCHS must build successfully
for ARCH in $SUPPORTED_ARCHS; do
  if [ "$ARCH" = "$(first_supported_arch)" ]; then
    req="required"   # Must succeed
  else
    req=""           # May fail
  fi

  test_compiler $ARCH-apple-$TARGET-clang   $BASE_DIR/oclang/test.c   "$req"
  test_compiler $ARCH-apple-$TARGET-clang++ $BASE_DIR/oclang/test.cpp "$req"
done

echo ""
echo "Do not forget to add"
echo ""
echo -e "\x1B[32m${TARGET_DIR}/bin\x1B[0m"
echo ""
echo "to your PATH variable."
echo ""
echo "All done! OSXCross is set up now."
echo "Make sure to check out the README \"Usage Examples\" section for further instructions."
echo ""

if arch_supported arm64; then
  echo "!!! When dealing with Automake projects make sure to use aarch64-apple-$TARGET-* instead of arm64-* !!!"
  echo "!!! CC=aarch64-apple-$TARGET-clang ./configure --host=aarch64-apple-$TARGET !!!"
  echo ""
fi

if ! arch_supported i386; then
  echo "i386 is not supported by this SDK."
  echo "Use SDK version 10.13 or earlier if you need i386."
  echo ""
fi

#if ! arch_supported x86_64; then
#  echo "x86_64 is not supported by this SDK."
#  echo "Use SDK version 26 or earlier if you need x86_64."
#  echo ""
#fi

if [ $(osxcross-cmp $SDK_VERSION ">=" 10.14) -eq 1 ]; then
  echo "libstdc++ is not supported by this SDK."
  echo "Use SDK version 10.13 or earlier if you need libstdc++."
  echo ""
fi
