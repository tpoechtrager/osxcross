#!/usr/bin/env bash
#
# Build and install the cctools the SDK and macports.
#
# This script requires the OS X SDK and the Clang/LLVM compiler.
#

VERSION=3.0

# Stable build flavor: cctools and ld64 versions
STABLE_CCTOOLS_VERSION=986
STABLE_LINKER_VERSION=711

# Latest build flavor: cctools and ld64 versions
LATEST_CCTOOLS_VERSION=1030.6.3
LATEST_LINKER_VERSION=956.6

# LLVM build flavor: optional cctools lipo compatibility tool
LLVM_LIPO_VERSION=1010.6

pushd "${0%/*}" &>/dev/null

echo "OSXCross $VERSION"
echo "------------"
echo ""

source tools/tools.sh

if [ -z "$SDK_VERSION" ]; then
  guess_sdk_version
  SDK_VERSION=$guess_sdk_version_result
fi
set_and_verify_sdk_path

SDK_ARCHIVE=$SDK
SDK_ARCHIVE=${SDK_ARCHIVE#"$BASE_DIR"/}

case $SDK_VERSION in
  10.4*|10.5*)
    echo ""
    echo "SDK <= 10.5 no longer supported. Use 'osxcross-1.1' branch instead."
    exit 1
      ;;
esac

case $SDK_VERSION in
  10.6*)    TARGET=darwin10;   SUPPORTED_ARCHS="i386 x86_64";                 NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.7*)    TARGET=darwin11;   SUPPORTED_ARCHS="i386 x86_64";                 NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.8*)    TARGET=darwin12;   SUPPORTED_ARCHS="i386 x86_64 x86_64h";         NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.9*)    TARGET=darwin13;   SUPPORTED_ARCHS="i386 x86_64 x86_64h";         NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.10*)   TARGET=darwin14;   SUPPORTED_ARCHS="i386 x86_64 x86_64h";         NEED_TAPI_SUPPORT=0; OSX_VERSION_MIN_INT=10.6 ;;
  10.11*)   TARGET=darwin15;   SUPPORTED_ARCHS="i386 x86_64 x86_64h";         NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6 ;;
  10.12*)   TARGET=darwin16;   SUPPORTED_ARCHS="i386 x86_64 x86_64h";         NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6 ;;
  10.13*)   TARGET=darwin17;   SUPPORTED_ARCHS="i386 x86_64 x86_64h";         NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.6 ;;
  10.14*)   TARGET=darwin18;   SUPPORTED_ARCHS="x86_64 x86_64h";              NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  10.15*)   TARGET=darwin19;   SUPPORTED_ARCHS="x86_64 x86_64h";              NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  10.16*)   TARGET=darwin20;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;

  11|11.0*) TARGET=darwin20.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  11.1*)    TARGET=darwin20.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  11.2*)    TARGET=darwin20.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  11.3*)    TARGET=darwin20.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;

  12|12.0*) TARGET=darwin21.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.1*)    TARGET=darwin21.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.2*)    TARGET=darwin21.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.3*)    TARGET=darwin21.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  12.4*)    TARGET=darwin21.5; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;

  13|13.0*) TARGET=darwin22.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  13.1*)    TARGET=darwin22.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  13.2*)    TARGET=darwin22.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;
  13.3*)    TARGET=darwin22.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.9 ;;

  14|14.0*) TARGET=darwin23;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.1*)    TARGET=darwin23.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.2*)    TARGET=darwin23.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.3*)    TARGET=darwin23.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.4*)    TARGET=darwin23.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.5*)    TARGET=darwin23.5; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  14.6*)    TARGET=darwin23.6; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;

  15|15.0*) TARGET=darwin24;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.1*)    TARGET=darwin24.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.2*)    TARGET=darwin24.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.3*)    TARGET=darwin24.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.4*)    TARGET=darwin24.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  15.5*)    TARGET=darwin24.5; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;

  26|26.0*) TARGET=darwin25;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  26.1*)    TARGET=darwin25.1; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  26.2*)    TARGET=darwin25.2; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  26.3*)    TARGET=darwin25.3; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  26.4*)    TARGET=darwin25.4; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  26.5*)    TARGET=darwin25.5; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;
  26.6*)    TARGET=darwin25.6; SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=10.13 ;;

  27|27.0*) TARGET=darwin27;   SUPPORTED_ARCHS="arm64 arm64e x86_64 x86_64h"; NEED_TAPI_SUPPORT=1; OSX_VERSION_MIN_INT=11.0 ;;

  *)
    echo "Error: Unsupported SDK." 1>&2
    exit 1
    ;;
