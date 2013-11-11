#!/usr/bin/env bash
TMPDIR=`mktemp -d`
test $? -eq 0 || exit 1
BASEDIR=`pwd`
cp -r . $TMPDIR
pushd $TMPDIR || exit 1
rm -rf build
rm -rf target
rm -rf tarballs/*MacOSX*
find . -name "*~" -exec rm {} \;
find . -name "*.save" -exec rm {} \;
rm -rf *.tar.xz
tar -cf - * | xz -9 -c - > $BASEDIR/osxcross.tar.xz || exit 1
popd
rm -rf $TMPDIR
