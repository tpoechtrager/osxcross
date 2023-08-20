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
  CLANG_VERSION=16.0.6
fi

if [ -z "$INSTALLPREFIX" ]; then
  INSTALLPREFIX="/usr/local"
fi

# acceptable values are llvm or apple
if [ -z "$GITPROJECT" ]; then
  GITPROJECT="llvm"
fi

require cmake
require curl

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
  
  if [ $GITPROJECT == "llvm" ]; then
    # with official LLVM we just pass the version straight into the URL
    CLANG_LLVM_PKG="https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-$CLANG_VERSION.zip"
  elif [ $GITPROJECT == "apple" ]; then
    # with Apple LLVM we only get each major version as a stable branch so we just compare the input major version
    CLANG_VERSION_PARTS=(${CLANG_VERSION//./ })
    case ${CLANG_VERSION_PARTS[0]} in

      17) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/stable/20230725.zip" ;;
      16) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/stable/20221013.zip" ;;
      15) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/stable/20220421.zip" ;;
      14) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/stable/20211026.zip" ;;
      13) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/stable/20210726.zip" ;;
      12) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20210107.zip" ;;
      11) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20200714.zip" ;;
      10) CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20200108.zip" ;;
      9)  CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20190619.zip" ;;
      8)  CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20190104.zip" ;;
      7)  CLANG_LLVM_PKG="https://github.com/apple/llvm-project/archive/refs/heads/apple/stable/20180801.zip" ;;

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

echo "Building Clang/LLVM $GITPROJECT-$CLANG_VERSION (this may take a long time)."
echo "Installation Prefix: $INSTALLPREFIX [INSTALLPREFIX=<Prefix>]"

echo ""

if [ -z "$UNATTENDED" ]; then
  if prompt "Perform two stage build? (recommended)"; then
    echo "Enabling two stage build ..."
    ENABLE_BOOTSTRAP=1
  else
    echo "Disabling two stage build ..."
  fi

  echo ""
  read -p "Press enter to start building."
  echo ""
else
  ENABLE_BOOTSTRAP=1
fi

# download the GitHub repo as a ZIP file - but only if it doesn't exist already
pushd $TARBALL_DIR &>/dev/null

if [ ! -f $(basename $CLANG_LLVM_PKG) ]; then
  download $CLANG_LLVM_PKG
fi

popd &>/dev/null #$TARBALL_DIR

# extract ZIP
pushd $BUILD_DIR &>/dev/null

rm -rf "clang-$CLANG_VERSION"
mkdir "clang-$CLANG_VERSION"
pushd "clang-$CLANG_VERSION" &>/dev/null

echo "extracting ..."
extract $TARBALL_DIR/$(basename $CLANG_LLVM_PKG)

# Various Buildfixes

if ([[ $CLANG_VERSION == 15* ]] || [[ $CLANG_VERSION == 14* ]] ||
    [[ $CLANG_VERSION == 13* ]] || [[ $CLANG_VERSION == 12* ]] ||
    [[ $CLANG_VERSION == 11* ]] || [[ $CLANG_VERSION == 10* ]]); then
  $SED -i 's/#include <string>/#include <string>\
\ #include <cstdint>/' *llvm*/llvm/include/llvm/Support/Signals.h
fi

if ([[ $CLANG_VERSION == 11* ]] || [[ $CLANG_VERSION == 10* ]] ||
    [[ $CLANG_VERSION == 9* ]] || [[ $CLANG_VERSION == 8* ]]); then
  $SED -i 's/#include <vector>/#include <vector>\
\ #include <limits>/' *llvm*/llvm/utils/benchmark/src/benchmark_register.h
fi

if ([[ $CLANG_VERSION == 9* ]] || [[ $CLANG_VERSION == 8* ]]); then
  $SED -i 's/#include <array>/#include <array>\
\ #include <cstdint>\
\ #include <string>/' *llvm*/llvm/include/llvm/Demangle/MicrosoftDemangleNodes.h
fi

function build()
{
  stage=$1
  mkdir -p $stage
  pushd $stage &>/dev/null
  cmake ../*llvm*/llvm \
    -DCMAKE_INSTALL_PREFIX=$INSTALLPREFIX \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_TARGETS_TO_BUILD="X86;AArch64;ARM" \
    -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=1
  $MAKE $2 -j $JOBS
  popd &>/dev/null
}

export CFLAGS=""
export CXXFLAGS=""

if [ -z "$PORTABLE" ]; then
  export CFLAGS+=" -march=native"
  export CXXFLAGS+=" -march=native"
fi

# Silence warnings to get a cleaner build state output
if [ -z "$ENABLE_COMPILER_WARNINGS" ]; then
  export CFLAGS+=" -w"
  export CXXFLAGS+=" -w"
fi

if [ -z "$ENABLE_BOOTSTRAP" ]; then
  build build
else
  build build_stage1 clang

  export CC=$PWD/build_stage1/bin/clang
  export CXX=$PWD/build_stage1/bin/clang++

  build build_stage2

  if [ -n "$ENABLE_FULL_BOOTSTRAP" ]; then
    CC=$PWD/build_stage2/bin/clang \
    CXX=$PWD/build_stage2/bin/clang++ \
    build build_stage3
  fi
fi

# install, but only if it is globally enabled
if [ -z "$ENABLE_CLANG_INSTALL" ]; then
  echo ""
  echo "Done!"
  echo ""
  echo -n "cd into '$BUILD_DIR/clang-$CLANG_VERSION/$stage' and type 'make install' to install "
  echo "clang/llvm to '$INSTALLPREFIX'"
  echo ""
else
  $MAKE install -j $JOBS VERBOSE=1
  echo ""
  echo "Done!"
  echo ""
fi
