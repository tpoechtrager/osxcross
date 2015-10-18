#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null

unset LIBRARY_PATH

DESC=gcc
USESYSTEMCOMPILER=1
source tools/tools.sh

eval $(tools/osxcross_conf.sh)

# GCC version to build
# (<4.7 will not work properly with libc++)
if [ -z "$GCC_VERSION" ]; then
  GCC_VERSION=5.2.0
  #GCC_VERSION=5-20140928 # snapshot
fi

# GCC mirror
GCC_MIRROR="ftp://ftp.fu-berlin.de/unix/languages/gcc"

require wget

pushd $OSXCROSS_BUILD_DIR &>/dev/null


function remove_locks()
{
  rm -rf $OSXCROSS_BUILD_DIR/have_gcc*
}

source $BASE_DIR/tools/trap_exit.sh

if [ -n "$POWERPC" ]; then
  if [ $(sdk_has_ppc_support $OSXCROSS_SDK) -ne 1 ]; then
    echo "The SDK you are using does not support PowerPC" 1>&2
    exit 1
  fi
  GCC_ARCH=powerpc64
  GCC_ARCH_SHORT=oppc64
  GCC_ARCH32=powerpc
  GCC_ARCH_SHORT32=oppc32
  TARGETARCHS=ppc
  export DISABLE_ANNOYING_LD64_ASSERTION=1
else
  GCC_ARCH=x86_64
  GCC_ARCH_SHORT=o64
  GCC_ARCH32=i386
  GCC_ARCH_SHORT32=o32
  TARGETARCHS=x86
fi

export TARGETARCHS

if [ ! -f "have_gcc_${GCC_VERSION}_${GCC_ARCH}_${OSXCROSS_TARGET}" ]; then

pushd $OSXCROSS_TARBALL_DIR &>/dev/null
if [[ $GCC_VERSION != *-* ]]; then
  wget -c "$GCC_MIRROR/releases/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.bz2"
else
  wget -c "$GCC_MIRROR/snapshots/$GCC_VERSION/gcc-$GCC_VERSION.tar.bz2"
fi
popd &>/dev/null

echo "cleaning up ..."
rm -rf gcc* 2>/dev/null

extract "$OSXCROSS_TARBALL_DIR/gcc-$GCC_VERSION.tar.bz2" 1
echo ""

pushd gcc*$GCC_VERSION* &>/dev/null

rm -f $OSXCROSS_TARGET_DIR/bin/*-gcc*
rm -f $OSXCROSS_TARGET_DIR/bin/*-g++*

if [ $(osxcross-cmp $GCC_VERSION '>' 5.0.0) == 1 ] &&
   [ $(osxcross-cmp $GCC_VERSION '<' 5.3.0) == 1 ]; then
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66035
  patch -p1 < $PATCH_DIR/gcc-pr66035.patch
fi

if [ $GCC_ARCH == "x86_64" ] &&
   [ $(osxcross-cmp $OSXCROSS_SDK_VERSION '<=' 10.4) == 1 ]; then
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64184
  $SED -i 's/sysconf(_SC_NPROCESSORS_ONLN)/1/g' \
    libcilkrts/runtime/sysdep-unix.c || true
fi

mkdir -p build_$GCC_ARCH
pushd build_$GCC_ARCH &>/dev/null

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

$SED -i "s/dsymutil\"/$GCC_ARCH-apple-$OSXCROSS_TARGET-dsymutil\"/" \
  ../gcc/config/darwin.h

../configure \
  --target=$GCC_ARCH-apple-$OSXCROSS_TARGET \
  --with-sysroot=$OSXCROSS_SDK \
  --with-ld=$OSXCROSS_TARGET_DIR/bin/$GCC_ARCH-apple-$OSXCROSS_TARGET-ld \
  --with-as=$OSXCROSS_TARGET_DIR/bin/$GCC_ARCH-apple-$OSXCROSS_TARGET-as \
  --disable-nls \
  --enable-languages=$LANGS \
  --without-headers \
  --enable-multilib \
  --with-multilib-list=m32,m64 \
  --enable-lto \
  --enable-checking=release \
  --disable-libstdcxx-pch \
  --prefix=$OSXCROSS_TARGET_DIR \
  --with-system-zlib

$MAKE -j$JOBS

popd &>/dev/null # build
popd &>/dev/null # gcc

touch "have_gcc_${GCC_VERSION}_${GCC_ARCH}_${OSXCROSS_TARGET}"

fi # have gcc


pushd gcc*$GCC_VERSION*/build_$GCC_ARCH &>/dev/null
$MAKE install

