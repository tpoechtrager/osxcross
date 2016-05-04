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
  GCC_VERSION=6.1.0
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

if [ ! -f "have_gcc_${GCC_VERSION}_${OSXCROSS_TARGET}" ]; then

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
  EXTRACONFFLAGS+="--with-ld=$OSXCROSS_TARGET_DIR/bin/x86_64-apple-$OSXCROSS_TARGET-ld "
  EXTRACONFFLAGS+="--with-as=$OSXCROSS_TARGET_DIR/bin/x86_64-apple-$OSXCROSS_TARGET-as "
fi

LANGS="c,c++,objc,obj-c++"

if [ -n "$ENABLE_FORTRAN" ]; then
  LANGS+=",fortran"
fi

../configure \
  --target=x86_64-apple-$OSXCROSS_TARGET \
  --with-sysroot=$OSXCROSS_SDK \
  --disable-nls \
  --enable-languages=$LANGS \
  --without-headers \
  --enable-multilib \
  --with-multilib-list=m32,m64 \
  --enable-lto \
  --enable-checking=release \
  --disable-libstdcxx-pch \
  --prefix=$OSXCROSS_TARGET_DIR \
  --with-system-zlib \
  $EXTRACONFFLAGS

$MAKE -j$JOBS
$MAKE install

GCC_VERSION=`echo $GCC_VERSION | tr '-' ' ' |  awk '{print $1}'`

pushd $OSXCROSS_TARGET_DIR/x86_64-apple-$OSXCROSS_TARGET/include &>/dev/null
pushd c++/${GCC_VERSION}* &>/dev/null

cat $OSXCROSS_TARGET_DIR/../patches/libstdcxx.patch | \
  $SED "s/darwin13/$OSXCROSS_TARGET/g" | \
  patch -p0 -l &>/dev/null || true

popd &>/dev/null
popd &>/dev/null

popd &>/dev/null # build
popd &>/dev/null # gcc

touch "have_gcc_${GCC_VERSION}_${OSXCROSS_TARGET}"

fi # have gcc

popd &>/dev/null # build dir

unset USESYSTEMCOMPILER
source tools/tools.sh

pushd $OSXCROSS_TARGET_DIR/bin &>/dev/null

if [ ! -f i386-apple-$OSXCROSS_TARGET-base-gcc ]; then
  mv x86_64-apple-$OSXCROSS_TARGET-gcc x86_64-apple-$OSXCROSS_TARGET-base-gcc
  mv x86_64-apple-$OSXCROSS_TARGET-g++ x86_64-apple-$OSXCROSS_TARGET-base-g++

  ln -sf x86_64-apple-$OSXCROSS_TARGET-base-gcc i386-apple-$OSXCROSS_TARGET-base-gcc
  ln -sf x86_64-apple-$OSXCROSS_TARGET-base-g++ i386-apple-$OSXCROSS_TARGET-base-g++
fi

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

test_compiler o32-gcc $BASE_DIR/oclang/test.c
test_compiler o64-gcc $BASE_DIR/oclang/test.c

test_compiler o32-g++ $BASE_DIR/oclang/test.cpp
test_compiler o64-g++ $BASE_DIR/oclang/test.cpp

echo ""

echo "Done! Now you can use o32-gcc/o32-g++ and o64-gcc/o64-g++ as compiler"
echo ""
echo "Example usage:"
echo ""
echo "Example 1: CC=o32-gcc ./configure --host=i386-apple-$OSXCROSS_TARGET"
echo "Example 2: CC=i386-apple-$OSXCROSS_TARGET-gcc ./configure --host=i386-apple-$OSXCROSS_TARGET"
echo "Example 3: o64-gcc -Wall test.c -o test"
echo "Example 4: x86_64-apple-$OSXCROSS_TARGET-strip -x test"
echo ""
