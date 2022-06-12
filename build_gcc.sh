#!/usr/bin/env bash
#
# Build and install gcc/gcc++ as a cross-compiler with target OSX,
# using `clang`.
#
# You may want to run this script if you want to build software using
# gcc. Please refer to the README.md for details.
#

pushd "${0%/*}" &>/dev/null

unset LIBRARY_PATH

DESC=gcc
USESYSTEMCOMPILER=1
source tools/tools.sh

# GCC version to build
# (<4.7 will not work properly with libc++)
if [ -z "$GCC_VERSION" ]; then
  GCC_VERSION=12.1.0
  #GCC_VERSION=5-20200228 # snapshot
fi

if [ $(osxcross-cmp $OSX_VERSION_MIN '<=' 10.5) -eq 1 ]; then
  echo "You must build OSXCross with OSX_VERSION_MIN >= 10.6" 2>&1
  exit 1
fi

# GCC mirror
# Official GNU "ftp" doesn't have GCC snapshots
GCC_MIRROR="https://ftp.gnu.org/pub/gnu/gcc"
GCC_MIRROR_WITH_SNAPSHOTS="https://mirror.koddos.net/gcc"

pushd $BUILD_DIR &>/dev/null

function remove_locks()
{
  rm -rf $BUILD_DIR/have_gcc*
}

source $BASE_DIR/tools/trap_exit.sh

if [ ! -f "have_gcc_${GCC_VERSION}_${TARGET}" ]; then

pushd $TARBALL_DIR &>/dev/null
if [[ $GCC_VERSION != *-* ]]; then
  download "$GCC_MIRROR/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
else
  download "$GCC_MIRROR_WITH_SNAPSHOTS/snapshots/$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi
popd &>/dev/null

echo "cleaning up ..."
rm -rf gcc* 2>/dev/null

extract "$TARBALL_DIR/gcc-$GCC_VERSION.tar.xz"
echo ""

pushd gcc*$GCC_VERSION* &>/dev/null

rm -f $TARGET_DIR/bin/*-gcc*
rm -f $TARGET_DIR/bin/*-g++*

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


mkdir -p build
pushd build &>/dev/null

if [[ $PLATFORM == *BSD ]]; then
  export CPATH="/usr/local/include:/usr/pkg/include:$CPATH"
  export LDFLAGS="-L/usr/local/lib -L/usr/pkg/lib $LDFLAGS"
  export LD_LIBRARY_PATH="/usr/local/lib:/usr/pkg/lib:$LD_LIBRARY_PATH"
elif [ "$PLATFORM" == "Darwin" ]; then
  export CPATH="/opt/local/include:$CPATH"
  export LDFLAGS="-L/opt/local/lib $LDFLAGS"
  export LD_LIBRARY_PATH="/opt/local/lib:$LD_LIBRARY_PATH"
fi

EXTRACONFFLAGS=""

if [ "$PLATFORM" != "Darwin" ]; then
  EXTRACONFFLAGS+="--with-ld=$TARGET_DIR/bin/x86_64-apple-$TARGET-ld "
  EXTRACONFFLAGS+="--with-as=$TARGET_DIR/bin/x86_64-apple-$TARGET-as "
fi

LANGS="c,c++,objc,obj-c++"

if [ -n "$ENABLE_FORTRAN" ]; then
  LANGS+=",fortran"
fi

if [ $(osxcross-cmp $SDK_VERSION "<=" 10.13) -eq 1 ]; then
  EXTRACONFFLAGS+="--with-multilib-list=m32,m64 --enable-multilib "
else
  EXTRACONFFLAGS+="--disable-multilib "
fi

../configure \
  --target=x86_64-apple-$TARGET \
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

GCC_VERSION=`echo $GCC_VERSION | tr '-' ' ' |  awk '{print $1}'`

pushd $TARGET_DIR/x86_64-apple-$TARGET/include &>/dev/null
pushd c++/${GCC_VERSION}* &>/dev/null

cat $PATCH_DIR/libstdcxx.patch | \
  $SED "s/darwin13/$TARGET/g" | \
  patch -p0 -l &>/dev/null || true

popd &>/dev/null
popd &>/dev/null

popd &>/dev/null # build
popd &>/dev/null # gcc

touch "have_gcc_${GCC_VERSION}_${TARGET}"

fi # have gcc

popd &>/dev/null # build dir

unset USESYSTEMCOMPILER
source tools/tools.sh

pushd $TARGET_DIR/bin &>/dev/null

if [ ! -f i386-apple-$TARGET-base-gcc ]; then
  mv x86_64-apple-$TARGET-gcc \
    x86_64-apple-$TARGET-base-gcc

  mv x86_64-apple-$TARGET-g++ \
    x86_64-apple-$TARGET-base-g++

  if [ $(osxcross-cmp $SDK_VERSION "<=" 10.13) -eq 1 ]; then
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

if [ $(osxcross-cmp $SDK_VERSION "<=" 10.13) -eq 1 ]; then
  test_compiler o32-gcc $BASE_DIR/oclang/test.c
  test_compiler o32-g++ $BASE_DIR/oclang/test.cpp
fi

test_compiler o64-gcc $BASE_DIR/oclang/test.c
test_compiler o64-g++ $BASE_DIR/oclang/test.cpp

echo ""

echo "Done! Now you can use o32-gcc/o32-g++ and o64-gcc/o64-g++ as compiler"
echo ""
echo "Example usage:"
echo ""
echo "Example 1: CC=o32-gcc ./configure --host=i386-apple-$TARGET"
echo "Example 2: CC=i386-apple-$TARGET-gcc ./configure --host=i386-apple-$TARGET"
echo "Example 3: o64-gcc -Wall test.c -o test"
echo "Example 4: x86_64-apple-$TARGET-strip -x test"
echo ""
