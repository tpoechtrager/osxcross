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

if [ -n "$APPLE_GCC" ]; then
  GCC_VERSION=4.2.1-apple-gcc

  if [ -z "$APPLE_GCC_VERSION" ]; then
    if [ -n "$POWERPC" ]; then
      APPLE_GCC_VERSION=5575.11
    else
      APPLE_GCC_VERSION=5666.3
    fi
  fi
fi

# GCC mirror
if [ -n "$APPLE_GCC" ]; then
  GCC_MIRROR="https://opensource.apple.com/"
else
  GCC_MIRROR="ftp://ftp.fu-berlin.de/unix/languages/gcc"
fi

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
  if [ -z "$APPLE_GCC" ]; then
    export DISABLE_ANNOYING_LD64_ASSERTION=1
  fi
else
  GCC_ARCH=x86_64
  GCC_ARCH_SHORT=o64
  GCC_ARCH32=i386
  GCC_ARCH_SHORT32=o32
  TARGETARCHS=x86
fi

export TARGETARCHS # wrapper

ARCHS_TO_BUILD=$GCC_ARCH
EXTRA_CONFFLAGS=""

if [ -n "$APPLE_GCC" ]; then
  # work around broken multilib support
  ARCHS_TO_BUILD+=" $GCC_ARCH32"
  EXTRA_CONFFLAGS+="--with-gxx-include-dir=/usr/include/c++/4.0.0 " ## TODO: version

  # fix -lstdc++ link
  if [ ! -f "$OSXCROSS_SDK/usr/lib/libstdc++.dylib" ]; then
    pushd $OSXCROSS_SDK/usr/lib &>/dev/null
    set +e
    if [ ! -f "libstdc++.dylib" ]; then
      ln -sf "libstdc++.6.dylib" "libstdc++.dylib"
    fi
    set -e
    popd &>/dev/null
  fi
else
  EXTRA_CONFFLAGS+="--enable-multilib --with-multilib-list=m32,m64 "
fi

if [ ! -f "have_gcc_${GCC_VERSION}_${ARCHS_TO_BUILD/ /_}_${OSXCROSS_TARGET}" ]; then

pushd $OSXCROSS_TARBALL_DIR &>/dev/null
if [ -n "$APPLE_GCC" ]; then
  wget -c "$GCC_MIRROR/tarballs/gcc/gcc-$APPLE_GCC_VERSION.tar.gz"
else
  if [[ $GCC_VERSION != *-* ]]; then
    wget -c "$GCC_MIRROR/releases/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"
  else
    wget -c "$GCC_MIRROR/snapshots/$GCC_VERSION/gcc-$GCC_VERSION.tar.bz2"
  fi
fi
popd &>/dev/null

echo "cleaning up ..."
rm -rf gcc* 2>/dev/null

if [ -n "$APPLE_GCC" ]; then
  extract "$OSXCROSS_TARBALL_DIR/gcc-$APPLE_GCC_VERSION.tar.gz" 1
else
  extract "$OSXCROSS_TARBALL_DIR/gcc-$GCC_VERSION.tar.gz" 1
  ( cd ./gcc-$GCC_VERSION/ && ./contrib/download_prerequisites )
fi

echo ""

if [ -n "$APPLE_GCC" ]; then
  pushd gcc*$APPLE_GCC_VERSION &>/dev/null
  [ -z "$CC" ] && CC=cc
  export CC+=" -std=gnu89"
else
  pushd gcc*$GCC_VERSION* &>/dev/null
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

if [ $(osxcross-cmp $GCC_VERSION '>' 5.0.0) == 1 ] &&
   [ $(osxcross-cmp $GCC_VERSION '<' 5.3.0) == 1 ]; then
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66035
  patch -p1 < $PATCH_DIR/gcc-pr66035.patch
fi

if [ $GCC_ARCH == "x86_64" ] &&
   [ $(osxcross-cmp $OSXCROSS_SDK_VERSION '<=' 10.4) == 1 ]; then
  # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64184
  $SED -i 's/sysconf(_SC_NPROCESSORS_ONLN)/1/g' \
    libcilkrts/runtime/sysdep-unix.c
fi

