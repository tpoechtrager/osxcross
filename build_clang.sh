#!/usr/bin/env bash
#
# Build and install Clang/LLVM, using `gcc`.
#
# You only need to run this if your distribution does not provide
# clang - or if you want to build your own version from a recent
# source tree.
#

pushd "${0%/*}" &>/dev/null

DESC=clang
USESYSTEMCOMPILER=1

source tools/tools.sh

mkdir -p $BUILD_DIR

source $BASE_DIR/tools/trap_exit.sh

if [ -z "$CLANG_VERSION" ]; then
  CLANG_VERSION=12.0.0
fi

if [ -z "$INSTALLPREFIX" ]; then
  INSTALLPREFIX="/usr/local"
fi

# acceptable values are llvm or apple
if [ -z "$GITPROJECT" ]; then
  GITPROJECT="apple"
fi

require cmake
require bsdtar

CLANG_LLVM_PKG=""

function set_package_link()
{
  pushd $TARBALL_DIR &>/dev/null
  
  # Official LLVM project download URLs look like:
  # https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-10.0.1.zip
  
  # Apple LLVM project download URLs look like:
  # https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20200108.zip
  # where the branch-to-major-version lookup is the below:
  # apple/stable/20210107 = 12
  # apple/stable/20200714 = 11
  # apple/stable/20200108 = 10
  # apple/stable/20190619 = 9
  # apple/stable/20190104 = 8
  # apple/stable/20180801 = 7
  # apple/stable/20180103 = 6
  # apple/stable/20170719 = 5
  # apple/stable/20170116 = 4
  
  if [ $GITPROJECT == "llvm" ]; then
    # with official LLVM we just pass the version straight into the URL
    CLANG_LLVM_PKG="https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-$CLANG_VERSION.zip"
  elif [ $GITPROJECT == "apple" ]; then
    # with Apple LLVM we only get each major version as a stable branch so we just compare the input major version
    CLANG_VERSION_PARTS=(${CLANG_VERSION//./ })
    case ${CLANG_VERSION_PARTS[0]} in

      12)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20210107.zip"
        ;;

      11)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20200714.zip"
        ;;

      10)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20200108.zip"
        ;;

      9)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20190619.zip"
        ;;

      8)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20190104.zip"
        ;;

      7)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20180801.zip"
        ;;

      6)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20180103.zip"
        ;;

      5)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20170719.zip"
        ;;

      4)
        CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20170116.zip"
        ;;

      *)
        echo "Unknown Apple Clang version $CLANG_VERSION!" 1>&2
        exit 1
        ;;
    esac
  fi
  
  # after we generate the URL string above we need to actually check it works
  if [ ! -f $(basename $CLANG_LLVM_PKG) ] && [ $(curl --head -L $CLANG_LLVM_PKG -o /dev/stderr -w "%{http_code}" 2> /dev/null) -ne 200 ]; then
    echo "Release $CLANG_VERSION not found in $GITPROJECT repo!" 1>&2
    exit 1
  fi

  popd &>/dev/null #$TARBALL_DIR
}

set_package_link

if [ -z "CLANG_LLVM_PKG" ]; then
  echo "Release $CLANG_VERSION not found!" 1>&2
  exit 1
fi


function warn_if_installed()
{
  set +e
  command -v $1 &>/dev/null && \
  {
    echo ""
    echo "It is highly recommended to uninstall previous $2 versions first:"
    echo "-> $(command -v $1 2>/dev/null)"
    echo ""
  }
  set -e
}

if [ $PLATFORM != "Darwin" -a $PLATFORM != "FreeBSD" ]; then
  warn_if_installed clang clang
  warn_if_installed llvm-config llvm
fi


echo "Building Clang/LLVM $GITPROJECT-$CLANG_VERSION may take a long time."
echo "Installation Prefix: $INSTALLPREFIX"

if [ -z "$UNATTENDED" ]; then
  echo ""
  read -p "Press enter to start building."
  echo ""
fi

# download the GitHub repo as a ZIP file - but only if it doesn't exist already
pushd $TARBALL_DIR &>/dev/null

if [ ! -f $(basename $CLANG_LLVM_PKG) ]; then
  download $CLANG_LLVM_PKG
fi

popd &>/dev/null #$TARBALL_DIR

# extract ZIP using bsdtar so we can remove the parent directory
pushd $BUILD_DIR &>/dev/null

echo "extracting ..."

bsdtar --strip-components=1 -xf $TARBALL_DIR/$(basename $CLANG_LLVM_PKG) 1>/dev/null

# DISABLE_BOOTSTRAP no longer available
# ENABLE_FULL_BOOTSTRAP no longer available

if [ -z "$PORTABLE" ]; then
  export CFLAGS="-march=native"
  export CXXFLAGS="-march=native"
fi

# build clang, llvm, libc++ and libc++abi in one go
mkdir build_stage
pushd build_stage &>/dev/null
cmake ../llvm \
  -G "Unix Makefiles" \
  -DCMAKE_INSTALL_PREFIX="$INSTALLPREFIX" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=OFF \
  -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi" \
  -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=1
$MAKE -j $JOBS VERBOSE=1

# install, but only if it is globally enabled
if [ -z "$ENABLE_CLANG_INSTALL" ]; then
  echo ""
  echo "Done!"
  echo ""
  echo -n "cd into '$BUILD_DIR/build_stage' and type 'make install' to install "
  echo "clang/llvm to '$INSTALLPREFIX'"
  echo ""
else
  $MAKE install -j $JOBS VERBOSE=1
  echo ""
  echo "Done!"
  echo ""
fi
popd &>/dev/null #build_stage

popd &>/dev/null #$BUILD_DIR

popd &>/dev/null #"${0%/*}"
