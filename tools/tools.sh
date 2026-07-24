#!/usr/bin/env bash

export LC_ALL="C"

function import_osxcross_environment()
{
  if [ -n "$OSXCROSS_VERSION" ]; then
    export VERSION=$OSXCROSS_VERSION
    export OSX_VERSION_MIN=$OSXCROSS_OSX_VERSION_MIN
    export TARGET=$OSXCROSS_TARGET
    export BASE_DIR=$OSXCROSS_BASE_DIR
    export TARBALL_DIR=$OSXCROSS_TARBALL_DIR
    export BUILD_DIR=$OSXCROSS_BUILD_DIR
    export TARGET_DIR=$OSXCROSS_TARGET_DIR
    export TARGET_DIR_SDK_TOOLS=$OSXCROSS_TARGET_DIR/SDK/tools
    export PATCH_DIR=$OSXCROSS_PATCH_DIR
    export SDK_DIR=$OSXCROSS_SDK_DIR
    export SDK_VERSION=$OSXCROSS_SDK_VERSION
    export SDK=$OSXCROSS_SDK
    export LIBLTO_PATH=$OSXCROSS_LIBLTO_PATH
    export LINKER_VERSION=$OSXCROSS_LINKER_VERSION
    export BUILD_FLAVOR=$OSXCROSS_BUILD_FLAVOR
    # Do not use these
    unset OSXCROSS_VERSION OSXCROSS_OSX_VERSION_MIN
    unset OSXCROSS_TARGET OSXCROSS_BASE_DIR
    unset OSXCROSS_SDK_VERSION OSXCROSS_SDK
    unset OSXCROSS_SDK_DIR OSXCROSS_TARBALL_DIR
    unset OSXCROSS_PATCH_DIR OSXCROSS_TARGET_DIR
    unset OSXCROSS_BUILD_DIR OSXCROSS_CCTOOLS_PATH
    unset OSXCROSS_LIBLTO_PATH OSXCROSS_LINKER_VERSION
    unset OSXCROSS_BUILD_FLAVOR
  else
    export BASE_DIR=$PWD
    export TARBALL_DIR=$PWD/tarballs
    export BUILD_DIR=$PWD/build
    export TARGET_DIR=${TARGET_DIR:-$BASE_DIR/target}
    export TARGET_DIR_SDK_TOOLS=$TARGET_DIR/SDK/tools
    export PATCH_DIR=$PWD/patches
    export SDK_DIR=$TARGET_DIR/SDK
  fi
}

import_osxcross_environment

PLATFORM=$(uname -s)
ARCH=$(uname -m)
OPERATING_SYSTEM=$(uname -o 2>/dev/null || echo "-")
SCRIPT=$(basename $0)

if [[ $PLATFORM == CYGWIN* ]]; then
  echo "Cygwin is no longer supported." 1>&2
  exit 1
fi

if [[ $PLATFORM == Darwin ]]; then
  echo $PATH
  CFLAGS_OPENSSL="$(pkg-config --cflags openssl)"
  LDFLAGS_OPENSSL="$(pkg-config --libs-only-L openssl)"
  export C_INCLUDE_PATH=${CFLAGS_OPENSSL:2}
  export CPLUS_INCLUDE_PATH=${CFLAGS_OPENSSL:2}
  export LIBRARY_PATH=${LDFLAGS_OPENSSL:2}
fi

# Check whether an architecture is included in a space-separated list.
#
# Usage:
#   arch_supported "<arch>"
#     Uses SUPPORTED_ARCHS, falling back to OSXCROSS_SUPPORTED_ARCHS.
#
#   arch_supported "<supported archs>" "<arch>"
#     Uses the explicitly provided list instead of the global variables.
#
# GCC calls arm64 "aarch64", so aarch64 is normalized to arm64 in both the
# list and the architecture being checked. Returns 0 when supported, 1 when
# unsupported and 2 when called with an invalid number of arguments.
function arch_supported() {
  local supported_archs
  local arch

  case $# in
    1)
      arch=$1
      supported_archs=${SUPPORTED_ARCHS:-$OSXCROSS_SUPPORTED_ARCHS}
      ;;
    2)
      supported_archs=$1
      arch=$2
      ;;
    *)
      echo "Usage: arch_supported [<supported archs>] <arch to check>" 1>&2
      return 2
      ;;
  esac

  if [ "$arch" = "aarch64" ]; then
    arch=arm64
  fi

  supported_archs=${supported_archs//aarch64/arm64}

  [[ " $supported_archs " == *" $arch "* ]]
}