rm -f $OSXCROSS_TARGET_DIR/bin/$GCC_ARCH-apple-$OSXCROSS_TARGET*-gcc*
rm -f $OSXCROSS_TARGET_DIR/bin/$GCC_ARCH-apple-$OSXCROSS_TARGET*-g++*

LANGS="c,c++,objc,obj-c++"

if [ -n "$ENABLE_FORTRAN" ]; then
  LANGS+=",fortran"
fi

if [ -n "$APPLE_GCC" ]; then
  $SED -i "s/dsymutil /$GCC_ARCH-apple-$OSXCROSS_TARGET-dsymutil /" \
    gcc/config/darwin.h
else
  $SED -i "s/dsymutil\"/$GCC_ARCH-apple-$OSXCROSS_TARGET-dsymutil\"/" \
    gcc/config/darwin.h
fi

for ARCH_TO_BUILD in $ARCHS_TO_BUILD; do

  mkdir -p build_$ARCH_TO_BUILD
  pushd build_$ARCH_TO_BUILD &>/dev/null

  ../configure \
    --target=$ARCH_TO_BUILD-apple-$OSXCROSS_TARGET \
    --with-sysroot=$OSXCROSS_SDK \
    --with-ld=$OSXCROSS_TARGET_DIR/bin/$GCC_ARCH-apple-$OSXCROSS_TARGET-ld \
    --with-as=$OSXCROSS_TARGET_DIR/bin/$GCC_ARCH-apple-$OSXCROSS_TARGET-as \
    --disable-nls \
    --enable-languages=$LANGS \
    --enable-lto \
    --enable-checking=release \
    --disable-libstdcxx-pch \
    --prefix=$OSXCROSS_TARGET_DIR \
    --with-system-zlib \
    $EXTRA_CONFFLAGS

  $MAKE -j$JOBS

  popd &>/dev/null # build

done

popd &>/dev/null # gcc

touch "have_gcc_${GCC_VERSION}_${ARCHS_TO_BUILD/ /_}_${OSXCROSS_TARGET}"

fi # have gcc

for ARCH_TO_BUILD in $ARCHS_TO_BUILD; do

  if [ -n "$APPLE_GCC" ]; then
    pushd gcc*$APPPE_GCC42_VERSION/build_$ARCH_TO_BUILD &>/dev/null
  else
    pushd gcc*$GCC_VERSION*/build_$ARCH_TO_BUILD &>/dev/null
  fi

  $MAKE install

  if [ -z "$APPLE_GCC" ]; then
    GCC_VERSION=$(echo $GCC_VERSION | tr '-' ' ' |  awk '{print $1}')

    pushd $OSXCROSS_TARGET_DIR/$ARCH_TO_BUILD-apple-$OSXCROSS_TARGET/include &>/dev/null
    pushd c++/${GCC_VERSION}* &>/dev/null

    cat $OSXCROSS_TARGET_DIR/../patches/libstdcxx.patch | \
      $SED "s/darwin13/$OSXCROSS_TARGET/g" | \
      patch -p0 -l &>/dev/null || true

    popd &>/dev/null
    popd &>/dev/null

  fi

  popd &>/dev/null # gcc/build

done


popd &>/dev/null # build dir


unset USESYSTEMCOMPILER
source tools/tools.sh


pushd $OSXCROSS_TARGET_DIR/bin &>/dev/null

for ARCH_TO_BUILD in $ARCHS_TO_BUILD; do
  mv $ARCH_TO_BUILD-apple-$OSXCROSS_TARGET-gcc $ARCH_TO_BUILD-apple-$OSXCROSS_TARGET-base-gcc
  mv $ARCH_TO_BUILD-apple-$OSXCROSS_TARGET-g++ $ARCH_TO_BUILD-apple-$OSXCROSS_TARGET-base-g++
  [ -n $APPLE_GCC ] && touch $ARCH_TO_BUILD-apple-$OSXCROSS_TARGET-apple-gcc
done

if [ -z "$APPLE_GCC" ]; then
  ln -sf $GCC_ARCH-apple-$OSXCROSS_TARGET-base-gcc $GCC_ARCH32-apple-$OSXCROSS_TARGET-base-gcc
  ln -sf $GCC_ARCH-apple-$OSXCROSS_TARGET-base-g++ $GCC_ARCH32-apple-$OSXCROSS_TARGET-base-g++
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
