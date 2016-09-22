#!/usr/bin/env bash

pushd "${0%/*}/.." &>/dev/null
source tools/tools.sh

if [[ $PLATFORM != CYGWIN* ]]; then
  exit 1
fi

LLVM_CONFIG="llvm-config"

CXXFLAGS="$($LLVM_CONFIG --cxxflags) -fno-PIC"
LDFLAGS="$($LLVM_CONFIG --cxxflags) -Wl,-s"
INCDIR=$($LLVM_CONFIG --includedir)
LIBDIR=$($LLVM_CONFIG --libdir)
LIBS=$($LLVM_CONFIG --libs all)
SYSLIBS="$($LLVM_CONFIG --system-libs) -ledit -lffi"

VERSION=$($LLVM_CONFIG --version | awk -F \. {'print $1$2'} | sed 's/svn//g')

set -e
TMP=$(mktemp -d)
set +e

pushd $TMP &>/dev/null
wget https://raw.githubusercontent.com/llvm-mirror/llvm/release_$VERSION/tools/lto/lto.cpp
wget https://raw.githubusercontent.com/llvm-mirror/llvm/release_$VERSION/tools/lto/LTODisassembler.cpp
wget https://raw.githubusercontent.com/llvm-mirror/llvm/release_$VERSION/tools/lto/lto.exports

echo "{" > cyglto.exports
echo "  global:" >> cyglto.exports
while read p; do
  echo "   $p;" >> cyglto.exports
done < lto.exports
echo "   LLVM*;" >> cyglto.exports
echo "  local: *;" >> cyglto.exports
echo "};" >> cyglto.exports

popd &>/dev/null

set -x

g++ -shared \
 -L$LIBDIR -I$INCDIR $CXXFLAGS $LDFLAGS \
 -Wl,--whole-archive $LIBS -Wl,--no-whole-archive $SYSLIBS \
 $TMP/lto.cpp $TMP/LTODisassembler.cpp -Wl,-version-script,$TMP/cyglto.exports \
 -o /bin/cygLTO.dll -Wl,--out-implib,/lib/libLTO.dll.a

rm -rf $TMP

popd &>/dev/null