esac

# Minimum targeted macOS version
# Must be <= SDK_VERSION
if [ -n "$OSX_VERSION_MIN_INT" -a -z "$OSX_VERSION_MIN" ]; then
  OSX_VERSION_MIN=$OSX_VERSION_MIN_INT
fi

function select_build_flavor()
{
  local flavor_number

  echo "Select build flavor:"
  echo ""
  printf "  1) %-9s%s\n" "stable" "cctools $STABLE_CCTOOLS_VERSION, ld64 $STABLE_LINKER_VERSION"
  printf "  2) %-9s%s\n" "latest" "cctools $LATEST_CCTOOLS_VERSION, ld64 $LATEST_LINKER_VERSION"
  printf "  3) %-9s%s\n" "llvm" "LLVM tools, ld64.lld"
  echo ""

  if [ -n "$BUILD_FLAVOR" ]; then
    case "$BUILD_FLAVOR" in
      stable) flavor_number=1 ;;
      latest) flavor_number=2 ;;
      llvm) flavor_number=3 ;;
      *)
        echo "Invalid BUILD_FLAVOR: '$BUILD_FLAVOR'. Expected stable, latest or llvm." 1>&2
        exit 1
        ;;
    esac

    echo "Build flavor [1]: $flavor_number"
    if [ "$UNATTENDED" = "1" ]; then
      echo "UNATTENDED=1: selecting $BUILD_FLAVOR."
    else
      echo "BUILD_FLAVOR=$BUILD_FLAVOR: selecting $BUILD_FLAVOR."
    fi
    return
  fi

  while true; do
    read -r -p "Build flavor [1]: " response

    case "$response" in
      1|"")
        BUILD_FLAVOR=stable
        return
        ;;
      2)
        BUILD_FLAVOR=latest
        return
        ;;
      3)
        BUILD_FLAVOR=llvm
        return
        ;;
      *)
        echo "Invalid build flavor. Please enter 1, 2 or 3."
        ;;
    esac
  done
}

function configure_llvm_flavor()
{
  local filtered_archs=""
  local arch

  # The LLVM flavor uses ld64.lld, which currently supports only arm64, arm64e,
  # and x86_64 targets. Remove i386 and x86_64h before the architecture list is
  # used by subsequent build steps.
  for arch in $SUPPORTED_ARCHS; do
    case "$arch" in
      arm64|arm64e|x86_64) filtered_archs+=" $arch" ;;
    esac
  done
  SUPPORTED_ARCHS="${filtered_archs# }"

  unset NEED_TAPI_SUPPORT
}

if [ "$UNATTENDED" = "1" ] && [ -z "$BUILD_FLAVOR" ]; then
  BUILD_FLAVOR=stable
fi

select_build_flavor
echo ""

if [ "$BUILD_FLAVOR" = "llvm" ]; then
  configure_llvm_flavor
fi

if [ -n "$ENABLE_ARCHS" ]; then
  for arch in $ENABLE_ARCHS; do
    if ! arch_supported "$arch"; then
      echo "ENABLE_ARCHS: Architecture '$arch' is not supported by the '$BUILD_FLAVOR' flavor with SDK '$SDK_VERSION'" >&2
      exit 1
    fi
  done
  # trim + normalize whitespace
  SUPPORTED_ARCHS="$(echo $ENABLE_ARCHS | xargs)"
fi

export TARGET

