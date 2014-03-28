#!/usr/bin/env bash
#
# OS X SDK packaging script
# This script must be run on OS X
#

if [ `uname -s` != "Darwin" ]; then
    echo "This script must be run on OS X"
    exit 1
fi

XCODEDIR=$(ls /Volumes | grep Xcode | head -n1)

if [ -z "$XCODEDIR" ]; then
    if [ -d "/Applications/Xcode.app" ]; then
        XCODEDIR="/Applications/Xcode.app"
    else
        echo "please mount Xcode.dmg"
        exit 1
    fi
else
    XCODEDIR="/Volumes/$XCODEDIR/Xcode.app"
fi

[ ! -d $XCODEDIR ] && exit 1
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
pushd "Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs" &>/dev/null || {
    echo "Xcode (or this script) is out of date"
    echo "trying some magic to find the SDKs anyway ..."

    SDKDIR=$(find . -name SDKs -type d | grep MacOSX | head -n1)

    if [ -z "$SDKDIR" ]; then
        echo "cannot find SDKs!"
        exit 1
    fi

    pushd $SDKDIR &>/dev/null
}

SDKS=$(ls | grep MacOSX)

if [ -z "$SDKS" ]; then
    echo "No SDK found"
    exit 1
fi

LIBCXXDIR="Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/c++/v1"

for SDK in $SDKS; do
    echo "packaging ${SDK/.sdk/} SDK (this may take several minutes) ..."

    if [[ $SDK == *.pkg ]]; then
        cp $SDK $WDIR
        continue
    fi

    TMP=$(mktemp -d /tmp/XXXXXXXXXXX)
    cp -r $SDK $TMP &>/dev/null || true

    pushd $XCODEDIR &>/dev/null

    # libc++ headers for C++11/C++14
    if [ -d $LIBCXXDIR ]; then
        cp -rf $LIBCXXDIR "$TMP/$SDK/usr/include/c++"
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
ls -l | grep MacOSX
