#!/usr/bin/env bash
#
# Package the macOS SDKs into a tar file to be used by `build.sh`.
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
      $TAR cf - $1 | $SDK_COMPRESSOR -5 - > $2 ;;
  esac
}


WDIR=$(pwd)

if [ -z "$XCODE_TOOLS" ]; then
  # Using full fledged Xcode

  function set_xcode_dir()
  {
    local tmp=$(ls $1 2>/dev/null | grep "^Xcode.*.app" | grep -v "beta" | head -n1)

    if [ -z "$tmp" ]; then
      tmp=$(ls $1 2>/dev/null | grep "^Xcode.*.app" | head -n1)
    fi

    if [ -n "$tmp" ]; then
      XCODEDIR="$1/$tmp"
    fi
  }

  if [ $(uname -s) != "Darwin" ]; then
    if [ -z "$XCODEDIR" ]; then
      echo "This script must be run on macOS" 1>&2
      echo "... Or with XCODEDIR=... on Linux" 1>&2
      exit 1
    else
      case "$XCODEDIR" in
        /*) ;;
        *) XCODEDIR="$PWD/$XCODEDIR" ;;
      esac
      set_xcode_dir "$XCODEDIR"
    fi
  else
    set_xcode_dir $(echo /Volumes/Xcode* | tr ' ' '\n' | grep -v "beta" | head -n1)

    if [ -z "$XCODEDIR" ]; then
      set_xcode_dir /Applications

      if [ -z "$XCODEDIR" ]; then
        set_xcode_dir $(echo /Volumes/Xcode* | tr ' ' '\n' | head -n1)

        if [ -z "$XCODEDIR" ]; then
          echo "please mount Xcode.dmg" 1>&2
          exit 1
        fi
      fi
    fi
  fi

  if [ ! -d "$XCODEDIR" ]; then
    echo "cannot find Xcode (XCODEDIR=$XCODEDIR)" 1>&2
    exit 1
  fi

  echo -e "found Xcode: $XCODEDIR"

  set -e

  pushd "$XCODEDIR" &>/dev/null

  if [ -d "Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs" ]; then
    pushd "Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs" &>/dev/null
  else
    if [ -d "../Packages" ]; then
      pushd "../Packages" &>/dev/null
    elif [ -d "Packages" ]; then
      pushd "Packages" &>/dev/null
    else
      if [ $? -ne 0 ]; then
        echo "Xcode (or this script) is out of date" 1>&2
        echo "trying some magic to find the SDKs anyway ..." 1>&2

        SDKDIR=$(find . -name SDKs -type d | grep MacOSX | head -n1)

        if [ -z "$SDKDIR" ]; then
          echo "cannot find SDKs!" 1>&2
          exit 1
        fi

        pushd "$SDKDIR" &>/dev/null
      fi
    fi
  fi

else
  # Using Xcode Command Line tools

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

  pushd "$XCODE_TOOLS_DIR" &>/dev/null

  if [ -d "SDKs" ]; then
    pushd "SDKs" &>/dev/null
  else
    echo "$XCODE_TOOLS_DIR/SDKs does not exist"  1>&2
    exit 1
  fi

  XCODEDIR=$XCODE_TOOLS_DIR

  set -e

fi

# Group SDK aliases by their physical directory. Keep the name with the most
# version components, so MacOSX13.3.sdk wins over MacOSX13.sdk.
SDKS=()
SDK_PATHS=()
SDK_VERSION_DEPTHS=()
for pat in MacOS[0-9]*.sdk MacOSX[0-9]*.sdk; do
  while IFS= read -r d; do
    if [[ "$d" != *Patch* && -d "$d" ]]; then
      sdk_path=$(cd -P -- "$d" && pwd -P) || continue
      sdk_version=${d%.sdk}
      sdk_version=${sdk_version#MacOSX}
      sdk_version=${sdk_version#MacOS}
      sdk_dots=${sdk_version//[^.]/}
      sdk_depth=${#sdk_dots}
      found=0
      for ((i = 0; i < ${#SDKS[@]}; i++)); do
        if [ "${SDK_PATHS[$i]}" = "$sdk_path" ]; then
          found=1
          if [ "$sdk_depth" -gt "${SDK_VERSION_DEPTHS[$i]}" ]; then
            SDKS[$i]=$d
            SDK_VERSION_DEPTHS[$i]=$sdk_depth
          fi
          break
        fi
      done
      if [ "$found" = 0 ]; then
        SDKS+=("$d")
        SDK_PATHS+=("$sdk_path")
        SDK_VERSION_DEPTHS+=("$sdk_depth")
      fi
    fi
  done < <(compgen -G "$pat")
done

if [ ${#SDKS[@]} -eq 0 ]; then
  echo "No SDK found" 1>&2
  exit 1
fi

# Xcode 5
LIBCXXDIR1="Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/c++/v1"

# Xcode 6
LIBCXXDIR2="Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1"

# Xcode Command Line Tools
LIBCXXDIR3="usr/include/c++/v1"

# Manual directory
MANDIR="Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/share/man"

for SDK in "${SDKS[@]}"; do
  echo -n "packaging $(echo "$SDK" | sed -E "s/(.sdk|.pkg)//g") SDK "
  echo "(this may take several minutes) ..."

  if [[ $SDK == *.pkg ]]; then
    cp $SDK $WDIR
    continue
  fi

  TMP=$(mktemp -d /tmp/XXXXXXXXXXX)
  SDK_PATH=$(cd -P -- "$SDK" && pwd -P)
  cp -r "$SDK_PATH" "$TMP/$SDK"

  pushd "$XCODEDIR" &>/dev/null

  mkdir -p $TMP/$SDK/usr/include/c++

  # libc++ headers
  if [ ! -f "$TMP/$SDK/usr/include/c++/v1/version" ]; then
    if [ -d $LIBCXXDIR1 ]; then
      cp -rf $LIBCXXDIR1 "$TMP/$SDK/usr/include/c++"
    elif [ -d $LIBCXXDIR2 ]; then
      cp -rf $LIBCXXDIR2 "$TMP/$SDK/usr/include/c++"
    elif [ -d $LIBCXXDIR3 ]; then
      cp -rf $LIBCXXDIR3 "$TMP/$SDK/usr/include/c++"
    fi
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
