#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null
pushd .. &>/dev/null
source ./tools/tools.sh
popd &>/dev/null

EXESUFFIX=""

function create_wrapper_link
{
  verbose_cmd ln -sf "${TARGETTRIPLE}-wrapper${EXESUFFIX}" "${1}${EXESUFFIX}"
}

[ -z "$TARGET" ] && TARGET=darwin12
[ -z "$OSX_VERSION_MIN" ] && OSX_VERSION_MIN=10.5
[ -z "$LINKER_VERSION" ] && LINKER_VERSION=134.9
[ -z "$TARGETCOMPILER" ] && TARGETCOMPILER=clang

TARGETTRIPLE=x86_64-apple-$TARGET

FLAGS=""

if [ -n "$BWPLATFORM" ]; then
  PLATFORM=$BWPLATFORM

  if [ $PLATFORM = "Darwin" -a $(uname -s) != "Darwin" ]; then
    CXX=o32-clang++
  elif [ $PLATFORM = "FreeBSD" -a $(uname -s) != "FreeBSD" ]; then
    CXX=amd64-pc-freebsd10.0-clang++
    FLAGS+="-lrt "
  elif [ $PLATFORM = "Windows" ]; then
    CXX=w32-clang++
    FLAGS+="-wc-static-runtime -g "
    EXESUFFIX=".exe"
  elif [ $PLATFORM = "MWindows" ]; then
    CXX=i686-w64-mingw32-g++
    FLAGS+="-static-libgcc -static-libstdc++ -g "
    EXESUFFIX=".exe"
  fi
else
  PLATFORM=$(uname -s)
  FLAGS="-march=native "
fi

if [ -n "$BWCXX" ]; then
  [ "$CXX" != "$BWCXX" ] && echo "using $BWCXX" 1>&2
  CXX=$BWCXX
fi

[ $PLATFORM = "Darwin" ] && FLAGS+="-framework CoreServices "
[ $PLATFORM = "FreeBSD" ] && FLAGS+="-lutil "

if [[ $PLATFORM != *Windows ]] && [ $PLATFORM != "Darwin" ]; then
  FLAGS+="-lrt "
fi

function compile_wrapper()
{
  mkdir -p ../target ../target/bin

  verbose_cmd $CXX compiler.cpp -std=c++0x -pedantic -Wall -Wextra \
    "-DOSXCROSS_TARGET=\"\\\"$TARGET\\\"\"" \
    "-DOSXCROSS_OSX_VERSION_MIN=\"\\\"$OSX_VERSION_MIN\\\"\"" \
    "-DOSXCROSS_LINKER_VERSION=\"\\\"$LINKER_VERSION\\\"\"" \
    -o "../target/bin/${TARGETTRIPLE}-wrapper${EXESUFFIX}" -O2 \
    $FLAGS $*
}

compile_wrapper

pushd "../target/bin" &>/dev/null

if [ $TARGETCOMPILER = "clang" ]; then
  create_wrapper_link o32-clang
  create_wrapper_link o32-clang++
  create_wrapper_link o32-clang++-libc++

  create_wrapper_link o64-clang
  create_wrapper_link o64-clang++
  create_wrapper_link o64-clang++-libc++

  create_wrapper_link i386-apple-$TARGET-clang
  create_wrapper_link i386-apple-$TARGET-clang++
  create_wrapper_link i386-apple-$TARGET-clang++-libc++

  create_wrapper_link x86_64-apple-$TARGET-clang
  create_wrapper_link x86_64-apple-$TARGET-clang++
  create_wrapper_link x86_64-apple-$TARGET-clang++-libc++
elif [ $TARGETCOMPILER = "gcc" ]; then
  create_wrapper_link o32-gcc
  create_wrapper_link o32-g++
  create_wrapper_link o32-g++-libc++

  create_wrapper_link o64-gcc
  create_wrapper_link o64-g++
  create_wrapper_link o64-g++-libc++

  create_wrapper_link i386-apple-$TARGET-gcc
  create_wrapper_link i386-apple-$TARGET-g++
  create_wrapper_link i386-apple-$TARGET-g++-libc++

  create_wrapper_link x86_64-apple-$TARGET-gcc
  create_wrapper_link x86_64-apple-$TARGET-g++
  create_wrapper_link x86_64-apple-$TARGET-g++-libc++
fi

create_wrapper_link i386-apple-$TARGET-cc
create_wrapper_link i386-apple-$TARGET-c++

create_wrapper_link x86_64-apple-$TARGET-cc
create_wrapper_link x86_64-apple-$TARGET-c++

create_wrapper_link osxcross-conf
create_wrapper_link osxcross-env

create_wrapper_link sw_vers
create_wrapper_link i386-apple-$TARGET-sw_vers
create_wrapper_link x86_64-apple-$TARGET-sw_vers

create_wrapper_link dsymutil
create_wrapper_link i386-apple-$TARGET-dsymutil
create_wrapper_link x86_64-apple-$TARGET-dsymutil

popd &>/dev/null
popd &>/dev/null
