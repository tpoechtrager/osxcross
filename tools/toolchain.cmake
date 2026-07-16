# OSXCross toolchain

macro(osxcross_getconf VAR)
  if(NOT ${VAR})
    set(${VAR} "$ENV{${VAR}}")
    if(${VAR})
      set(${VAR} "${${VAR}}" CACHE STRING "${VAR}")
      message(STATUS "Found ${VAR}: ${${VAR}}")
    else()
      message(FATAL_ERROR "Cannot determine \"${VAR}\"")
    endif()
  endif()
endmacro()

osxcross_getconf(OSXCROSS_HOST)
osxcross_getconf(OSXCROSS_TARGET_DIR)
osxcross_getconf(OSXCROSS_TARGET)
osxcross_getconf(OSXCROSS_SDK)

set(CMAKE_SYSTEM_NAME "Darwin")
string(REGEX REPLACE "-.*" "" CMAKE_SYSTEM_PROCESSOR "${OSXCROSS_HOST}")

# specify the cross compiler. The osxcross-cmake dispatcher script picks
# the right osxcross wrapper names (clang, clang++, clang++-gstdc++, gcc,
# g++, ...) and exports them via OSXCROSS_C_COMPILER / OSXCROSS_CXX_COMPILER.
# We do not invent stdlib flags here -- the wrappers take care of that
# internally based on their own invocation name.
set(OSXCROSS_C_COMPILER   "$ENV{OSXCROSS_C_COMPILER}")
set(OSXCROSS_CXX_COMPILER "$ENV{OSXCROSS_CXX_COMPILER}")
if(NOT OSXCROSS_C_COMPILER OR NOT OSXCROSS_CXX_COMPILER)
  message(FATAL_ERROR
    "OSXCROSS_C_COMPILER and OSXCROSS_CXX_COMPILER must be set by the "
    "osxcross-cmake dispatcher (got C='${OSXCROSS_C_COMPILER}', "
    "CXX='${OSXCROSS_CXX_COMPILER}')")
endif()

set(CMAKE_C_COMPILER
  "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-${OSXCROSS_C_COMPILER}")
set(CMAKE_CXX_COMPILER
  "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-${OSXCROSS_CXX_COMPILER}")
set(CMAKE_OBJC_COMPILER
  "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-${OSXCROSS_C_COMPILER}")
set(CMAKE_OBJCXX_COMPILER
  "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-${OSXCROSS_CXX_COMPILER}")

# where is the target environment
set(CMAKE_FIND_ROOT_PATH
  "${CMAKE_FIND_ROOT_PATH}"
  "${OSXCROSS_SDK}"
  "${OSXCROSS_TARGET_DIR}/macports/pkgs/opt/local")

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_AR "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-ar" CACHE FILEPATH "ar")
set(CMAKE_RANLIB "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-ranlib" CACHE FILEPATH "ranlib")
set(CMAKE_INSTALL_NAME_TOOL "${OSXCROSS_TARGET_DIR}/bin/${OSXCROSS_HOST}-install_name_tool" CACHE FILEPATH "install_name_tool")

set(ENV{PKG_CONFIG_LIBDIR} "${OSXCROSS_TARGET_DIR}/macports/pkgs/opt/local/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${OSXCROSS_TARGET_DIR}/macports/pkgs")
