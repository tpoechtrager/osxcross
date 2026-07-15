#!/usr/bin/env bash
#
# Build and install gcc/gcc++ as a cross-compiler with target OSX.
#
# You may want to run this script if you want to build software using
# gcc. Please refer to the README.md for details.
#

pushd "${0%/*}" &>/dev/null

unset LIBRARY_PATH

DESC=gcc
USESYSTEMCOMPILER=1
source tools/tools.sh

GCC_SOURCE_DIR=""

if [ "${0##*/}" = "build_gcc_with_arm64_support.sh" ]; then
  # Build a patched GCC that supports both x86_64 and arm64 targets.
  # https://github.com/iains/gcc-darwin-arm64

  if [ -z "$ARM64_GCC_REPO" ]; then
    # Build the 16 branch by default.
    ARM64_GCC_REPO="gcc-16-branch"
  fi

  BUILD_ARM64_GCC=1
  GCC_TARGET_ARCHS="aarch64 x86_64 i386"
else
  # GCC version to build
  # (<4.7 will not work properly with libc++)
  if [ -z "$GCC_VERSION" ]; then
    GCC_VERSION=16.1.0
    #GCC_VERSION=5-20200228 # snapshot
  fi

  # GCC mirror
  GCC_MIRROR="https://mirrorservice.org/sites/sourceware.org/pub/gcc"
  GCC_TARGET_ARCHS="x86_64 i386"
fi

# Export the GCC target list for wrapper/build_wrapper.sh.
export GCC_TARGET_ARCHS


if [ $(osxcross-cmp $OSX_VERSION_MIN '<=' 10.5) -eq 1 ]; then
  echo "You must build OSXCross with OSX_VERSION_MIN >= 10.6" 2>&1
  exit 1
fi

# Remove unsupported target architectures from the list of GCC target architectures.
function filter_supported_gcc_target_archs()
{
  local supported_gcc_target_archs=""
  local gcc_target_arch

  for gcc_target_arch in $GCC_TARGET_ARCHS; do
    if ! arch_supported "$gcc_target_arch"; then
      # Do not warn about i386 on SDKs that no longer support it.
      if [ "$gcc_target_arch" = "i386" ] &&
         [ "$(osxcross-cmp "$SDK_VERSION" '>' 10.13)" -eq 1 ]; then
        continue
      fi

      echo "Warning target $gcc_target_arch is not supported or not enabled; skipping $gcc_target_arch." 1>&2
      sleep 2
      continue
    fi

    supported_gcc_target_archs="${supported_gcc_target_archs:+$supported_gcc_target_archs }$gcc_target_arch"
  done

  GCC_TARGET_ARCHS=$supported_gcc_target_archs

  if [ -z "$GCC_TARGET_ARCHS" ]; then
    echo "No supported GCC target architectures found." 1>&2
    exit 1
  fi
}

filter_supported_gcc_target_archs