echo "Configuration"
echo "-------------"
printf "%-26s: %s\n" "Build flavor" "$BUILD_FLAVOR"
printf "%-26s: macOS %s (%s)\n" "SDK" "$SDK_VERSION" "$SDK_ARCHIVE"
printf "%-26s: %s\n" "Target" "$TARGET"
printf "%-26s: %s\n" "Minimum deployment target" "$OSX_VERSION_MIN"
printf "%-26s: %s\n" "Architectures" "${SUPPORTED_ARCHS// /, }"

echo ""
echo "Directories"
echo "-----------"
printf "%-17s: %s\n" "Tarballs" "$TARBALL_DIR"
printf "%-17s: %s\n" "Build" "$BUILD_DIR"
printf "%-17s: %s\n" "Install" "$TARGET_DIR"
printf "%-17s: %s\n" "SDK install" "$SDK_DIR"  

if ! check_for_existing_osxcross_installation; then
  if [ -z "$UNATTENDED" ]; then
    echo ""
    read -r -p "Press enter to start building"
  fi
fi

export PATH=$TARGET_DIR/bin:$PATH

mkdir -p $BUILD_DIR
mkdir -p $TARGET_DIR/bin
mkdir -p $SDK_DIR

source $BASE_DIR/tools/trap_exit.sh

pushd $BUILD_DIR &>/dev/null



function build_stable()
{
  # XAR

  build_xar

  # XAR END

  ## Apple TAPI Library ##

  if [ $NEED_TAPI_SUPPORT -eq 1 ]; then
    require $CMAKE

    get_sources https://github.com/tpoechtrager/apple-libtapi.git 1300.6.5

    if [ $f_res -eq 1 ]; then
      pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
      INSTALLPREFIX=$TARGET_DIR ./build.sh
      ./install.sh
      popd &>/dev/null
    fi
  fi

  ## cctools and ld64 ##

  CCTOOLS_VERSION=$STABLE_CCTOOLS_VERSION
  LINKER_VERSION=$STABLE_LINKER_VERSION

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
}

function build_latest()
{
  # XAR

  build_xar

  # XAR END

  if [ $NEED_TAPI_SUPPORT -eq 1 ]; then
    ## Apple Dispatch/Blocks library ##
    require $CMAKE

    get_sources https://github.com/tpoechtrager/apple-libdispatch.git main

    if [ $f_res -eq 1 ]; then
      pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
      mkdir -p build
      pushd build &>/dev/null
      $CMAKE .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=$TARGET_DIR
      $MAKE install -j$JOBS
      popd &>/dev/null
      popd &>/dev/null
    fi

    ## Apple TAPI Library ##

    if ! arch_supported x86_64h; then
      # https://github.com/tpoechtrager/apple-libtapi/issues/32#issuecomment-2870102119
      TAPI_VERSION=1600.0.11.8
    else
      TAPI_VERSION=1300.6.5
    fi

    get_sources https://github.com/tpoechtrager/apple-libtapi.git $TAPI_VERSION

    if [ $f_res -eq 1 ]; then
      pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
      INSTALLPREFIX=$TARGET_DIR ./build.sh
      ./install.sh
      popd &>/dev/null
    fi
  fi

  ## cctools and ld64 ##

  CCTOOLS_VERSION=$LATEST_CCTOOLS_VERSION
  LINKER_VERSION=$LATEST_LINKER_VERSION

  get_sources \
    https://github.com/tpoechtrager/cctools-port.git \
    $CCTOOLS_VERSION-ld64-$LINKER_VERSION

  if [ $f_res -eq 1 ]; then
    pushd $CURRENT_BUILD_PROJECT_NAME/cctools &>/dev/null
    echo ""

    CONFFLAGS="--prefix=$TARGET_DIR --target=$(first_supported_arch)-apple-$TARGET "
    if [ $NEED_TAPI_SUPPORT -eq 1 ]; then
      CONFFLAGS+="--with-libdispatch=$TARGET_DIR "
      CONFFLAGS+="--with-libblocksruntime=$TARGET_DIR "
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
}