function first_supported_arch() {
  echo "${SUPPORTED_ARCHS%% *}"
}

# Compare two dotted versions without external tools; this replaces the old
# osxcross-cmp wrapper program and prints 1 when the comparison is true, else 0.
function cmp-version() {
  (( $# >= 3 )) || return 1

  local lhs=$1 op=$2 rhs=$3
  local -a parts values av=(0 0 0) bv=(0 0 0)
  local version part sign digits n i operand

  operand=0
  for version in "$lhs" "$rhs"; do
    parts=()
    values=(0 0 0)
    IFS='.' read -r -a parts <<< "$version"

    for i in 0 1 2; do
      part=${parts[i]:-0}
      n=0

      if [[ $part =~ ^[[:space:]]*([+-]?)([0-9]+) ]]; then
        sign=${BASH_REMATCH[1]}
        digits=${BASH_REMATCH[2]}

        while [[ $digits == 0* && $digits != 0 ]]; do
          digits=${digits#0}
        done

        n=$((10#$digits))
        [[ $sign == "-" ]] && n=$((-n))
      fi

      values[i]=$n
    done

    if (( operand == 0 )); then
      av=("${values[@]}")
    else
      bv=("${values[@]}")
    fi
    ((operand += 1))
  done

  local a=$((av[0] * 10000 + av[1] * 100 + av[2]))
  local b=$((bv[0] * 10000 + bv[1] * 100 + bv[2]))
  local result

  case "$op" in
    '>')  result=$((a > b)) ;;
    '<')  result=$((a < b)) ;;
    '>=') result=$((a >= b)) ;;
    '<=') result=$((a <= b)) ;;
    '==') result=$((a == b)) ;;
    '!=') result=$((a != b)) ;;
    *) return 1 ;;
  esac

  printf '%d' "$result"
}

function require()
{
  if ! command -v $1 &>/dev/null; then
    echo "Required dependency '$1' is not installed" 1>&2
    exit 1
  fi
}

if [[ $PLATFORM == *BSD ]] || [ $PLATFORM == "DragonFly" ]; then
  MAKE=gmake
  SED=gsed
else
  MAKE=make
  SED=sed
fi

if [ -z "$USESYSTEMCOMPILER" ]; then

  if [ -z "$CC" ]; then
    export CC="clang"
  fi

  if [ -z "$CXX" ]; then
    export CXX="clang++"
  fi
fi

if [ -z "$CMAKE" ]; then
  CMAKE="cmake"
fi

if [ -n "$CC" ]; then
  require $CC
fi

if [ -n "$CXX" ]; then
  require $CXX
fi

require $SED
require $MAKE
require patch
require gunzip


# enable debug messages
[ -n "$OCDEBUG" ] && set -x

# how many concurrent jobs should be used for compiling?
if [ -z "$JOBS" ]; then
  JOBS=$(tools/get_cpu_count.sh || echo 1)
fi

# Don't run osxcross-conf for the top build.sh script
if [ $SCRIPT != "build.sh" ]; then
  res=$(tools/osxcross_conf.sh || echo "")

  if [ -z "$res" ] &&
      [[ $SCRIPT != gen_sdk_package*.sh ]] &&
      [ $SCRIPT != "build_wrapper.sh" ] &&
      [[ $SCRIPT != build*_clang.sh ]] &&
      [ $SCRIPT != "mount_xcode_image.sh" ]; then
    echo "you must run ./build.sh first before you can start building $DESC"
    exit 1
  fi

  if [ -z "$TOP_BUILD_SCRIPT" ]; then
    eval "$res"
    import_osxcross_environment
  fi
