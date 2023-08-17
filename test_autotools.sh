#!/usr/bin/env bash

trap "exit 0" INT EXIT TERM HUP PIPE QUIT ILL KILL ABRT

################################################################################
# OS X Cross Build Test Some Non-Trivial Autotools Configured Projects:
################################################################################
# This script downloads mainline sources for several non-trivial and recent
#  Autotool configured projects and build them from source using the OS X Cross
#  toolchain.
################################################################################
# NOTE: The OS X Cross stage directory target/bin is assumed to be at the
#  beginning of the PATH.
#
# The following command must be able to locate the toolchain compilers:
#
#     xcrun -f cc
################################################################################
# The following environment variables effect the operation of this script:
#
#  # Set the test architecture to use:
#  OSXCROSS_TEST_ARCH=aarch64|arm64
#  OSXCROSS_TEST_ARCH=x86_64|x86_64h|i386
#  OSXCROSS_TEST_ARCH=powerpc|powerpc64
#
#  # Trigger a rebuild of everything:
#  OSXCROSS_TEST_REBUILD=0|1
#
#  # Project Versions:
#  OSXCROSS_TEST_OPENSSL_VERSION=3.1.2
#  OSXCROSS_TEST_WGET_VERSION=1.21.4
#  OSXCROSS_TEST_CURL_VERSION=8.2.1
#  OSXCROSS_TEST_ZSTD_VERSION=1.5.5
#  OSXCROSS_TEST_LIBSODIUM_VERSION=1.0.18
################################################################################

################################################################################
# Determine the host prefix and compiler tools:
################################################################################

