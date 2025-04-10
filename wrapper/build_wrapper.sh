#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null
pushd .. &>/dev/null
source ./tools/tools.sh
popd &>/dev/null

set +e
if [ -n "$VERSION" ]; then
  if [ -n "$SDK_VERSION" ]; then
    if [ -z "$ARM_SUPPORTED" ]; then
      if [ $(osxcross-cmp $SDK_VERSION ">=" 11.0) -eq 1 ]; then
        ARM_SUPPORTED=1
      else
        ARM_SUPPORTED=0
      fi
    fi
  fi
fi
set -e

if [ -z "$ARM_SUPPORTED" ]; then
  ARM_SUPPORTED=0
fi

function create_wrapper_link
{
  # arg 1:
  #  program name
  # arg 2:
  #  1: create a standalone link and links with the target triple prefix
  #  2: create links with target triple prefix and shortcut links such
  #     as o32, o64, ...
  #
  # example:
  #  create_wrapper_link osxcross 1
  # creates the following symlinks:
  #  -> osxcross
  #  -> x86_64-apple-darwinXX-osxcross

  if [ $# -ge 2 ] && [ $2 -eq 1 ]; then
    verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
      "${1}"
  fi

  verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
    "x86_64-apple-${TARGET}-${1}"

  if ([[ $1 != gcc* ]] && [[ $1 != g++* ]] && [[ $1 != *gstdc++ ]]); then
    if [ $ARM_SUPPORTED -eq 1 ]; then
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "aarch64-apple-${TARGET}-${1}"
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "arm64-apple-${TARGET}-${1}"
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "arm64e-apple-${TARGET}-${1}"
    fi
  fi

  if [ $# -ge 2 ] && [ $2 -eq 2 ]; then
    verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
      "o64-${1}"

    if [ $ARM_SUPPORTED -eq 1 ]; then
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "oa64-${1}"
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "oa64e-${1}"
    fi
  fi
}

[ -z "$TARGETCOMPILER" ] && TARGETCOMPILER=clang

TARGETTRIPLE=x86_64-apple-${TARGET}

FLAGS=""

if [ -n "$BWPLATFORM" ]; then
  PLATFORM=$BWPLATFORM

  if [ $PLATFORM = "Darwin" -a $(uname -s) != "Darwin" ]; then
    CXX=$(xcrun -f clang++)
    #CXX=$(xcrun -f g++)
    FLAGS+="-fvisibility-inlines-hidden "
  elif [ $PLATFORM = "FreeBSD" -a $(uname -s) != "FreeBSD" ]; then
    CXX=amd64-pc-freebsd13.0-clang++
  elif [ $PLATFORM = "NetBSD" -a $(uname -s) != "NetBSD" ]; then
    CXX=amd64-pc-netbsd6.1.3-clang++
  fi

  [ -z "$BWCOMPILEONLY" ] && BWCOMPILEONLY=1
else
  [ -z "$PORTABLE" ] && FLAGS="$CXXFLAGS "
fi

if [ -n "$BWCXX" ]; then
  [ "$CXX" != "$BWCXX" ] && echo "using $BWCXX" 1>&2
  CXX=$BWCXX
fi

if [ "$PLATFORM" == "Linux" ]; then
  FLAGS+="-isystem quirks/include "
fi

function compile_wrapper()
{
  mkdir -p ${TARGET_DIR}/bin
  export PLATFORM
  export CXX

  verbose_cmd $MAKE clean

  ADDITIONAL_CXXFLAGS="$FLAGS" \
    verbose_cmd $MAKE wrapper -j$JOBS
}

compile_wrapper

if [ -n "$BWCOMPILEONLY" ]; then
  exit 0
fi

verbose_cmd mkdir -p ${TARGET_DIR}/bin
verbose_cmd mv wrapper "${TARGET_DIR}/bin/${TARGETTRIPLE}-wrapper"

pushd "${TARGET_DIR}/bin" &>/dev/null

if [ $TARGETCOMPILER = "clang" ]; then
  create_wrapper_link clang 2
  create_wrapper_link clang++ 2
  create_wrapper_link clang++-libc++ 2
  create_wrapper_link clang++-stdc++ 2
  create_wrapper_link clang++-gstdc++ 2
elif [ $TARGETCOMPILER = "gcc" ]; then
  create_wrapper_link gcc 2
  create_wrapper_link g++ 2
  create_wrapper_link g++-libc++ 2
fi

create_wrapper_link cc
create_wrapper_link c++

create_wrapper_link dsymutil
create_wrapper_link ld
create_wrapper_link otool 1
create_wrapper_link lipo 1
create_wrapper_link nm
create_wrapper_link ar
create_wrapper_link libtool
create_wrapper_link ranlib
create_wrapper_link readtapi
create_wrapper_link objdump
create_wrapper_link strip
create_wrapper_link strings
create_wrapper_link size
create_wrapper_link symbolizer
create_wrapper_link cov
create_wrapper_link profdata
create_wrapper_link readobj
create_wrapper_link readelf
create_wrapper_link dwarfdump
create_wrapper_link cxxfilt
create_wrapper_link objcopy
create_wrapper_link config
create_wrapper_link as
create_wrapper_link dis
create_wrapper_link link
create_wrapper_link lto
create_wrapper_link lto2
create_wrapper_link bcanalyzer
create_wrapper_link bitcode-strip

create_wrapper_link osxcross 1
create_wrapper_link osxcross-conf 1
create_wrapper_link osxcross-env 1
create_wrapper_link osxcross-cmp 1
create_wrapper_link osxcross-man 1
create_wrapper_link pkg-config

create_wrapper_link sw_vers 1
create_wrapper_link xcrun 1
create_wrapper_link xcodebuild 1

popd &>/dev/null
popd &>/dev/null
