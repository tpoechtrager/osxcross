#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null

source tools/tools.sh
eval $(tools/osxcross_conf.sh)

if [ $PLATFORM == "Darwin" ]; then
  exit 0
fi

require git

set +e

which xcrun &>/dev/null

# Clang <= 3.4 doesn't like '-arch x86_64h'

if [ $? -ne 0 ] ||
   [[ $(xcrun clang -arch x86_64h --version 2>/dev/null) \
      == *Target:\ x86_64-* ]];
then
  echo "Please re-run ./build.sh" 1>&2
  exit 1
fi

set -e

CLANG_VERSION=$(echo "__clang_major__ __clang_minor__ __clang_patchlevel__" | \
 clang -xc -E - | tail -n1 | tr ' ' '.')

# Drop patch level for <= 3.3.
if [ $(osxcross-cmp $CLANG_VERSION "<=" 3.3) -eq 1 ]; then
  CLANG_VERSION=$(echo $CLANG_VERSION | tr '.' ' ' |
                  awk '{print $1, $2}' | tr ' ' '.')
fi

CLANG_LIB_DIR=$(clang -print-search-dirs | grep "libraries: =" | \
                tr '=' ' ' | tr ':' ' ' | awk '{print $2}')

VERSION=$(echo "${CLANG_LIB_DIR}" | tr '/' '\n' | tail -n1)

if [ $VERSION != $CLANG_VERSION ]; then
  echo "sanity check failed: $VERSION != ${CLANG_VERSION}" 1>&2
  exit 1
fi

CLANG_INCLUDE_DIR="${CLANG_LIB_DIR}/include"
CLANG_DARWIN_LIB_DIR="${CLANG_LIB_DIR}/lib/darwin"

case $CLANG_VERSION in
  3.2*) BRANCH=release_32 ;;
  3.3*) BRANCH=release_33 ;;
  3.4*) BRANCH=release_34 ;;
  3.5*) BRANCH=release_35 ;;
  3.6*) BRANCH=release_36 ;;
  3.7*) BRANCH=release_37 ;;
  3.8*) BRANCH=release_38 ;;
  3.9*) BRANCH=master ;;
  * ) echo "Unsupported Clang version, must be >= 3.2 and <= 3.9" 1>&2; exit 1;
esac

pushd $OSXCROSS_BUILD_DIR &>/dev/null

if [ ! -e compiler-rt/.clone_complete ]; then
  rm -rf compiler-rt
  git clone http://llvm.org/git/compiler-rt.git
fi

pushd compiler-rt &>/dev/null

git reset --hard

git checkout $BRANCH
git clean -fdx
touch .clone_complete
git pull

if [ $BRANCH == "release_38" ]; then
  patch -p0 < $PATCH_DIR/compiler-rt-llvm38-makefile.patch
fi

$SED -i "s/Configs += ios//g" make/platform/clang_darwin.mk
$SED -i "s/Configs += cc_kext_ios5//g" make/platform/clang_darwin.mk
$SED -i "s/Configs += profile_ios//g" make/platform/clang_darwin.mk
$SED -i "s/Configs += asan_iossim_dynamic//g" make/platform/clang_darwin.mk

# Unbreak the -Werror build.
if [ -f lib/asan/asan_mac.h ]; then
  $SED -i "s/ASAN__MAC_H/ASAN_MAC_H/g" lib/asan/asan_mac.h
fi

if [ $(osxcross-cmp $CLANG_VERSION ">=" 3.5) -eq 1 ]; then
  export MACOSX_DEPLOYMENT_TARGET=10.7
else
  export MACOSX_DEPLOYMENT_TARGET=10.4
fi

if [ $(osxcross-cmp $MACOSX_DEPLOYMENT_TARGET ">" \
                    $OSXCROSS_SDK_VERSION) -eq 1 ];
then
  echo ">= $MACOSX_DEPLOYMENT_TARGET SDK required" 1>&2
  exit 1
fi

EXTRA_MAKE_FLAGS="LIPO=\"$(xcrun -f lipo)\""

if [ $(osxcross-cmp $CLANG_VERSION "<=" 3.3) -eq 1 ]; then
  EXTRA_MAKE_FLAGS+=" AR=\"$(xcrun -f ar)\""
  EXTRA_MAKE_FLAGS+=" RANLIB=\"$(xcrun -f ranlib)\""
  EXTRA_MAKE_FLAGS+=" CC=\"$(xcrun -f clang)\""
fi

if [ -n "$OCDEBUG" ]; then
  EXTRA_MAKE_FLAGS+=" VERBOSE=1"
fi

# Must eval here because of the spaces in EXTRA_MAKE_FLAGS.

eval \
  "OSXCROSS_NO_X86_64H_DEPLOYMENT_TARGET_WARNING=1 \
   $MAKE clang_darwin $EXTRA_MAKE_FLAGS -j $JOBS"

echo ""
echo ""
echo ""
echo "Please run the following commands by hand to install compiler-rt:"
echo ""

echo "mkdir -p ${CLANG_INCLUDE_DIR}"
echo "mkdir -p ${CLANG_DARWIN_LIB_DIR}"
echo "cp -r $PWD/include/sanitizer ${CLANG_INCLUDE_DIR}"

pushd "clang_darwin" &>/dev/null

function print_install_command() {
  if [ -f "$1" ]; then
    echo "cp $PWD/$1 ${CLANG_DARWIN_LIB_DIR}/$2"
  fi
}

print_install_command "osx/libcompiler_rt.a"         "libclang_rt.osx.a"
print_install_command "10.4/libcompiler_rt.a"        "libclang_rt.10.4.a"
print_install_command "eprintf/libcompiler_rt.a"     "libclang_rt.eprintf.a"
print_install_command "cc_kext/libcompiler_rt.a"     "libclang_rt.cc_kext.a"
print_install_command "profile_osx/libcompiler_rt.a" "libclang_rt.profile_osx.a"

print_install_command "ubsan_osx_dynamic/libcompiler_rt.dylib" \
  "libclang_rt.ubsan_osx_dynamic.dylib"

print_install_command "asan_osx_dynamic/libcompiler_rt.dylib" \
  "libclang_rt.asan_osx_dynamic.dylib"

echo ""