GCC_VERSION=$(echo $GCC_VERSION | tr '-' ' ' |  awk '{print $1}')

pushd $OSXCROSS_TARGET_DIR/$GCC_ARCH-apple-$OSXCROSS_TARGET/include &>/dev/null
pushd c++/${GCC_VERSION}* &>/dev/null

cat $OSXCROSS_TARGET_DIR/../patches/libstdcxx.patch | \
  $SED "s/darwin13/$OSXCROSS_TARGET/g" | \
  patch -p0 -l &>/dev/null || true

popd &>/dev/null
popd &>/dev/null
popd &>/dev/null # gcc/build


popd &>/dev/null # build dir


unset USESYSTEMCOMPILER
source tools/tools.sh


pushd $OSXCROSS_TARGET_DIR/bin &>/dev/null

mv $GCC_ARCH-apple-$OSXCROSS_TARGET-gcc $GCC_ARCH-apple-$OSXCROSS_TARGET-base-gcc
mv $GCC_ARCH-apple-$OSXCROSS_TARGET-g++ $GCC_ARCH-apple-$OSXCROSS_TARGET-base-g++
ln -sf $GCC_ARCH-apple-$OSXCROSS_TARGET-base-gcc $GCC_ARCH32-apple-$OSXCROSS_TARGET-base-gcc
ln -sf $GCC_ARCH-apple-$OSXCROSS_TARGET-base-g++ $GCC_ARCH32-apple-$OSXCROSS_TARGET-base-g++

echo "compiling wrapper ..."

export OSXCROSS_VERSION
export OSXCROSS_LIBLTO_PATH
export OSXCROSS_TARGET
export OSXCROSS_OSX_VERSION_MIN=$OSXCROSS_OSX_VERSION_MIN
export OSXCROSS_LINKER_VERSION=$OSXCROSS_LINKER_VERSION

TARGETCOMPILER=gcc \
  $BASE_DIR/wrapper/build.sh 1>/dev/null

popd &>/dev/null # wrapper dir

echo ""

test_compiler $GCC_ARCH_SHORT32-gcc $BASE_DIR/oclang/test.c
test_compiler $GCC_ARCH_SHORT-gcc $BASE_DIR/oclang/test.c

test_compiler $GCC_ARCH_SHORT32-g++ $BASE_DIR/oclang/test.cpp
test_compiler $GCC_ARCH_SHORT-g++ $BASE_DIR/oclang/test.cpp

echo ""

echo -n "Done! Now you can use $GCC_ARCH_SHORT32-gcc/$GCC_ARCH_SHORT32-g++ and "
echo    "$GCC_ARCH_SHORT32-gcc/$GCC_ARCH_SHORT32-g++ as compiler"
echo ""
echo "Example usage:"
echo ""
echo "Example 1: CC=$GCC_ARCH_SHORT32-gcc ./configure --host=$GCC_ARCH32-apple-$OSXCROSS_TARGET"
echo "Example 2: CC=$GCC_ARCH-apple-$OSXCROSS_TARGET-gcc ./configure --host=$GCC_ARCH-apple-$OSXCROSS_TARGET"
echo "Example 3: $GCC_ARCH_SHORT-gcc -Wall test.c -o test"
echo "Example 4: $GCC_ARCH-apple-$OSXCROSS_TARGET-strip -x test"
echo ""