OSXCROSS_TEST_ARCH="${OSXCROSS_TEST_ARCH:-x86_64}"
OSXCROSS_TEST_OSVERSION="unknown"
OSXCROSS_TEST_XCRUNCC="$(xcrun -f cc 2>/dev/null)"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX1=".*darwin([0-9])[-]cc$"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX2=".*darwin([0-9]{2})[-]cc$"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX3=".*darwin([0-9]{2}[.][0-9])[-]cc$"
OSXCROSS_TEST_XCRUNCC_VERSION_REGEX4=".*darwin([0-9]{2}[.][0-9]{2})[-]cc$"
if [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX1 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
elif [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX2 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
elif [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX3 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
elif [[ $OSXCROSS_TEST_XCRUNCC =~ $OSXCROSS_TEST_XCRUNCC_VERSION_REGEX4 ]]
then
   OSXCROSS_TEST_OSVERSION="${BASH_REMATCH[1]}"
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot Determine Toolchain OSVersion." 1>&2
   exit 1
fi

OSXCROSS_TEST_HOST_PREFIX=\
"${OSXCROSS_TEST_ARCH}-apple-darwin${OSXCROSS_TEST_OSVERSION}"
OSXCROSS_TEST_TOOLCHAIN_CC="${OSXCROSS_TEST_HOST_PREFIX}-gcc"
OSXCROSS_TEST_TOOLCHAIN_CXX="${OSXCROSS_TEST_HOST_PREFIX}-g++"
OSXCROSS_TEST_TOOLCHAIN_FORTRAN="${OSXCROSS_TEST_HOST_PREFIX}-gfortran"
OSXCROSS_TEST_TOOLCHAIN_AR="${OSXCROSS_TEST_HOST_PREFIX}-ar"
OSXCROSS_TEST_TOOLCHAIN_RANLIB="${OSXCROSS_TEST_HOST_PREFIX}-ranlib"
echo
echo "======================================================================"
echo " OS X Cross Autotool Configure Projects Test:"
echo "======================================================================"
echo " OSXCROSS_TEST_ARCH=${OSXCROSS_TEST_ARCH}"
echo " OSXCROSS_TEST_HOST_PREFIX=${OSXCROSS_TEST_HOST_PREFIX}"
echo " OSXCROSS_TEST_TOOLCHAIN_CC=${OSXCROSS_TEST_TOOLCHAIN_CC}"
echo "======================================================================"
OSXCROSS_TEST_TOOLCHAIN_CC_RUN="$(${OSXCROSS_TEST_TOOLCHAIN_CC} --version 2>&1)"
if [ "${?}x" = "0x" ]
then
   echo "${OSXCROSS_TEST_TOOLCHAIN_CC_RUN}"
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Failure running '${OSXCROSS_TEST_TOOLCHAIN_CC} --version'." 1>&2
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "${OSXCROSS_TEST_TOOLCHAIN_CC_RUN}" 1>&2
   exit 1
fi

################################################################################
# Create the test and stage directories:
################################################################################

OSXCROSS_TEST_DIR="$(pwd)/test/${OSXCROSS_TEST_ARCH}"
OSXCROSS_TEST_STAGE_DIR="${OSXCROSS_TEST_DIR}/stage"
mkdir -p "${OSXCROSS_TEST_DIR}"
mkdir -p "${OSXCROSS_TEST_STAGE_DIR}"
if [ ! -d "${OSXCROSS_TEST_STAGE_DIR}" ]
then
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot create Test Directory '${OSXCROSS_TEST_STAGE_DIR}'." 1>&2
   exit 1
fi

echo
echo "======================================================================"
echo " OSXCROSS_TEST_DIR=${OSXCROSS_TEST_DIR}"
echo " OSXCROSS_TEST_STAGE_DIR=${OSXCROSS_TEST_STAGE_DIR}"
echo "======================================================================"
echo

################################################################################
# Download the project sources:
################################################################################

OSXCROSS_TEST_OPENSSL_VERSION="${OSXCROSS_TEST_OPENSSL_VERSION:-3.1.2}"
OSXCROSS_TEST_OPENSSL_PREFIX="openssl-${OSXCROSS_TEST_OPENSSL_VERSION}"
OSXCROSS_TEST_OPENSSL_TAG="${OSXCROSS_TEST_OPENSSL_PREFIX}".TAG
if [ ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_PREFIX}".tar.gz ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_TAG}"
   OSXCROSS_TEST_REBUILD=1
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && wget https://www.openssl.org/source/"\
${OSXCROSS_TEST_OPENSSL_PREFIX}".tar.gz \
   )
fi
if [ -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_PREFIX}".tar.gz ]
then
   echo "Found: '${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_PREFIX}.tar.gz'."
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot retrieve OPENSSL Source." 1>&2
   exit 1
fi
if [ ! -d "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_PREFIX}" ]
then
   echo "Uncompressing: '${OSXCROSS_TEST_OPENSSL_PREFIX}.tar.gz'."
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_TAG}"
   OSXCROSS_TEST_REBUILD=1
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && tar xf "${OSXCROSS_TEST_OPENSSL_PREFIX}".tar.gz \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Uncompressing '${OSXCROSS_TEST_OPENSSL_PREFIX}.tar.gz'." 1>&2
      exit 1
   }
fi

OSXCROSS_TEST_WGET_VERSION="${OSXCROSS_TEST_WGET_VERSION:-1.21.4}"
OSXCROSS_TEST_WGET_PREFIX="wget-${OSXCROSS_TEST_WGET_VERSION}"
OSXCROSS_TEST_WGET_TAG="${OSXCROSS_TEST_WGET_PREFIX}".TAG
if [ ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_PREFIX}".tar.gz ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && wget http://ftp.gnu.org/gnu/wget/\
"${OSXCROSS_TEST_WGET_PREFIX}".tar.gz \
   )
fi
if [ -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_PREFIX}".tar.gz ]
then
   echo "Found: '${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_PREFIX}.tar.gz'."
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot retrieve WGET Source." 1>&2
   exit 1
fi
if [ ! -d "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_PREFIX}" ]
then
   echo "Uncompressing: '${OSXCROSS_TEST_WGET_PREFIX}.tar.gz'."
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && tar xf "${OSXCROSS_TEST_WGET_PREFIX}".tar.gz \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Uncompressing '${OSXCROSS_TEST_WGET_PREFIX}.tar.gz'." 1>&2
      exit 1
   }
fi

OSXCROSS_TEST_CURL_VERSION="${OSXCROSS_TEST_CURL_VERSION:-8.2.1}"
OSXCROSS_TEST_CURL_PREFIX="curl-${OSXCROSS_TEST_CURL_VERSION}"
OSXCROSS_TEST_CURL_TAG="${OSXCROSS_TEST_CURL_PREFIX}".TAG
if [ ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_PREFIX}".tar.gz ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && wget http://curl.haxx.se/download/\
"${OSXCROSS_TEST_CURL_PREFIX}".tar.gz \
   )
fi
if [ -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_PREFIX}".tar.gz ]
then
   echo "Found: '${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_PREFIX}.tar.gz'."
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot retrieve CURL Source." 1>&2
   exit 1
fi
if [ ! -d "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_PREFIX}" ]
then
   echo "Uncompressing: '${OSXCROSS_TEST_CURL_PREFIX}.tar.gz'."
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && tar xf "${OSXCROSS_TEST_CURL_PREFIX}".tar.gz \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Uncompressing '${OSXCROSS_TEST_CURL_PREFIX}.tar.gz'." 1>&2
      exit 1
   }