GCC_BUILD_ARCHS=${GCC_TARGET_ARCHS//aarch64/arm64}
GCC_BUILD_ARCHS=${GCC_BUILD_ARCHS// /_}

if arch_supported "$GCC_TARGET_ARCHS" aarch64; then
  # Ensure that the ARM64 GCC tools are available for use by GCC.
  function ensure_arm64_gcc_tools()
  {
    local arm64_ranlib="arm64-apple-$TARGET-ranlib"
    local aarch64_ranlib="aarch64-apple-$TARGET-ranlib"
    local aarch64_dsymutil="aarch64-apple-$TARGET-dsymutil"
    local dsymutil_path

    pushd "$TARGET_DIR/bin" &>/dev/null

    if [ ! -e "$aarch64_ranlib" ]; then
      if [ ! -e "$arm64_ranlib" ]; then
        echo "Missing ARM64 ranlib: '$arm64_ranlib'" 1>&2
        exit 1
      fi
      create_symlink "./$arm64_ranlib" "$aarch64_ranlib"
    fi

    if [ ! -e "$aarch64_dsymutil" ]; then
      dsymutil_path=$(which dsymutil) || {
        echo "Required dependency 'dsymutil' not installed or not in PATH" 1>&2
        exit 1
      }
      create_symlink "$dsymutil_path" "$aarch64_dsymutil"
    fi

    popd &>/dev/null
  }

  ensure_arm64_gcc_tools
fi


pushd $BUILD_DIR &>/dev/null


source $BASE_DIR/tools/trap_exit.sh

if [ -n "$BUILD_ARM64_GCC" ]; then
  get_sources https://github.com/iains/$ARM64_GCC_REPO.git
  GCC_SOURCE_DIR=$CURRENT_BUILD_PROJECT_NAME
  GCC_VERSION=$(cat "$GCC_SOURCE_DIR/gcc/BASE-VER")
  echo ""
  echo "GCC version: ${GCC_VERSION}"
  echo ""
else
  f_res=1
fi

if [ $f_res -eq 1 ]; then

if [ -z "$BUILD_ARM64_GCC" ]; then
pushd $TARBALL_DIR &>/dev/null
if [[ $GCC_VERSION != *-* ]]; then
  download "$GCC_MIRROR/releases/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
else
  download "$GCC_MIRROR/snapshots/$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi
popd &>/dev/null

echo "cleaning up ..."
rm -rf "gcc-$GCC_VERSION" 2>/dev/null

extract "$TARBALL_DIR/gcc-$GCC_VERSION.tar.xz"
echo ""

GCC_SOURCE_DIR=gcc-$GCC_VERSION
fi

pushd "$GCC_SOURCE_DIR" &>/dev/null

if arch_supported "$GCC_TARGET_ARCHS" aarch64; then
  rm -f "$TARGET_DIR/bin/aarch64-apple-$TARGET-gcc"*
  rm -f "$TARGET_DIR/bin/aarch64-apple-$TARGET-g++"*
  rm -f "$TARGET_DIR/bin/aarch64-apple-$TARGET-base-gcc"*
  rm -f "$TARGET_DIR/bin/aarch64-apple-$TARGET-base-g++"*
  rm -f "$TARGET_DIR/bin/arm64-apple-$TARGET-gcc"*
  rm -f "$TARGET_DIR/bin/arm64-apple-$TARGET-g++"*
  rm -f "$TARGET_DIR/bin/arm64-apple-$TARGET-base-gcc"*
  rm -f "$TARGET_DIR/bin/arm64-apple-$TARGET-base-g++"*
  rm -f "$TARGET_DIR/bin/oa64-gcc"*
  rm -f "$TARGET_DIR/bin/oa64-g++"*
fi

if arch_supported "$GCC_TARGET_ARCHS" x86_64; then
  rm -f "$TARGET_DIR/bin/x86_64-apple-$TARGET-gcc"*
  rm -f "$TARGET_DIR/bin/x86_64-apple-$TARGET-g++"*
  rm -f "$TARGET_DIR/bin/x86_64-apple-$TARGET-base-gcc"*
  rm -f "$TARGET_DIR/bin/x86_64-apple-$TARGET-base-g++"*
  rm -f "$TARGET_DIR/bin/o64-gcc"*
  rm -f "$TARGET_DIR/bin/o64-g++"*
fi

if arch_supported "$GCC_TARGET_ARCHS" i386; then
  rm -f "$TARGET_DIR/bin/i386-apple-$TARGET-gcc"*
  rm -f "$TARGET_DIR/bin/i386-apple-$TARGET-g++"*
  rm -f "$TARGET_DIR/bin/i386-apple-$TARGET-base-gcc"*
  rm -f "$TARGET_DIR/bin/i386-apple-$TARGET-base-g++"*
  rm -f "$TARGET_DIR/bin/o32-gcc"*
  rm -f "$TARGET_DIR/bin/o32-g++"*
fi


if [ $(osxcross-cmp $GCC_VERSION '>' 5.0.0) -eq 1 ] &&
   [ $(osxcross-cmp $GCC_VERSION '<' 5.3.0) -eq 1 ]; then
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66035
  patch -p1 < $PATCH_DIR/gcc-pr66035.patch
fi

if [ $(osxcross-cmp $GCC_VERSION '>=' 6.1.0) -eq 1 ] &&
   [ $(osxcross-cmp $GCC_VERSION '<=' 6.3.0) -eq 1 ]; then
  # https://gcc.gnu.org/ml/gcc-patches/2016-09/msg00129.html
  patch -p1 < $PATCH_DIR/gcc-6-buildfix.patch
fi

if [ $(osxcross-cmp $GCC_VERSION '==' 6.3.0) -eq 1 ]; then
  # https://gcc.gnu.org/viewcvs/gcc/trunk/gcc/config/darwin-driver.c?r1=244010&r2=244009&pathrev=244010
  patch -p1 < $PATCH_DIR/darwin-driver.c.patch
fi

if [ $(osxcross-cmp $SDK_VERSION '>=' 10.14) -eq 1 ] &&
   [ $(osxcross-cmp $GCC_VERSION '<' 9.0.0) -eq 1 ]; then
  files_to_patch=(
    libsanitizer/asan/asan_mac.cc
    libsanitizer/sanitizer_common/sanitizer_platform_limits_posix.cc
    libsanitizer/sanitizer_common/sanitizer_posix.cc
    libsanitizer/sanitizer_common/sanitizer_mac.cc
    gcc/ada/init.c
    gcc/config/darwin-driver.c
  )

  for file in ${files_to_patch[*]}; do
    if [ -f $file ]; then
      echo "patching $PWD/$file"
      $SED -i 's/#include <sys\/sysctl.h>/#define _Atomic volatile\n#include <sys\/sysctl.h>\n#undef _Atomic/g' $file
      $SED -i 's/#include <sys\/mount.h>/#define _Atomic volatile\n#include <sys\/mount.h>\n#undef _Atomic/g' $file
    fi
  done

  echo ""
fi

if [ "$(osxcross-cmp "$GCC_VERSION" '>=' 15.3.0)" -eq 1 ] &&
   [ "$(osxcross-cmp "$SDK_VERSION" '>=' 27)" -eq 1 ]; then
  patch -p0 -N -f < "$PATCH_DIR/gcc-darwin20-plus-config.gcc.patch" || true

  if [ -z "$BUILD_ARM64_GCC" ] && [ "$(osxcross-cmp "$GCC_VERSION" '<' 17.0.0)" -eq 1 ]; then
    patch -p1 -N -f < "$PATCH_DIR/gcc-darwin20-plus-driver.patch" || true
  fi
fi

# Fix GCC builds of the optional libstdc++ C++ standard modules
# by ensuring Apple SDKs define rsize_t outside Clang's stddef.h path.
if [ $(osxcross-cmp $GCC_VERSION '>=' 15) -eq 1 ]; then
if [ -n "$SDK" ]; then
  RSIZE_HEADER="$SDK/usr/include/sys/_types/_rsize_t.h"

  if [ -f "$RSIZE_HEADER" ] &&
     grep -Fqx '#if defined(__has_feature) && __has_feature(modules)' "$RSIZE_HEADER"; then
    echo "Patching rsize_t header in '$SDK' ..."

    $SED -i \
      's/^#if defined(__has_feature) && __has_feature(modules)$/#if defined(__clang__) \&\& defined(__has_feature) \&\& __has_feature(modules)/' \
      "$RSIZE_HEADER"
  fi
fi
fi


if [[ $PLATFORM == *BSD ]]; then
  export CPATH="/usr/local/include:/usr/pkg/include:$CPATH"
  export LDFLAGS="-L/usr/local/lib -L/usr/pkg/lib $LDFLAGS"
  export LD_LIBRARY_PATH="/usr/local/lib:/usr/pkg/lib:$LD_LIBRARY_PATH"
elif [ "$PLATFORM" == "Darwin" ]; then
  export CPATH="/opt/local/include:$CPATH"
  export LDFLAGS="-L/opt/local/lib $LDFLAGS"
  export LD_LIBRARY_PATH="/opt/local/lib:$LD_LIBRARY_PATH"
fi

LANGS="c,c++,objc,obj-c++"

if [ -n "$ENABLE_FORTRAN" ]; then
  LANGS+=",fortran"
fi

GCC_INSTALL_VERSION=$(echo $GCC_VERSION | tr '-' ' ' | awk '{print $1}')

for GCC_TARGET_ARCH in $GCC_TARGET_ARCHS; do
  # i386 is built by the x86_64 target through GCC multilib.
  if [ "$GCC_TARGET_ARCH" = "i386" ]; then
    continue
  fi

  GCC_TARGET_TRIPLE=$GCC_TARGET_ARCH-apple-$TARGET
  GCC_BUILD_SUBDIR=build-$GCC_TARGET_ARCH

  mkdir -p "$GCC_BUILD_SUBDIR"
  pushd "$GCC_BUILD_SUBDIR" &>/dev/null

  EXTRACONFFLAGS=""

  if [ "$PLATFORM" != "Darwin" ]; then
    EXTRACONFFLAGS+="--with-ld=$TARGET_DIR/bin/$GCC_TARGET_TRIPLE-ld "
    EXTRACONFFLAGS+="--with-as=$TARGET_DIR/bin/$GCC_TARGET_TRIPLE-as "
  fi

  # Enable multilib support for x86_64 builds if i386 is supported and enabled.
  if [ "$GCC_TARGET_ARCH" = "x86_64" ] &&
     arch_supported "$GCC_TARGET_ARCHS" i386; then
    EXTRACONFFLAGS+="--with-multilib-list=m32,m64 --enable-multilib "
  else
    EXTRACONFFLAGS+="--disable-multilib "
  fi

  ../configure \
    --target=$GCC_TARGET_TRIPLE \
    --with-sysroot=$SDK \
    --disable-nls \
    --enable-languages=$LANGS \
    --without-headers \
    --enable-lto \
    --enable-checking=release \
    --disable-libstdcxx-pch \
    --prefix=$TARGET_DIR \
    --with-system-zlib \
    $EXTRACONFFLAGS

  $MAKE -j$JOBS
  $MAKE install

  popd &>/dev/null # build

  pushd $TARGET_DIR/$GCC_TARGET_TRIPLE/include &>/dev/null
  pushd c++/${GCC_INSTALL_VERSION}* &>/dev/null

  cat $PATCH_DIR/libstdcxx.patch | \
    $SED "s/darwin13/$TARGET/g" | \
    patch -p0 -l &>/dev/null || true

  popd &>/dev/null
  popd &>/dev/null
done

popd &>/dev/null # gcc

fi # build required

popd &>/dev/null # build dir

unset USESYSTEMCOMPILER
source tools/tools.sh

pushd $TARGET_DIR/bin &>/dev/null

if arch_supported "$GCC_TARGET_ARCHS" aarch64; then
  if [ ! -f aarch64-apple-$TARGET-base-gcc ]; then
    mv aarch64-apple-$TARGET-gcc \
      aarch64-apple-$TARGET-base-gcc

    mv aarch64-apple-$TARGET-g++ \
      aarch64-apple-$TARGET-base-g++
  fi

  create_symlink aarch64-apple-$TARGET-base-gcc \
                 arm64-apple-$TARGET-base-gcc
  create_symlink aarch64-apple-$TARGET-base-g++ \
                 arm64-apple-$TARGET-base-g++
fi

if arch_supported "$GCC_TARGET_ARCHS" x86_64; then
  if [ ! -f x86_64-apple-$TARGET-base-gcc ]; then
    mv x86_64-apple-$TARGET-gcc \
      x86_64-apple-$TARGET-base-gcc

    mv x86_64-apple-$TARGET-g++ \
      x86_64-apple-$TARGET-base-g++
  fi

  if arch_supported "$GCC_TARGET_ARCHS" i386; then
    create_symlink x86_64-apple-$TARGET-base-gcc \
                   i386-apple-$TARGET-base-gcc

    create_symlink x86_64-apple-$TARGET-base-g++ \
                   i386-apple-$TARGET-base-g++
  fi
fi

echo "compiling wrapper ..."

TARGETCOMPILER=gcc \
  $BASE_DIR/wrapper/build_wrapper.sh

popd &>/dev/null # wrapper dir

echo ""

for GCC_TARGET_ARCH in $GCC_TARGET_ARCHS; do
  GCC_TARGET_TRIPLE=$GCC_TARGET_ARCH-apple-$TARGET
  GCC_TEST_ARCH=$GCC_TARGET_ARCH
  if [ "$GCC_TEST_ARCH" = "aarch64" ]; then
    GCC_TEST_ARCH=arm64
  fi
  GCC_TEST_TRIPLE=$GCC_TEST_ARCH-apple-$TARGET
  test_compiler $GCC_TEST_TRIPLE-gcc $BASE_DIR/oclang/test.c
  test_compiler $GCC_TEST_TRIPLE-g++ $BASE_DIR/oclang/test.cpp
done

echo ""

echo "Done! GCC was built for: $GCC_TARGET_ARCHS"

if arch_supported "$GCC_TARGET_ARCHS" aarch64; then
  echo ""
  echo "!!! When dealing with Automake projects make sure to use aarch64-apple-$TARGET-* instead of arm64-* !!!"
  echo "!!! CC=aarch64-apple-$TARGET-gcc ./configure --host=aarch64-apple-$TARGET !!!"
fi

echo ""
echo "Example usage:"
echo ""

if arch_supported "$GCC_TARGET_ARCHS" i386; then
  echo "CC=i386-apple-$TARGET-gcc ./configure --host=i386-apple-$TARGET"
fi

if arch_supported "$GCC_TARGET_ARCHS" aarch64; then
  echo "CC=aarch64-apple-$TARGET-gcc ./configure --host=aarch64-apple-$TARGET"
  echo "arm64-apple-$TARGET-gcc -Wall test.c -o test-arm64"
fi

if arch_supported "$GCC_TARGET_ARCHS" x86_64; then
  echo "CC=x86_64-apple-$TARGET-gcc ./configure --host=x86_64-apple-$TARGET"
  echo "x86_64-apple-$TARGET-gcc -Wall test.c -o test-x86_64"
  echo "x86_64-apple-$TARGET-strip -x test-x86_64"
fi
echo ""
