#!/usr/bin/env bash
#
# Build and install the "compiler-rt" runtime library.
#
# This requires that you already finished `build.sh`.
# Please refer to README.COMPILER-RT.md for details.
#

pushd "${0%/*}" &>/dev/null

DESC=compiler-rt
source tools/tools.sh
eval $(tools/osxcross_conf.sh)

if [ $PLATFORM == "Darwin" ]; then
  exit 1
fi

CLANG_VERSION=$(echo "__clang_major__ __clang_minor__ __clang_patchlevel__" | \
 xcrun clang -xc -E - | tail -n1 | tr ' ' '.')

# Drop patch level for <= 3.3.
if [ $(osxcross-cmp $CLANG_VERSION "<=" 3.3) -eq 1 ]; then
  CLANG_VERSION=$(echo $CLANG_VERSION | tr '.' ' ' |
                  awk '{print $1, $2}' | tr ' ' '.')
fi

CLANG_LIB_DIR=$(clang -print-search-dirs | grep "libraries: =" | \
                tr '=' ' ' | tr ':' ' ' | awk '{print $2}')

VERSION=$(echo "${CLANG_LIB_DIR}" | tr '/' '\n' | tail -n1)
CLANG_INCLUDE_DIR="${CLANG_LIB_DIR}/include"
CLANG_DARWIN_LIB_DIR="${CLANG_LIB_DIR}/lib/darwin"

USE_CMAKE=0

case $CLANG_VERSION in
  3.2* ) BRANCH=release/3.2.x ;;
  3.3* ) BRANCH=release/3.3.x ;;
  3.4* ) BRANCH=release/3.4.x ;;
  3.5* ) BRANCH=release/3.5.x ;;
  3.6* ) BRANCH=release/3.6.x ;;
  3.7* ) BRANCH=release/3.7.x ;;
  3.8* ) BRANCH=release/3.8.x;   USE_CMAKE=1; ;;
  3.9* ) BRANCH=release/3.9.x;   USE_CMAKE=1; ;;
  4.0* ) BRANCH=release/4.x;     USE_CMAKE=1; ;;
  5.0* ) BRANCH=release/5.x;     USE_CMAKE=1; ;;
  6.0* ) BRANCH=release/6.x;     USE_CMAKE=1; ;;
  7.*  ) BRANCH=release/7.x;     USE_CMAKE=1; ;;
  8.*  ) BRANCH=release/8.x;     USE_CMAKE=1; ;;
  9.*  ) BRANCH=release/9.x;     USE_CMAKE=1; ;;
  10.* ) BRANCH=release/10.x;    USE_CMAKE=1; ;;
  11.* ) BRANCH=release/11.x;    USE_CMAKE=1; ;;
  12.* ) BRANCH=release/12.x;    USE_CMAKE=1; ;;
  13.* ) BRANCH=release/13.x;    USE_CMAKE=1; ;;
  14.* ) BRANCH=release/14.x;    USE_CMAKE=1; ;;
  15.* ) BRANCH=main;            USE_CMAKE=1; ;;
     * ) echo "Unsupported Clang version, must be >= 3.2 and <= 15.0" 1>&2; exit 1;
esac

if [ $(osxcross-cmp $CLANG_VERSION ">=" 3.5) -eq 1 ]; then
  export MACOSX_DEPLOYMENT_TARGET=10.8 # x86_64h
else
  export MACOSX_DEPLOYMENT_TARGET=10.4
fi

if [ $(osxcross-cmp $MACOSX_DEPLOYMENT_TARGET ">" \
                    $SDK_VERSION) -eq 1 ];
then
  echo ">= $MACOSX_DEPLOYMENT_TARGET SDK required" 1>&2
  exit 1
fi

HAVE_OS_LOCK=0

if echo "#include <os/lock.h>" | xcrun clang -E - &>/dev/null; then
  HAVE_OS_LOCK=1
fi

export OSXCROSS_NO_10_5_DEPRECATION_WARNING=1

pushd $BUILD_DIR &>/dev/null

get_sources https://github.com/llvm/llvm-project.git $BRANCH "compiler-rt"

if [ $f_res -eq 1 ]; then
  pushd "$CURRENT_BUILD_PROJECT_NAME/compiler-rt" &>/dev/null

  if [ $(osxcross-cmp $SDK_VERSION "<=" 10.11) -eq 1 ]; then
    # https://github.com/tpoechtrager/osxcross/issues/178
    patch -p1 < $PATCH_DIR/compiler-rt_clock-gettime.patch
  fi

  EXTRA_MAKE_FLAGS=""
  if [ -n "$OCDEBUG" ]; then
    EXTRA_MAKE_FLAGS+="VERBOSE=1 "
  fi

  if [ $USE_CMAKE -eq 1 ]; then

    ### CMAKE ###

    $SED -i 's/COMMAND xcodebuild -version -sdk ${sdk_name}.internal Path/'\
\ \ \ \ \ \ \ 'COMMAND xcrun -sdk ${sdk_name}.internal --show-sdk-path/g' \
      cmake/Modules/CompilerRTDarwinUtils.cmake

    $SED -i 's/COMMAND xcodebuild -version -sdk ${sdk_name} Path/'\