fi

OSXCROSS_TEST_ZSTD_VERSION="${OSXCROSS_TEST_ZSTD_VERSION:-1.5.2}"
OSXCROSS_TEST_ZSTD_PREFIX="zstd-${OSXCROSS_TEST_ZSTD_VERSION}"
OSXCROSS_TEST_ZSTD_TAG="${OSXCROSS_TEST_ZSTD_PREFIX}".TAG
if [ ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_PREFIX}".tar.gz ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && wget https://github.com/facebook/zstd/releases/download/v1.5.2/\
"${OSXCROSS_TEST_ZSTD_PREFIX}".tar.gz \
   )
fi
if [ -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_PREFIX}".tar.gz ]
then
   echo "Found: '${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_PREFIX}.tar.gz'."
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot retrieve ZSTD Source." 1>&2
   exit 1
fi
if [ ! -d "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_PREFIX}" ]
then
   echo "Uncompressing: '${OSXCROSS_TEST_ZSTD_PREFIX}.tar.gz'."
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && tar xf "${OSXCROSS_TEST_ZSTD_PREFIX}".tar.gz \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Uncompressing '${OSXCROSS_TEST_ZSTD_PREFIX}.tar.gz'." 1>&2
      exit 1
   }
fi

OSXCROSS_TEST_LIBSODIUM_VERSION="${OSXCROSS_TEST_LIBSODIUM_VERSION:-1.0.18}"
OSXCROSS_TEST_LIBSODIUM_PREFIX="libsodium-${OSXCROSS_TEST_LIBSODIUM_VERSION}"
OSXCROSS_TEST_LIBSODIUM_TAG="${OSXCROSS_TEST_LIBSODIUM_PREFIX}".TAG
if [ ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_PREFIX}".tar.gz ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && wget https://download.libsodium.org/libsodium/releases/\
"${OSXCROSS_TEST_LIBSODIUM_PREFIX}".tar.gz \
   )
fi
if [ -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_PREFIX}".tar.gz ]
then
   echo "Found: '${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_PREFIX}.tar.gz'."
else
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "Cannot retrieve LIBSODIUM Source." 1>&2
   exit 1
fi
if [ ! -d "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_PREFIX}" ]
then
   echo "Uncompressing: '${OSXCROSS_TEST_LIBSODIUM_PREFIX}.tar.gz'."
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}" \
      && tar xf "${OSXCROSS_TEST_LIBSODIUM_PREFIX}".tar.gz \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Uncompressing '${OSXCROSS_TEST_LIBSODIUM_PREFIX}.tar.gz'." 1>&2
      exit 1
   }
fi

################################################################################
# Build the projects
################################################################################

OSXCROSS_TEST_REBUILD="${OSXCROSS_TEST_REBUILD:-0}"

OSXCROSS_TEST_OPENSSL_TARGET="unset"
case "${OSXCROSS_TEST_ARCH}" in
   aarch64|arm64|arm64e)
      # NOTE: Maybe wont work for arm64e?
      #  Perhaps 'BSD-generic64 no-asm' instead?
      OSXCROSS_TEST_OPENSSL_TARGET="darwin64-arm64-cc"
      ;;
   x86_64|x86_64h)
      # NOTE: Maybe wont work for x86_64h?
      #  Perhaps 'BSD-generic64 no-asm' instead?
      OSXCROSS_TEST_OPENSSL_TARGET="darwin64-x86_64-cc"
      ;;
   i[3456]86)
      #OSXCROSS_TEST_OPENSSL_TARGET="BSD-generic32 no-asm"
      OSXCROSS_TEST_OPENSSL_TARGET="darwin-i386-cc 386 no-asm"
      ;;
   powerpc64)
      OSXCROSS_TEST_OPENSSL_TARGET="darwin64-ppc-cc"
      ;;
   powerpc)
      OSXCROSS_TEST_OPENSSL_TARGET="darwin-ppc-cc"
      ;;
   *)
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Cannot determine OPENSSL Target." 1>&2
      exit 1
      ;;
esac

