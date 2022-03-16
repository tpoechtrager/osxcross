#!/usr/bin/env bash

export LC_ALL="C"

function set_path_vars()
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
    # Do not use these
    unset OSXCROSS_VERSION OSXCROSS_OSX_VERSION_MIN
    unset OSXCROSS_TARGET OSXCROSS_BASE_DIR
    unset OSXCROSS_SDK_VERSION OSXCROSS_SDK
    unset OSXCROSS_SDK_DIR OSXCROSS_TARBALL_DIR
    unset OSXCROSS_PATCH_DIR OSXCROSS_TARGET_DIR
    unset OSXCROSS_BUILD_DIR OSXCROSS_CCTOOLS_PATH
    unset OSXCROSS_LIBLTO_PATH OSXCROSS_LINKER_VERSION
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

set_path_vars

PLATFORM=$(uname -s)
ARCH=$(uname -m)
OPERATING_SYSTEM=$(uname -o 2>/dev/null || echo "-")
SCRIPT=$(basename $0)

if [[ $PLATFORM == CYGWIN* ]]; then
  echo "Cygwin is no longer supported." 1>&2
  exit 1
fi

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
require $CMAKE
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
    set_path_vars
  fi
fi


# find sdk version to use
function guess_sdk_version()
{
  tmp1=
  tmp2=
  tmp3=
  file=
  sdk=
  guess_sdk_version_result=
  sdkcount=$(find -L tarballs/ -type f | grep MacOSX | wc -l)
  if [ $sdkcount -eq 0 ]; then
    echo no SDK found in 'tarballs/'. please see README.md
    exit 1
  elif [ $sdkcount -gt 1 ]; then
    sdks=$(find -L tarballs/ -type f | grep MacOSX)
    for sdk in $sdks; do echo $sdk; done
    echo 'more than one MacOSX SDK tarball found. please set'
    echo 'SDK_VERSION environment variable for the one you want'
    echo '(for example: SDK_VERSION=10.x [OSX_VERSION_MIN=10.x] [TARGET_DIR=...] ./build.sh)'
    exit 1
  else
    sdk=$(find -L tarballs/ -type f | grep MacOSX)
    tmp2=$(echo ${sdk/bz2/} | $SED s/[^0-9.]//g)
    tmp3=$(echo $tmp2 | $SED s/\\\.*$//g)
    guess_sdk_version_result=$tmp3
    echo 'found SDK version' $guess_sdk_version_result 'at tarballs/'$(basename $sdk)
  fi
  if [ $guess_sdk_version_result ]; then
    if [ $guess_sdk_version_result = 10.4 ]; then
      guess_sdk_version_result=10.4u
    fi
  fi
  export guess_sdk_version_result
}

# make sure there is actually a file with the given SDK_VERSION
function verify_sdk_version()
{
  sdkv=$1
  for file in tarballs/*; do
    if [ -f "$file" ] && [ $(echo $file | grep OSX.*$sdkv) ]; then
      echo "verified at "$file
      sdk=$file
    fi
  done
  if [ ! $sdk ] ; then
    echo cant find SDK for OSX $sdkv in tarballs. exiting
    exit 1
  fi
}


function extract()
{
  echo "extracting $(basename $1) ..."

  local tarflags

  tarflags="xf"
  test -n "$OCDEBUG" && tarflags+="v"

  case $1 in
    *.tar.xz)
      xz -dc $1 | tar $tarflags -
      ;;
    *.tar.gz)
      gunzip -dc $1 | tar $tarflags -
      ;;
    *.tar.bz2)
      bzip2 -dc $1 | tar $tarflags -
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

# f_res=1 = something has changed upstream
# f_res=0 = nothing has changed

function git_clone_repository
{
  local url=$1
  local branch=$2
  local project_name=$3

  if [ -n "$TP_OSXCROSS_DEV" ]; then
    # copy files from local working directory
    rm -rf $project_name
    cp -r $TP_OSXCROSS_DEV/$project_name .
    if [ -e ${project_name}/.git ]; then
      pushd $project_name &>/dev/null
      git clean -fdx &>/dev/null
      popd &>/dev/null
    fi
    f_res=1
    return
  fi

  local git_extra_opts=""

  if [ -z "$FULL_CLONE" ]; then
    git_extra_opts="--depth 1 "
  fi

  if [ ! -d $project_name ]; then
    git clone $url $project_name $git_extra_opts
  fi

  pushd $project_name &>/dev/null

  git reset --hard &>/dev/null
  git clean -fdx &>/dev/null

  if git show-ref refs/heads/$branch &>/dev/null; then
    git fetch origin $branch
  else
    git fetch origin $branch:$branch $git_extra_opts
  fi
  
  git checkout $branch
  git pull origin $branch

  local new_hash=$(git rev-parse HEAD)
  local old_hash=""
  local hash_file="$BUILD_DIR/.${project_name}_git_hash"

  if [ -f $hash_file ]; then
    old_hash=$(cat $hash_file)
  fi

  echo -n $new_hash > $hash_file

  if [ "$old_hash" != "$new_hash" ]; then
    f_res=1
  else
    f_res=0
  fi

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

function build_success()
{
  local project_name=$1
  touch "$BUILD_DIR/.${CURRENT_BUILD_PROJECT_NAME}_build_complete"
  unset CURRENT_BUILD_PROJECT_NAME
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

# f_res=1 = build the project
# f_res=0 = nothing to do

function get_sources()
{
  local url="$1"
  local branch="$2"
  local project_name="$3"
  local build_complete_file

  if [[ -z "${project_name}" ]]; then
    project_name=$(get_project_name_from_url "${url}")
  fi
  build_complete_file="${BUILD_DIR}/.${project_name}_build_complete"

  CURRENT_BUILD_PROJECT_NAME="${project_name}"

  build_msg "${project_name}" "${branch}"

  if [[ "${SKIP_BUILD}" == *${project_name}* ]]; then
    f_res=0
    return
  fi

  git_clone_repository "${url}" "${branch}" "${project_name}"

  if [[ $f_res -eq 1 ]]; then
    rm -f "${build_complete_file}"
    f_res=1
  else
    # nothing has changed upstream

    if [[ -f "${build_complete_file}" ]]; then
      echo ""
      echo "## Nothing to do ##"
      echo ""
      f_res=0
    else
      rm -f "${build_complete_file}"
      f_res=1
    fi
  fi
}

function download()
{
  local uri=$1
  local filename=$(basename $1)

  if command -v curl &>/dev/null; then
    ## cURL ##
    local curl_opts="-L -C - "
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
  ln -sf $1 $2
}


function verbose_cmd()
{
  echo "$@"
  eval "$@"
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

## Also used in gen_sdk_package_pbzx.sh ##

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
    build_success
  fi

  popd &>/dev/null
}



# exit on error
set -e