\ \ \ \ \ \ \ 'COMMAND xcrun -sdk ${sdk_name} --show-sdk-path/g' \
      cmake/Modules/CompilerRTDarwinUtils.cmake

    $SED -i 's/COMMAND xcodebuild -version -sdk ${sdk_name}.internal SDKVersion/'\
\ \ \ \ \ \ \ 'COMMAND xcrun -sdk ${sdk_name}.internal --show-sdk-version/g' \
      cmake/Modules/CompilerRTDarwinUtils.cmake

    $SED -i 's/COMMAND xcodebuild -version -sdk ${sdk_name}.internal SDKVersion/'\
\ \ \ \ \ \ \ 'COMMAND xcrun -sdk ${sdk_name} --show-sdk-version/g' \
      cmake/Modules/CompilerRTDarwinUtils.cmake

    $SED -i "s/COMMAND lipo /COMMAND xcrun lipo /g" \
      cmake/Modules/CompilerRTDarwinUtils.cmake

    $SED -i "s/COMMAND ld /COMMAND xcrun ld /g" \
      cmake/Modules/CompilerRTDarwinUtils.cmake

    $SED -i "s/COMMAND codesign /COMMAND true /g" \
      cmake/Modules/AddCompilerRT.cmake

    $SED -i 's/${CMAKE_COMMAND} -E ${COMPILER_RT_LINK_OR_COPY}/ln -sf/g' \
      lib/builtins/CMakeLists.txt

    if [ -f "lib/orc/CMakeLists.txt" ]; then
      $SED -i 's/list(APPEND ORC_CFLAGS -I${DIR})//g' \
        lib/orc/CMakeLists.txt
    fi

    if [ $HAVE_OS_LOCK -eq 0 ]; then
      $SED -i "s/COMPILER_RT_HAS_TSAN TRUE/COMPILER_RT_HAS_TSAN FALSE/g" \
        cmake/config-ix.cmake
    fi

    function build
    {
      local arch=$1
      local build_dir="build"
      local extra_cmake_flags=""

      if [ -n "$arch" ]; then
        build_dir+="_$arch"

        extra_cmake_flags+="-DDARWIN_osx_ARCHS=$arch "
        extra_cmake_flags+="-DDARWIN_osx_BUILTIN_ARCHS=$arch "

        if [ $arch == "arm64" ] || [ $arch == "arm64e" ]; then
          # https://github.com/tpoechtrager/osxcross/issues/259
          extra_cmake_flags+="-DCOMPILER_RT_BUILD_SANITIZERS=OFF "
          extra_cmake_flags+="-DCOMPILER_RT_BUILD_XRAY=OFF "
        fi

        echo ""
        echo "Building for arch $arch ..."
        echo ""
      fi

      mkdir $build_dir
      pushd $build_dir &>/dev/null

      CC=$(xcrun -f clang) CXX=$(xcrun -f clang++) $CMAKE .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SYSTEM_NAME=Darwin \
        -DCMAKE_LIPO=$(xcrun -f lipo) \
        -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) \
        -DCMAKE_AR=$(xcrun -f ar) \
        $extra_cmake_flags

      $MAKE -j $JOBS $EXTRA_MAKE_FLAGS

      popd &>/dev/null
    }

    if [ $(osxcross-cmp $SDK_VERSION ">=" 11.0) -eq 1 ] &&
       [ $(osxcross-cmp $CLANG_VERSION ">=" 4.0) -eq 1 ]; then
      # https://github.com/tpoechtrager/osxcross/issues/258
      # https://github.com/tpoechtrager/osxcross/issues/286

      function check_archs
      {
        tmp=$(mktemp -d)
        [ -z "$tmp" ] && exit 1
        pushd $tmp &>/dev/null

        for arch in $*; do
          if echo "int main(){}" | xcrun clang -arch $arch -xc -o test - &>/dev/null; then
            rm test
            [ -n "$ARCHS" ] && ARCHS+=" "
            ARCHS+="$arch"
          fi
        done

        popd &>/dev/null
        rmdir $tmp
      }

      ARCHS=""
      check_archs i386 x86_64 x86_64h arm64 arm64e

      if [ -z "$ARCHS" ]; then
        echo "Compiler does not seem to work"
        exit 1
      fi

      echo ""
      echo "Building for archs $ARCHS ..."
      echo ""

      if [ -z "$DISABLE_PARALLEL_ARCH_BUILD" ] && [ $JOBS -gt 2 ]; then
        build_pids="";
        jobs_per_build_job=$(awk "BEGIN{print int($JOBS/$(echo $ARCHS | wc -w)+0.5)}")
        ((jobs_per_build_job=jobs_per_build_job+1))

        for arch in $ARCHS; do
          JOBS=$jobs_per_build_job build $arch &
          build_pids+=" $!"
        done

        for pid in $build_pids; do
          wait $pid || {
            echo ""
            echo "Build failed!"
            echo "Use DISABLE_PARALLEL_ARCH_BUILD=1 to disable parallel building of architectures"
            echo ""
            exit 1
          }
        done
      else
        for arch in $ARCHS; do
          build $arch
        done
      fi

      arch1=$(echo $ARCHS | awk '{print $1}')

      for file in $(ls build_$arch1/lib/darwin/); do
        libs=""

        for arch in $ARCHS; do
          lib="build_$arch/lib/darwin/$file"
          [ -n "$libs" ] && libs+=" "
          if [ -f "$lib" ]; then
            libs+="$lib"
          fi
        done

        xcrun lipo -create $libs -output build_$arch1/lib/darwin/$file.lipo
        rm build_$arch1/lib/darwin/$file
        mv build_$arch1/lib/darwin/$file.lipo build_$arch1/lib/darwin/$file
      done

      create_symlink build_$arch1 build
    else
      build
    fi

    ### CMAKE END ###

  else

    ### MAKE ###

    $SED -i "s/Configs += ios//g" make/platform/clang_darwin.mk
    $SED -i "s/Configs += cc_kext_ios5//g" make/platform/clang_darwin.mk
    $SED -i "s/Configs += profile_ios//g" make/platform/clang_darwin.mk
    $SED -i "s/Configs += asan_iossim_dynamic//g" make/platform/clang_darwin.mk

    # Unbreak the -Werror build.
    if [ -f lib/asan/asan_mac.h ]; then
      $SED -i "s/ASAN__MAC_H/ASAN_MAC_H/g" lib/asan/asan_mac.h
    fi

    EXTRA_MAKE_FLAGS+="LIPO=\"$(xcrun -f lipo)\""

    if [ $(osxcross-cmp $CLANG_VERSION "<=" 3.3) -eq 1 ]; then
      EXTRA_MAKE_FLAGS+=" AR=\"$(xcrun -f ar)\""
      EXTRA_MAKE_FLAGS+=" RANLIB=\"$(xcrun -f ranlib)\""
      EXTRA_MAKE_FLAGS+=" CC=\"$(xcrun -f clang)\""
    fi

    # Must eval here because of the spaces in EXTRA_MAKE_FLAGS.

    eval "$MAKE clang_darwin $EXTRA_MAKE_FLAGS -j $JOBS"

    ### MAKE END ###

  fi

  build_success
