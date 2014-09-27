#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null
pushd .. &>/dev/null
source ./tools/tools.sh
popd &>/dev/null

set +e
if [ -z "$OSXCROSS_VERSION" ]; then
  `../target/bin/osxcross-conf 2>/dev/null`
fi
set -e

EXESUFFIX=""

function create_wrapper_link
{
  # arg 2:
  # 1: Create a standalone link and links with target triple prefix
  # 2: Create links with target triple prefix and shorcut links such as o32, o64, ...

  if [ $# -ge 2 ] && [ $2 -eq 1 ]; then
    verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "${1}${EXESUFFIX}"
  fi

  verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "i386-apple-${OSXCROSS_TARGET}-${1}${EXESUFFIX}"
  verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "x86_64-apple-${OSXCROSS_TARGET}-${1}${EXESUFFIX}"

  if [[ $1 == *clang* ]] || ([ $# -ge 3 ] && [ $3 -eq 1 ]); then
    # Do not create Haswell links for gcc
    verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "x86_64h-apple-${OSXCROSS_TARGET}-${1}${EXESUFFIX}"
  fi

  if [ $# -ge 2 ] && [ $2 -eq 2 ]; then
    verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "o32-${1}${EXESUFFIX}"
    verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "o64-${1}${EXESUFFIX}"

    if [[ $1 == *clang* ]]; then
      # Do not create Haswell links for gcc
      verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "o64h-${1}${EXESUFFIX}"
    fi
  fi
}

[ -z "$TARGETCOMPILER" ] && TARGETCOMPILER=clang

TARGETTRIPLE=x86_64-apple-${OSXCROSS_TARGET}

FLAGS=""

if [ -n "$BWPLATFORM" ]; then
  PLATFORM=$BWPLATFORM

  if [ $PLATFORM = "Darwin" -a $(uname -s) != "Darwin" ]; then
    CXX=o32-clang++
    #CXX=o32-g++
    FLAGS+="-fvisibility-inlines-hidden "
  elif [ $PLATFORM = "FreeBSD" -a $(uname -s) != "FreeBSD" ]; then
    CXX=amd64-pc-freebsd10.0-clang++
    #CXX=amd64-pc-freebsd10.0-g++
  elif [ $PLATFORM = "NetBSD" -a $(uname -s) != "NetBSD" ]; then
    CXX=amd64-pc-netbsd6.1.3-clang++
    #CXX=amd64-pc-netbsd6.1.3-g++
  elif [ $PLATFORM = "Windows" ]; then
    CXX=w32-clang++
    FLAGS+="-wc-static-runtime -g "
    EXESUFFIX=".exe"
  elif [ $PLATFORM = "MWindows" ]; then
    CXX=i686-w64-mingw32-g++
    FLAGS+="-static-libgcc -static-libstdc++ -g "
    EXESUFFIX=".exe"
  fi

  [ -z "$BWCOMPILEONLY" ] && BWCOMPILEONLY=1
else
  PLATFORM=$(uname -s)
  FLAGS="-march=native $CXXFLAGS "
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
  mkdir -p ../target ../target/bin
  export PLATFORM
  export CXX

  verbose_cmd $MAKE clean

  OSXCROSS_CXXFLAGS="$FLAGS" \
    verbose_cmd $MAKE wrapper -j$JOBS
}

compile_wrapper

if [ -n "$BWCOMPILEONLY" ]; then
  exit 0
fi

verbose_cmd mv wrapper "../target/bin/${TARGETTRIPLE}-wrapper${EXESUFFIX}"

pushd "../target/bin" &>/dev/null

if [ $TARGETCOMPILER = "clang" ]; then
  create_wrapper_link clang 2
  create_wrapper_link clang++ 2
  create_wrapper_link clang++-libc++ 2
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
create_wrapper_link pkg-config 0 1

if [ "$PLATFORM" != "Darwin" ]; then
  create_wrapper_link sw_vers 1
  create_wrapper_link dsymutil 1
fi

popd &>/dev/null
popd &>/dev/null
