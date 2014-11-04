#!/usr/bin/env bash
# OS X SDK packaging script

export LC_ALL=C

if [ $(uname -s) != "Darwin" ]; then
  if [ -z "$XCODEDIR" ]; then
    echo "This script must be run on OS X"
    echo "... Or with XCODEDIR=... on Linux"
    exit 1
  else
    XCODEDIR+="/$(ls "$XCODEDIR" | grep "^Xcode.*" | head -n1)"
  fi
else
  XCODEDIR=$(ls /Volumes | grep "^Xcode.*" | head -n1)

  if [ -z "$XCODEDIR" ]; then
    if [ -d /Applications/Xcode*.app ]; then
      XCODEDIR="/Applications/Xcode*.app"
    else
      echo "please mount Xcode.dmg"
      exit 1
    fi
  else
    XCODEDIR="/Volumes/$XCODEDIR/Xcode*.app"
  fi
fi

if [ ! -d $XCODEDIR ]; then
  echo "cannot find Xcode (XCODEDIR=$XCODEDIR)"
  exit 1
fi

echo -e "found Xcode: $XCODEDIR"

WDIR=$(pwd)

which gnutar &>/dev/null

if [ $? -eq 0 ]; then
  TAR=gnutar
else
  TAR=tar
fi

which xz &>/dev/null

if [ $? -eq 0 ]; then
  COMPRESSOR=xz
  PKGEXT="tar.xz"
else
  COMPRESSOR=bzip2
  PKGEXT="tar.bz2"
fi

set -e

pushd $XCODEDIR &>/dev/null

if [ -d "Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs" ]; then
  pushd "Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs" &>/dev/null
else
  if [ -d "../Packages" ]; then
    pushd "../Packages" &>/dev/null
  else
    if [ $? -ne 0 ]; then
      echo "Xcode (or this script) is out of date"
      echo "trying some magic to find the SDKs anyway ..."

      SDKDIR=$(find . -name SDKs -type d | grep MacOSX | head -n1)

      if [ -z "$SDKDIR" ]; then
        echo "cannot find SDKs!"
        exit 1
      fi

      pushd $SDKDIR &>/dev/null
    fi
  fi
fi

SDKS=$(ls | grep "^MacOSX10.*" | grep -v "Patch")

if [ -z "$SDKS" ]; then
    echo "No SDK found"
    exit 1
fi

# Xcode 5
LIBCXXDIR1="Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/c++/v1"

# Xcode 6
LIBCXXDIR2="Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1"

for SDK in $SDKS; do
  echo -n "packaging $(echo "$SDK" | sed -E "s/(.sdk|.pkg)//g") SDK "
  echo "(this may take several minutes) ..."

  if [[ $SDK == *.pkg ]]; then
    cp $SDK $WDIR
    continue
  fi

  TMP=$(mktemp -d /tmp/XXXXXXXXXXX)
  cp -r $SDK $TMP &>/dev/null || true

  pushd $XCODEDIR &>/dev/null

  # libc++ headers for C++11/C++14
  if [ -d $LIBCXXDIR1 ]; then
    cp -rf $LIBCXXDIR1 "$TMP/$SDK/usr/include/c++"
  elif [ -d $LIBCXXDIR2 ]; then
    cp -rf $LIBCXXDIR2 "$TMP/$SDK/usr/include/c++"
  fi

  popd &>/dev/null

  pushd $TMP &>/dev/null
  $TAR -cf - * | $COMPRESSOR -9 -c - > "$WDIR/$SDK.$PKGEXT"
  popd &>/dev/null

  rm -rf $TMP
done

popd &>/dev/null
popd &>/dev/null

echo ""
ls -lh | grep MacOSX