fi

# We must re-build every time. git clean -fdx
# removes the libraries.
rm -f $BUILD_DIR/.compiler-rt_build_complete


# Installation. Can be either automated (ENABLE_COMPILER_RT_INSTALL) or will
# print the commands that the user should run manually.

function print_or_run() {
  if [ -z "$ENABLE_COMPILER_RT_INSTALL" ]; then
    echo "$@"
  else
    $@
  fi
}

mkdir -p ${CLANG_INCLUDE_DIR} && \
  touch ${CLANG_INCLUDE_DIR} 2>/dev/null && ENABLE_COMPILER_RT_INSTALL=1

echo ""
echo ""
echo ""
if [ -z "$ENABLE_COMPILER_RT_INSTALL" ]; then
  echo "Please run the following commands by hand to install compiler-rt:"
else
  echo "Installing compiler-rt headers and libraries to the following paths:"
  echo "  ${CLANG_INCLUDE_DIR}"
  echo "  ${CLANG_DARWIN_LIB_DIR}"
fi
echo ""

print_or_run mkdir -p ${CLANG_INCLUDE_DIR}
print_or_run mkdir -p ${CLANG_DARWIN_LIB_DIR}
print_or_run cp -rv $BUILD_DIR/compiler-rt/compiler-rt/include/sanitizer ${CLANG_INCLUDE_DIR}

if [ $USE_CMAKE -eq 1 ]; then

  ### CMAKE ###

  print_or_run cp -v $BUILD_DIR/compiler-rt/compiler-rt/build/lib/darwin/*.a ${CLANG_DARWIN_LIB_DIR}
  print_or_run cp -v $BUILD_DIR/compiler-rt/compiler-rt/build/lib/darwin/*.dylib ${CLANG_DARWIN_LIB_DIR}

  ### CMAKE END ###

else

  ### MAKE ###

  pushd "clang_darwin" &>/dev/null

  function print_install_command() {
    if [ -f "$1" ]; then
      print_or_run cp $PWD/compiler-rt/$1 ${CLANG_DARWIN_LIB_DIR}/$2
    fi
  }

  print_install_command "osx/libcompiler_rt.a"         "libclang_rt.osx.a"
  print_install_command "10.4/libcompiler_rt.a"        "libclang_rt.10.4.a"
  print_install_command "eprintf/libcompiler_rt.a"     "libclang_rt.eprintf.a"
  print_install_command "cc_kext/libcompiler_rt.a"     "libclang_rt.cc_kext.a"
  print_install_command "profile_osx/libcompiler_rt.a" "libclang_rt.profile_osx.a"

  print_install_command "ubsan_osx_dynamic/libcompiler_rt.dylib" \
    "libclang_rt.ubsan_osx_dynamic.dylib"

  print_install_command "asan_osx_dynamic/libcompiler_rt.dylib" \
    "libclang_rt.asan_osx_dynamic.dylib"

  popd &>/dev/null

  ### MAKE END ###

fi


echo ""