if [ "${OSXCROSS_TEST_REBUILD}x" = "1x" \
   -o ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_TAG}" \
   -o ! -e "${OSXCROSS_TEST_STAGE_DIR}/bin/openssl" \
   -o ! -e "${OSXCROSS_TEST_STAGE_DIR}/lib/libssl.dylib" ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_TAG}"
   OSXCROSS_TEST_REBUILD=1
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_PREFIX}" \
      && make clean 2>/dev/null \
   )
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_PREFIX}" \
      && CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         AR="${OSXCROSS_TEST_TOOLCHAIN_AR}" \
         RANLIB="${OSXCROSS_TEST_TOOLCHAIN_RANLIB}" \
         ./Configure \
         ${OSXCROSS_TEST_OPENSSL_TARGET} \
         --prefix="${OSXCROSS_TEST_STAGE_DIR}" \
         -latomic \
      && make clean \
         CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         AR="${OSXCROSS_TEST_TOOLCHAIN_AR}" \
         RANLIB="${OSXCROSS_TEST_TOOLCHAIN_RANLIB}" \
      && make install \
         CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         AR="${OSXCROSS_TEST_TOOLCHAIN_AR}" \
         RANLIB="${OSXCROSS_TEST_TOOLCHAIN_RANLIB}" \
      && touch "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_TAG}" \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Building OPENSSL." 1>&2
      exit 1
   }
fi
if [ -e "${OSXCROSS_TEST_STAGE_DIR}/bin/openssl" \
   -a -e "${OSXCROSS_TEST_STAGE_DIR}/lib/libssl.dylib" ]
then
   echo
   echo "======================================================================"
   echo " OPENSSL Build:"
   echo "======================================================================"
   file "${OSXCROSS_TEST_STAGE_DIR}/bin/openssl"
   xcrun otool -arch all -hvL "${OSXCROSS_TEST_STAGE_DIR}/bin/openssl"
   xcrun vtool -show-build \
      "${OSXCROSS_TEST_STAGE_DIR}/bin/openssl" 2>/dev/null
   file "${OSXCROSS_TEST_STAGE_DIR}/lib/libssl.dylib"
   xcrun otool -arch all -hvL "${OSXCROSS_TEST_STAGE_DIR}/lib/libssl.dylib"
   xcrun vtool -show-build \
      "${OSXCROSS_TEST_STAGE_DIR}/lib/libssl.dylib" 2>/dev/null
else
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_OPENSSL_TAG}"
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "OPENSSL Objects not Found." 1>&2
   exit 1
fi

if [ "${OSXCROSS_TEST_REBUILD}x" = "1x" \
   -o ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_TAG}" \
   -o ! -e "${OSXCROSS_TEST_STAGE_DIR}/bin/wget" ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_PREFIX}" \
      && make clean 2>/dev/null \
   )
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_PREFIX}" \
      && CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         ./configure \
         ${OSXCROSS_TEST_WGET_TARGET} \
         --host="${OSXCROSS_TEST_HOST_PREFIX}" \
         --prefix="${OSXCROSS_TEST_STAGE_DIR}" \
         --with-ssl=openssl \
         --with-libssl-prefix="${OSXCROSS_TEST_STAGE_DIR}" \
      && make clean \
      && make install \
      && touch "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_TAG}" \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Building WGET." 1>&2
      exit 1
   }
fi
if [ -e "${OSXCROSS_TEST_STAGE_DIR}/bin/wget" ]
then
   echo
   echo "======================================================================"
   echo " WGET Build:"
   echo "======================================================================"
   file "${OSXCROSS_TEST_STAGE_DIR}/bin/wget"
   xcrun otool -arch all -hvL "${OSXCROSS_TEST_STAGE_DIR}/bin/wget"
   xcrun vtool -show-build \
      "${OSXCROSS_TEST_STAGE_DIR}/bin/wget" 2>/dev/null
else
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_WGET_TAG}"
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "WGET Objects not Found." 1>&2
   exit 1
fi

if [ "${OSXCROSS_TEST_REBUILD}x" = "1x" \
   -o ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_TAG}" \
   -o ! -e "${OSXCROSS_TEST_STAGE_DIR}/bin/curl" ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_PREFIX}" \
      && make clean 2>/dev/null \
   )
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_PREFIX}" \
      && CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         LIBS="-framework Foundation -framework SystemConfiguration" \
         ./configure \
         ${OSXCROSS_TEST_CURL_TARGET} \
         --host="${OSXCROSS_TEST_HOST_PREFIX}" \
         --prefix="${OSXCROSS_TEST_STAGE_DIR}" \
         --with-ssl="${OSXCROSS_TEST_STAGE_DIR}" \
      && make clean \
      && make install \
      && touch "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_TAG}" \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Building CURL." 1>&2
      exit 1
   }
