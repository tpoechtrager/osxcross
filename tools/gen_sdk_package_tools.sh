#!/usr/bin/env bash
#
# Package the macOS SDKs into a tar file to be used by `build.sh`.
# As opposed to gen_sdk_package.sh, which is used for extraction of SDKs
# from full Xcode version, gen_sdk_tools.sh extracts SDKs from Xcode
# Command Line Tools.
#
# Tested with XCode Command Line Tools 12.x
#

export LC_ALL=C


command -v gnutar &>/dev/null

if [ $? -eq 0 ]; then
  TAR=gnutar
else
  TAR=tar
fi


if [ -z "$SDK_COMPRESSOR" ]; then
  command -v xz &>/dev/null

  if [ $? -eq 0 ]; then
    SDK_COMPRESSOR=xz
    SDK_EXT="tar.xz"
  else
    SDK_COMPRESSOR=bzip2
    SDK_EXT="tar.bz2"
  fi
fi

case $SDK_COMPRESSOR in
  "gz")
    SDK_COMPRESSOR=gzip
    SDK_EXT=".tar.gz"
    ;;
  "bzip2")
    SDK_EXT=".tar.bz2"
    ;;
  "xz")
    SDK_EXT=".tar.xz"
    ;;
  "zip")
    SDK_EXT=".zip"
    ;;
  *)
    echo "error: unknown compressor \"$SDK_COMPRESSOR\"" >&2
    exit 1
esac

function compress()
{
  case $SDK_COMPRESSOR in
    "zip")
      $SDK_COMPRESSOR -q -5 -r - $1 > $2 ;;
    *)
      tar cf - $1 | $SDK_COMPRESSOR -5 - > $2 ;;
  esac
}


function rreadlink()
{
  if [ ! -h "$1" ]; then
    echo "$1"
  else
    local link="$(expr "$(command ls -ld -- "$1")" : '.*-> \(.*\)$')"
    cd $(dirname $1)
    rreadlink "$link" | sed "s|^\([^/].*\)\$|$(dirname $1)/\1|"
  fi
}


if [ $(uname -s) != "Darwin" ]; then
  if [ -z "$XCODE_TOOLS_DIR" ]; then
    echo "This script must be run on macOS" 1>&2
    echo "... Or with XCODE_TOOLS_DIR=... on Linux" 1>&2
    exit 1
  else
    case "$XCODE_TOOLS_DIR" in
      /*) ;;
      *) XCODE_TOOLS_DIR="$PWD/$XCODE_TOOLS_DIR" ;;
    esac
  fi
else
  XCODE_TOOLS_DIR="/Library/Developer/CommandLineTools"
fi

if [ ! -d "$XCODE_TOOLS_DIR" ]; then
  echo "cannot find Xcode Command Line Tools (XCODE_TOOLS_DIR=$XCODE_TOOLS_DIR)" 1>&2
  exit 1
fi

echo -e "found Xcode Command Line Tools: $XCODE_TOOLS_DIR"

WDIR=$(pwd)

set -e

pushd "$XCODE_TOOLS_DIR" &>/dev/null

if [ -d "SDKs" ]; then
  pushd "SDKs" &>/dev/null
else
  echo "$XCODE_TOOLS_DIR/SDKs does not exist"  1>&2
  exit 1
fi

SDKS=$(ls | grep -E "^MacOSX13.*|^MacOSX12.*|^MacOSX11.*|^MacOSX10.*" | grep -v "Patch")

if [ -z "$SDKS" ]; then
    echo "No SDK found" 1>&2
    exit 1
fi

# libc++ headers for C++11/C++14
LIBCXXDIR="usr/include/c++/v1"

# Manual directory
MANDIR="usr/share/man"

for SDK in $SDKS; do
  echo -n "packaging $(echo "$SDK" | sed -E "s/(.sdk|.pkg)//g") SDK "
  echo "(this may take several minutes) ..."

  if [[ $SDK == *.pkg ]]; then
    cp $SDK $WDIR
    continue
  fi

  TMP=$(mktemp -d /tmp/XXXXXXXXXXX)
  cp -r $(rreadlink $SDK) $TMP/$SDK &>/dev/null || true

  pushd "$XCODE_TOOLS_DIR" &>/dev/null

  mkdir -p $TMP/$SDK/usr/include/c++

  # libc++ headers for C++11/C++14
  if [ -d $LIBCXXDIR ]; then
    cp -rf $LIBCXXDIR "$TMP/$SDK/usr/include/c++"
  fi

  if [ -d $MANDIR ]; then
    mkdir -p $TMP/$SDK/usr/share/man
    cp -rf $MANDIR/* $TMP/$SDK/usr/share/man
  fi

  popd &>/dev/null

  pushd $TMP &>/dev/null
  compress "*" "$WDIR/$SDK$SDK_EXT"
  popd &>/dev/null

  rm -rf $TMP
done

popd &>/dev/null
popd &>/dev/null

echo ""
ls -lh | grep MacOSX
