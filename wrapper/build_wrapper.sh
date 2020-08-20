#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null
pushd .. &>/dev/null
source ./tools/tools.sh
popd &>/dev/null

set +e
if [ -n "$VERSION" ]; then
  if [ -n "$SDK_VERSION" ]; then
    if [ -z "$X86_64H_SUPPORTED" ]; then
      if [ $(osxcross-cmp $SDK_VERSION ">=" 10.8) -eq 1 ]; then
        X86_64H_SUPPORTED=1
      else
        X86_64H_SUPPORTED=0
      fi
    fi
    if [ -z "$I386_SUPPORTED" ]; then
      if [ $(osxcross-cmp $SDK_VERSION "<=" 10.13) -eq 1 ]; then
        I386_SUPPORTED=1
      else
        I386_SUPPORTED=0
      fi
    fi
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

if [ -z "$I386_SUPPORTED" ]; then
  I386_SUPPORTED=1
fi

if [ -z "$X86_64H_SUPPORTED" ]; then
  X86_64H_SUPPORTED=0
fi

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
  #  -> i386-apple-darwinXX-osxcross
  #  -> x86_64-apple-darwinXX-osxcross
  #  -> x86_64h-apple-darwinXX-osxcross

  if [ $# -ge 2 ] && [ $2 -eq 1 ]; then
    verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
      "${1}"
  fi

  if [ $I386_SUPPORTED -eq 1 ]; then
    verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
      "i386-apple-${TARGET}-${1}"
  fi

  verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
    "x86_64-apple-${TARGET}-${1}"

  if ([[ $1 != gcc* ]] && [[ $1 != g++* ]] && [[ $1 != *gstdc++ ]]); then
    if [ $X86_64H_SUPPORTED -eq 1 ]; then
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "x86_64h-apple-${TARGET}-${1}"
    fi

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
    if [ $I386_SUPPORTED -eq 1 ]; then
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "o32-${1}"
    fi

    verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
      "o64-${1}"

    if [ $X86_64H_SUPPORTED -eq 1 ] &&
       ([[ $1 != gcc* ]] && [[ $1 != g++* ]] && [[ $1 != *gstdc++ ]]); then
      verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" \
        "o64h-${1}"
    fi

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

create_wrapper_link osxcross 1
create_wrapper_link osxcross-conf 1
create_wrapper_link osxcross-env 1
create_wrapper_link osxcross-cmp 1
create_wrapper_link osxcross-man 1
create_wrapper_link pkg-config

if [ "$PLATFORM" != "Darwin" ]; then
  create_wrapper_link sw_vers 1

  if which dsymutil &>/dev/null; then
    # If dsymutil is in PATH then it's most likely a recent
    # LLVM dsymutil version. In this case don't wrap it.
    # Just create target symlinks.

    verbose_cmd create_symlink $(which dsymutil) x86_64-apple-$TARGET-dsymutil

    if [ $I386_SUPPORTED -eq 1 ]; then
      verbose_cmd create_symlink $(which dsymutil) i386-apple-$TARGET-dsymutil
    fi
    if [ $X86_64H_SUPPORTED -eq 1 ]; then
      verbose_cmd create_symlink $(which dsymutil) x86_64h-apple-$TARGET-dsymutil
    fi
  else
    create_wrapper_link dsymutil 1
  fi

  create_wrapper_link xcrun 1
  create_wrapper_link xcodebuild 1
fi

popd &>/dev/null
popd &>/dev/null