fi
if [ -e "${OSXCROSS_TEST_STAGE_DIR}/bin/curl" ]
then
   echo
   echo "======================================================================"
   echo " CURL Build:"
   echo "======================================================================"
   file "${OSXCROSS_TEST_STAGE_DIR}/bin/curl"
   xcrun otool -arch all -hvL "${OSXCROSS_TEST_STAGE_DIR}/bin/curl"
   xcrun vtool -show-build \
      "${OSXCROSS_TEST_STAGE_DIR}/bin/curl" 2>/dev/null
else
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_CURL_TAG}"
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "CURL Objects not Found." 1>&2
   exit 1
fi

if [ "${OSXCROSS_TEST_REBUILD}x" = "1x" \
   -o ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_TAG}" \
   -o ! -e "${OSXCROSS_TEST_STAGE_DIR}/bin/zstd" ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_PREFIX}" \
      && make clean 2>/dev/null \
   )
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_PREFIX}" \
      && make clean \
         UNAME=Darwin \
         PREFIX="${OSXCROSS_TEST_STAGE_DIR}" \
         CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         AR="${OSXCROSS_TEST_TOOLCHAIN_AR}" \
         RANLIB="${OSXCROSS_TEST_TOOLCHAIN_RANLIB}" \
      && make allmost examples \
         UNAME=Darwin \
         PREFIX="${OSXCROSS_TEST_STAGE_DIR}" \
         CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         AR="${OSXCROSS_TEST_TOOLCHAIN_AR}" \
         RANLIB="${OSXCROSS_TEST_TOOLCHAIN_RANLIB}" \
      && make install \
         UNAME=Darwin \
         PREFIX="${OSXCROSS_TEST_STAGE_DIR}" \
         CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         AR="${OSXCROSS_TEST_TOOLCHAIN_AR}" \
         RANLIB="${OSXCROSS_TEST_TOOLCHAIN_RANLIB}" \
      && touch "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_TAG}" \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Building ZSTD." 1>&2
      exit 1
   }
fi
if [ -e "${OSXCROSS_TEST_STAGE_DIR}/bin/zstd" ]
then
   echo
   echo "======================================================================"
   echo " ZSTD Build:"
   echo "======================================================================"
   file "${OSXCROSS_TEST_STAGE_DIR}/bin/zstd"
   xcrun otool -arch all -hvL "${OSXCROSS_TEST_STAGE_DIR}/bin/zstd"
   xcrun vtool -show-build \
      "${OSXCROSS_TEST_STAGE_DIR}/bin/zstd" 2>/dev/null
else
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_ZSTD_TAG}"
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "ZSTD Objects not Found." 1>&2
   exit 1
fi

if [ "${OSXCROSS_TEST_REBUILD}x" = "1x" \
   -o ! -e "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_TAG}" \
   -o ! -e "${OSXCROSS_TEST_STAGE_DIR}/lib/libsodium.dylib" ]
then
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_TAG}"
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_PREFIX}" \
      && make clean 2>/dev/null \
   )
   ( \
      cd "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_PREFIX}" \
      && CC="${OSXCROSS_TEST_TOOLCHAIN_CC}" \
         CXX="${OSXCROSS_TEST_TOOLCHAIN_CXX}" \
         ./configure \
         ${OSXCROSS_TEST_LIBSODIUM_TARGET} \
         --host="${OSXCROSS_TEST_HOST_PREFIX}" \
         --prefix="${OSXCROSS_TEST_STAGE_DIR}" \
      && make clean \
      && make install \
      && touch "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_TAG}" \
   ) || {
      echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
         "Failure Building LIBSODIUM." 1>&2
      exit 1
   }
fi
if [ -e "${OSXCROSS_TEST_STAGE_DIR}/bin/wget" ]
then
   echo
   echo "======================================================================"
   echo " LIBSODIUM Build:"
   echo "======================================================================"
   file "${OSXCROSS_TEST_STAGE_DIR}/lib/libsodium.dylib"
   xcrun otool -arch all -hvL "${OSXCROSS_TEST_STAGE_DIR}/lib/libsodium.dylib"
   xcrun vtool -show-build \
      "${OSXCROSS_TEST_STAGE_DIR}/lib/libsodium.dylib" 2>/dev/null
else
   rm -f "${OSXCROSS_TEST_DIR}/${OSXCROSS_TEST_LIBSODIUM_TAG}"
   echo "${BASH_SOURCE}:${LINENO}:ERROR:" \
      "LIBSODIUM Objects not Found." 1>&2
   exit 1
fi