function build_llvm()
{
  # The LLVM flavor uses the host LLVM tools and ld64.lld directly.
  LINKER_VERSION=
  LIBLTO_PATH=

  require ld64.lld

  if [ -z "${ENABLE_REPLACEMENT_LIPO+x}" ]; then
    echo ""
    echo "LLVM flavor compatibility"
    echo "-------------------------"
    echo ""

    message=$'Use cctools lipo instead of llvm-lipo to improve compatibility?'
    message+=$'\nYou can still use llvm-lipo at runtime by setting OSXCROSS_FORCE_LLVM_LIPO=1.'

    if [ "$UNATTENDED" = "1" ]; then
      echo "$message"
      ENABLE_REPLACEMENT_LIPO=1
      echo "UNATTENDED=1: automatically selecting cctools lipo."
    else
      if prompt "$message"; then
        ENABLE_REPLACEMENT_LIPO=1
      else
        ENABLE_REPLACEMENT_LIPO=0
      fi
    fi
  fi

  case "$ENABLE_REPLACEMENT_LIPO" in
    0)
      echo "Using llvm-lipo ..."
      ;;
    1)
      echo "Enabling cctools lipo ..."
      get_sources \
        https://github.com/tpoechtrager/cctools-port.git \
        lipo-$LLVM_LIPO_VERSION

      if [ $f_res -eq 1 ]; then
        pushd $CURRENT_BUILD_PROJECT_NAME/lipo &>/dev/null
        echo ""
        ./configure
        $MAKE -j$JOBS
        cp misc/lipo $TARGET_DIR/bin/osxcross-cctools-lipo
        popd &>/dev/null
      fi
      ;;
    *)
      echo "ENABLE_REPLACEMENT_LIPO must be 0 or 1" >&2
      exit 1
      ;;
  esac
}

case "$BUILD_FLAVOR" in
  stable) build_stable ;;
  latest) build_latest ;;
  llvm) build_llvm ;;
esac

## Create Arch Symlinks ##

if [ "$BUILD_FLAVOR" == "llvm" ]; then
  # LLVM tools are invoked by the wrapper. The necessary symlinks are created by
  # wrapper/build_wrapper.sh.
  true
else
  function create_cctools_symlinks()
  {
    pushd $TARGET_DIR/bin &>/dev/null

    # GCC installs a separate backend for each target architecture. In particular,
    # arm64 base-gcc/base-g++ already point to GCC's aarch64 backends; creating the
    # reverse aliases here would form a symlink loop.
    TOOLS=($(find . -name "$(first_supported_arch)-apple-${TARGET}*" \
      ! -name "*-base-gcc" ! -name "*-base-g++"))

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

    for supported_arch in $SUPPORTED_ARCHS; do
      # Create aarch64 aliases for arm64.
      if [ "$supported_arch" = "arm64" ]; then
        create_arch_symlinks "aarch64"
      fi

      create_arch_symlinks "$supported_arch"
    done

    # LLVM dsymutil invokes "lipo" directly, even in recent releases such as 22.1.8.
    # Provide the osxcross host-lipo wrapper under that name.
    create_symlink "$(first_supported_arch)-apple-$TARGET-lipo" lipo

    popd &>/dev/null
  }

  create_cctools_symlinks

fi


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

if [ $(cmp-version $SDK_VERSION ">=" 27) -eq 1 ]; then
  # SDK 27 libc++ may miss NAN and INFINITY with Clang < 22.
  # The patch is a no-op for newer Clang versions.
  echo "SDK needs patching for libc++ math.h issue ..."
  patch -N -p1 -r /dev/null < $PATCH_DIR/libcxx_math_h.patch || true
fi

set -e
popd &>/dev/null

popd &>/dev/null

## Wrapper ##

build_msg "wrapper"

OSXCROSS_CONF="$TARGET_DIR/bin/osxcross-conf"
OSXCROSS_ENV="$TARGET_DIR/bin/osxcross-env"
rm -f $OSXCROSS_CONF $OSXCROSS_ENV

if [ "$BUILD_FLAVOR" = "llvm" ]; then
  LIBLTO_PATH=
  LINKER_VERSION=