fi


# Detects the SDK archive in TARBALL_DIR and derives its macOS version.
# Fails if no SDK is found or if multiple SDKs are present without an
# explicit SDK_VERSION. The detected version is stored in and exported as
# guess_sdk_version_result.
function guess_sdk_version()
{
  local file=
  local sdk=
  local sdkcount=0
  local -a sdks=()
  guess_sdk_version_result=

  while IFS= read -r -d '' sdk; do
    file=$(basename "$sdk")
    if [[ $file =~ ^MacOSX[0-9]+(\.[0-9]+)*u?(\.sdk)?\.tar\.(xz|gz|bz2)$ ]]; then
      sdks+=("$sdk")
    fi
  done < <(find -L "$TARBALL_DIR" -type f -name 'MacOSX*' -print0)

  sdkcount=${#sdks[@]}
  if [ $sdkcount -eq 0 ]; then
    echo "Error: No SDK found in '$TARBALL_DIR'. Please see README.md."
    exit 1
  elif [ $sdkcount -gt 1 ]; then
    echo "Found the following SDKs:"
    echo ""
    for sdk in "${sdks[@]}"; do
      echo "$sdk"
    done
    echo ""

    echo "Please set the SDK_VERSION environment variable for the one you want."
    echo ""
    echo "(for example: SDK_VERSION=10.x [OSX_VERSION_MIN=10.x] [TARGET_DIR=...] ./build.sh)"
    exit 1
  else
    sdk=${sdks[0]}
    file=$(basename "$sdk")
    if [[ $file =~ ^MacOSX([0-9]+(\.[0-9]+)*) ]]; then
      guess_sdk_version_result=${BASH_REMATCH[1]}
    fi
    #echo "Found SDK version $guess_sdk_version_result at $TARBALL_DIR/$file"
  fi
  if [ "$guess_sdk_version_result" = 10.4 ]; then
    guess_sdk_version_result=10.4u
  fi
  export guess_sdk_version_result
}

# Locates the SDK archive matching SDK_VERSION in TARBALL_DIR and stores its
# path in $SDK. Accepts either a full version such as 10.15 or a major version
# such as 27. Terminates the build if no matching SDK archive is found.
function set_and_verify_sdk_path()
{
  if [[ $SDK_VERSION == *.* ]]; then
    SDK=$(ls $TARBALL_DIR/MacOSX$SDK_VERSION* 2>/dev/null || echo "")
  else
    SDK=$(ls $TARBALL_DIR/MacOSX$SDK_VERSION.* 2>/dev/null | grep -v "\.[0-9]\+" || echo "")
  fi

  if [ -z "$SDK" ] ; then
    echo "Error: Can't find SDK for macOS $SDK_VERSION in '$TARBALL_DIR'." 1>&2
    exit 1
  fi
}

# Detects an existing OSXCross installation in TARGET_DIR or PATH.
# Reads its target and build directories, warns about reusing them,
# and asks for confirmation unless running in unattended mode.
function check_for_existing_osxcross_installation()
{
  EXISTING_OSXCROSS_CONF=""

  if [ -x "$TARGET_DIR/bin/osxcross-conf" ]; then
    EXISTING_OSXCROSS_CONF="$TARGET_DIR/bin/osxcross-conf"
  elif EXISTING_OSXCROSS_CONF=$(command -v osxcross-conf 2>/dev/null); then
    :
  fi

  if [ -z "$EXISTING_OSXCROSS_CONF" ]; then
    return 1
  fi

  IFS=$'\t' read -r \
    EXISTING_OSXCROSS_TARGET_DIR \
    EXISTING_OSXCROSS_BUILD_DIR < <(
    (
      eval "$("$EXISTING_OSXCROSS_CONF")"

      printf '%s\t%s\n' \
        "$(realpath "$OSXCROSS_TARGET_DIR")" \
        "$(realpath "$OSXCROSS_BUILD_DIR")"
    )
  )

  echo ""
  echo "WARNING: Existing OSXCross installation detected"
  echo "------------------------------------------------"
  echo "The following directories already contain an"
  echo "OSXCross installation:"
  echo ""
  printf "%-7s: %s\n" "Target" "$EXISTING_OSXCROSS_TARGET_DIR"
  printf "%-7s: %s\n" "Build" "$EXISTING_OSXCROSS_BUILD_DIR"
  echo ""
  echo "Remove these directories before rebuilding."
  echo "Reusing them may produce an inconsistent or unreliable build."
  echo ""

  if [ "$UNATTENDED" = "1" ]; then
    echo "UNATTENDED=1: continuing without confirmation."
    return 0
  else
    read -r -p "Continue without removing them? [y/N] " response

    case "$response" in
      y|Y|yes|YES) ;;
      *)
        echo ""
        echo "Build cancelled."
        exit 0
        ;;
    esac

    return 0
  fi

  return 0
}


function extract()
{
  echo "extracting $(basename $1) ..."

  local tarflags

  tarflags="xf"
  test -n "$OCDEBUG" && tarflags+="v"

  case $1 in
    *.tar.xz)
      tar $tarflags $1
      ;;
    *.tar.gz)
      tar $tarflags $1
      ;;
    *.tar.bz2)
      tar $tarflags $1
      ;;
    *.zip)
      unzip $1
      ;;
    *)
      echo "Unhandled archive type" 2>&1
      exit 1
      ;;
  esac
}


function get_exec_dir()
{
  local dirs=$(dirs)
  echo ${dirs##* }
}

function make_absolute_path()
{
  local current_path

  if [ $# -eq 1 ]; then
    current_path=$PWD
  else
    current_path=$2
  fi

  case $1 in
    /*) echo "$1" ;;
     *) echo "${current_path}/$1" ;;
  esac
}

function cleanup_tmp_dir()
{
  if [ -n "$OC_KEEP_TMP_DIR" ]; then
      echo "Not removing $TMP_DIR ..."
      return
  fi
  echo "Removing $TMP_DIR ..."
  rm -rf $TMP_DIR
}

function create_tmp_dir()
{
  mkdir -p $BUILD_DIR
  pushd $BUILD_DIR &>/dev/null
  local tmp

  for i in {1..100}; do
    tmp="tmp_$RANDOM"
    [ -e $tmp ] && continue
    mkdir $tmp && break
  done

  if [ ! -d $tmp ]; then
    echo "cannot create $BUILD_DIR/$tmp directory" 1>&2
    exit 1
  fi

  TMP_DIR=$BUILD_DIR/$tmp
  trap cleanup_tmp_dir EXIT

  popd &>/dev/null
}

# Clone or update a source repository and check out the requested branch.
#
# Usage:
#   git_clone_repository "<url>" ["<branch>"] "<project name>"
#
# When no branch is supplied, the repository's default branch is used.
# A shallow clone is used unless FULL_CLONE is set.
function git_clone_repository
{
  local url="$1"
  local branch="${2:-}"
  local project_name="$3"

  if [ -n "$TP_OSXCROSS_DEV" ] && [ -d "$TP_OSXCROSS_DEV/$project_name" ] ; then
    # copy files from local working directory
    rm -rf $project_name
    cp -r $TP_OSXCROSS_DEV/$project_name .
    if [ -e ${project_name}/.git ]; then
      pushd $project_name &>/dev/null
      git clean -fdx &>/dev/null
      popd &>/dev/null
    fi
    return
  fi

  local git_extra_opts=""

  if [ -z "$FULL_CLONE" ]; then
    git_extra_opts="--depth 1 "
  fi

  if [ ! -d "$project_name" ]; then
    if [ -n "$branch" ]; then
      git clone $git_extra_opts --branch "$branch" "$url" "$project_name"
    else
      git clone $git_extra_opts "$url" "$project_name"
    fi
  fi

  pushd "$project_name" &>/dev/null

  git reset --hard &>/dev/null
  git clean -fdx &>/dev/null

  if [ -z "$branch" ]; then
    branch=$(git symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null)
    branch=${branch#origin/}
  fi

  if [ -z "$branch" ]; then
    echo "Unable to determine the default branch for $url" 1>&2
    popd &>/dev/null
    return 1
  fi

  if git show-ref "refs/heads/$branch" &>/dev/null; then
    git fetch origin "$branch"
  else
    git fetch origin "$branch:$branch" $git_extra_opts
  fi
  
  git checkout "$branch"
  git pull origin "$branch"

  popd &>/dev/null
}

function get_project_name_from_url()
{
  local url=$1
  local project_name
  project_name=$(basename $url)
  project_name=${project_name/\.git/}
  echo -n $project_name
}

function build_msg()
{
  echo ""

  if [ $# -eq 2 ]; then
    echo "## Building $1 ($2) ##"
  else
    echo "## Building $1 ##"
  fi

  echo "" 
}

# Get the sources for a build project.
#
# Usage:
#   get_sources "<url>" ["<branch>"] ["<project name>"]
#
# The project name defaults to the repository name. When no branch is
# supplied, the repository's default branch is used.
#
# f_res=1 = build the project
# f_res=0 = skip the project
function get_sources()
{
  local url="$1"
  local branch="${2:-}"
  local project_name="${3:-}"

  if [[ -z "${project_name}" ]]; then
    project_name=$(get_project_name_from_url "${url}")
  fi
  CURRENT_BUILD_PROJECT_NAME="${project_name}"

  if [ -n "${branch}" ]; then
    build_msg "${project_name}" "${branch}"
  else
    build_msg "${project_name}"
  fi

  if [[ "${SKIP_BUILD}" == *${project_name}* ]]; then
    f_res=0
    return
  fi

  git_clone_repository "${url}" "${branch}" "${project_name}"
  f_res=1
}

function download()
{
  local uri=$1
  local filename=$(basename $1)

  if command -v curl &>/dev/null; then
    ## cURL ##
    local curl_opts="-fL -C - "
    curl $curl_opts -o $filename $uri
  elif command -v wget &>/dev/null; then
    ## wget ##
    local wget_opts="-c "
    local output=$(wget --no-config 2>&1)
    if [[ $output != *--no-config* ]]; then
      wget_opts+="--no-config "
    fi
    wget $wget_opts -O $filename $uri
  else
    echo "Required dependency 'curl or wget' not installed" 1>&2
    exit 1
  fi
}

function create_symlink()
{
  if [ "$1" = "$2" ]; then
    echo "Symlink target and source are identical. Rebuild from scratch."
    exit 1
  fi
  ln -sf "$1" "$2"
}

function install_cmake_toolchain_files()
{
  local compiler=$1
  local arch
  local variants_cmake_suffix
  local cmake_suffix

  if [ -z "$compiler" ]; then
    echo "Usage: install_cmake_toolchain_files <compiler> [arch ...]" 1>&2
    return 1
  fi

  shift

  case "$compiler" in
    clang)
      variants_cmake_suffix=("" "-clang" "-clang-libc++" "-clang-gstdc++")
      ;;
    gcc)
      variants_cmake_suffix=("-gcc" "-gcc-libc++")
      ;;
    *)
      echo "Unsupported CMake compiler: '$compiler'" 1>&2
      return 1
      ;;
  esac

  cp -f "$BASE_DIR/tools/toolchain.cmake" "$TARGET_DIR/"
  cp -f "$BASE_DIR/tools/osxcross-cmake" "$TARGET_DIR/bin/"
  chmod 755 "$TARGET_DIR/bin/osxcross-cmake"

  for arch in "$@"; do
    for cmake_suffix in "${variants_cmake_suffix[@]}"; do
      create_symlink osxcross-cmake \
                     "$TARGET_DIR/bin/$arch-apple-$TARGET-cmake$cmake_suffix"

      # GCC also exposes an aarch64 -> arm64 alias because GCC itself
      # is built using the aarch64-apple-darwin-* triple.
      if [ "$compiler" = "gcc" ] && [ "$arch" = "aarch64" ]; then
        create_symlink osxcross-cmake \
                       "$TARGET_DIR/bin/arm64-apple-$TARGET-cmake$cmake_suffix"
      fi
    done
  done
}

function verbose_cmd()
{
  echo "$@"
  eval "$@"
}

# Function for yes/no prompt with default 'yes'
function prompt()
{
  while true; do
    read -p "$1 [Y/n]: " yn
    case $yn in
      [Yy]* | "" ) return 0;;  # Default to 'yes' if empty input
      [Nn]* ) return 1;;
      * ) echo "Please answer yes or no.";;
    esac
  done
}


function test_compiler()
{
  if [ "$3" != "required" ]; then
    set +e
  fi

  echo -ne "testing $1 ... "
  $1 $2 -O2 -Wall -o test

  if [ $? -eq 0 ]; then
    rm test
    echo "works"
  else
    echo "failed (ignored)"
  fi

  if [ "$3" != "required" ]; then
    set -e
  fi
}

function test_compiler_cxx11()
{
  set +e
  echo -ne "testing $1 -stdlib=libc++ -std=c++11 ... "
  $1 $2 -O2 -stdlib=libc++ -std=c++11 -Wall -o test &>/dev/null
  if [ $? -eq 0 ]; then
    rm test
    echo "works"
  else
    echo "failed (ignored)"
  fi
  set -e
}

function test_compiler_cxx2b()
{
  set +e
  echo -ne "testing $1 -std=c++20 -mmacos-version-min=$SDK_VERSION ... "
  $1 $2 -O2 -std=c++20 -mmacos-version-min=$SDK_VERSION -Wall -o test &>/dev/null
  if [ $? -eq 0 ]; then
    rm test
    echo "works"
  else
    echo "failed (ignored)"
  fi
  set -e
}



function build_xar()
{
  pushd $BUILD_DIR &>/dev/null

  get_sources https://github.com/tpoechtrager/xar.git master

  if [ $f_res -eq 1 ]; then
    pushd $CURRENT_BUILD_PROJECT_NAME/xar &>/dev/null
    CFLAGS+=" -w" \
      ./configure --prefix=$TARGET_DIR
    $MAKE -j$JOBS
    $MAKE install -j$JOBS
    popd &>/dev/null
  fi

  popd &>/dev/null
}

function build_p7zip()
{
  get_sources https://github.com/tpoechtrager/p7zip.git master

  if [ $f_res -eq 1 ]; then
    pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null

    if [ -n "$CC" ] && [ -n "$CXX" ]; then
      [[ $CC == *clang* ]] && CC="$CC -Qunused-arguments"
      [[ $CXX == *clang* ]] && CXX="$CXX -Qunused-arguments"
      $MAKE 7z -j $JOBS CC="$CC" CXX="$CXX -std=gnu++98"
    else
      $MAKE 7z -j $JOBS CXX="c++ -std=gnu++98"
    fi

    $MAKE install DEST_HOME=$TARGET_DIR_SDK_TOOLS
    find $TARGET_DIR_SDK_TOOLS/share -type f -exec chmod 0664 {} \;
    find $TARGET_DIR_SDK_TOOLS/share -type d -exec chmod 0775 {} \;
    popd &>/dev/null
  fi
}

function build_pbxz()
{
  get_sources https://github.com/tpoechtrager/pbzx.git master

  if [ $f_res -eq 1 ]; then
    pushd $CURRENT_BUILD_PROJECT_NAME &>/dev/null
    mkdir -p $TARGET_DIR_SDK_TOOLS/bin
    verbose_cmd $CC -O2 -Wall \
                -I $TARGET_DIR/include -L $TARGET_DIR/lib pbzx.c \
                -o $TARGET_DIR_SDK_TOOLS/bin/pbzx -llzma -lxar \
                -Wl,-rpath,$TARGET_DIR/lib
    popd &>/dev/null
  fi
}


# exit on error
set -e