elif [ "$PLATFORM" != "Darwin" ]; then
  # libLTO.so
  set +e
  eval $(cat $BUILD_DIR/cctools*/cctools/config.log | grep LLVM_LIB_DIR | head -n1)
  set -e
  export LIBLTO_PATH=$LLVM_LIB_DIR
fi

export VERSION
export BUILD_FLAVOR
export TARGET
export BUILD_DIR
export OSX_VERSION_MIN
export LIBLTO_PATH
export LINKER_VERSION
export SUPPORTED_ARCHS
export TOP_BUILD_SCRIPT=1

$BASE_DIR/wrapper/build_wrapper.sh

echo ""

if [ $(cmp-version $SDK_VERSION "<" $OSX_VERSION_MIN) -eq 1 ]; then
  echo "OSX_VERSION_MIN must be <= SDK_VERSION"
  trap "" EXIT
  exit 1
elif [ $(cmp-version $OSX_VERSION_MIN "<" 10.6) -eq 1  ]; then
  echo "OSX_VERSION_MIN must be >= 10.6"
  trap "" EXIT
  exit 1
fi

## CMake ##

install_cmake_toolchain_files clang $SUPPORTED_ARCHS

## Compiler test ##

unset MACOSX_DEPLOYMENT_TARGET

if [ $(cmp-version $SDK_VERSION ">=" 10.7) -eq 1 ]; then
  pushd $SDK_DIR/MacOSX$SDK_VERSION.sdk &>/dev/null
  if [ ! -f "usr/include/c++/v1/vector" ]; then
    echo ""
    echo -n "Given SDK does not contain libc++ headers "
    echo "(-stdlib=libc++ test may fail)"
    echo -n "You may want to re-package your SDK using "
    echo "'tools/gen_sdk_package.sh' on macOS"
  fi
  if [ -f "usr/include/c++/v1/__hash_table" ]; then
    if [ $(cmp-version $SDK_VERSION ">=" 10.7) -eq 1 ]; then
    if [ $(cmp-version $SDK_VERSION "<=" 10.12) -eq 1 ]; then
      # https://github.com/tpoechtrager/osxcross/issues/171
      echo "SDK needs patching for libc++ hash table issue ..."
      patch -N -p1 -r /dev/null < $PATCH_DIR/libcxx__hash_table.patch || true
    fi
    fi
  fi
  if [ -f "usr/include/c++/v1/typeinfo" ]; then
    if [ $(cmp-version "$SDK_VERSION" ">=" 10.7) -eq 1 ]; then
    if [ $(cmp-version "$SDK_VERSION" "<=" 10.8) -eq 1 ]; then
      echo "SDK needs patching for libc++ typeinfo issue ..."
      sed_expr='s/_ATTRIBUTE(noreturn) friend void rethrow_exception(exception_ptr);/'
      sed_expr+='friend void rethrow_exception(exception_ptr);/g'
      $SED -i "$sed_expr" usr/include/c++/v1/exception
    fi
    fi
  fi
  if [ -f "usr/include/Availability.h" ]; then
    if [ $(cmp-version $SDK_VERSION "==" 10.15) -eq 1 ]; then
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

if [ $(cmp-version $SDK_VERSION ">=" 13.3) -eq 1 ]; then
  CLANG_VERSION=$(echo "__clang_major__ __clang_minor__ __clang_patchlevel__" | \
                  xcrun clang -xc -E - | tail -n1 | tr ' ' '.')

  if [ $(cmp-version $CLANG_VERSION ">=" 13.0) -eq 1 ]; then
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
echo "OSXCross was built for: $SUPPORTED_ARCHS"
echo "Done! OSXCross is set up now."
echo ""
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
#  echo "Use SDK version 27 or earlier if you need x86_64."
#  echo ""
#fi

if [ $(cmp-version $SDK_VERSION ">=" 10.14) -eq 1 ]; then
  echo "libstdc++ is not supported by this SDK."
  echo "Use SDK version 10.13 or earlier if you need libstdc++."
  echo ""
fi
