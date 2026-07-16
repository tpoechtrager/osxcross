#!/usr/bin/env bash

# Build the osxcross wrapper and install the names handled by this script.
#
# Execution order:
#   1. Load the shared build environment.
#   2. Configure and compile the wrapper binary.
#   3. Stop after compilation when BWCOMPILEONLY is set.
#   4. Install the binary and create the required symlinks.
#
# Link order is significant because verbose_cmd exposes every operation and a
# failed command may stop a build immediately. clang++-gstdc++ uses
# SUPPORTED_ARCHS and supports the ARM64 aliases aarch64, arm64 and oa64 in
# addition to the classic x86 aliases.

pushd "${0%/*}" &>/dev/null
pushd .. &>/dev/null
source ./tools/tools.sh
popd &>/dev/null

if [ -z "$SUPPORTED_ARCHS" ]; then
  export SUPPORTED_ARCHS=$OSXCROSS_SUPPORTED_ARCHS
fi

if [ -z "$SUPPORTED_ARCHS" ]; then
  echo "SUPPORTED_ARCHS not set. Rebuild from scratch." 1>&2
  exit 1
fi

[ -z "$TARGETCOMPILER" ] && TARGETCOMPILER=clang
TARGETTRIPLE=$(first_supported_arch)-apple-${TARGET}
FLAGS=""

# A cross-platform wrapper build compiles only by default and selects the
# compiler expected by the requested host platform.
if [ -n "$BWPLATFORM" ]; then
  PLATFORM=$BWPLATFORM

  if [ $PLATFORM = "Darwin" -a $(uname -s) != "Darwin" ]; then
    CXX=$(xcrun -f clang++)
    FLAGS+="-fvisibility-inlines-hidden "
  elif [ $PLATFORM = "FreeBSD" -a $(uname -s) != "FreeBSD" ]; then
    CXX=amd64-pc-freebsd13.0-clang++
  elif [ $PLATFORM = "NetBSD" -a $(uname -s) != "NetBSD" ]; then
    CXX=amd64-pc-netbsd6.1.3-clang++
  fi

  [ -z "$BWCOMPILEONLY" ] && BWCOMPILEONLY=1
else
  [ -z "$PORTABLE" ] && FLAGS="$CXXFLAGS "
fi

if [ -n "$BWCXX" ]; then
  [ "$CXX" != "$BWCXX" ] && echo "using $BWCXX" 1>&2
  CXX=$BWCXX
fi

if [ "$PLATFORM" == "Linux" ]; then
  FLAGS+="-isystem quirks/include "
fi

# Create the installation directory, clean the previous wrapper build and
# compile with the configured flags.
mkdir -p ${TARGET_DIR}/bin
export PLATFORM
export CXX

verbose_cmd $MAKE clean
ADDITIONAL_CXXFLAGS="$FLAGS" \
  verbose_cmd $MAKE wrapper -j$JOBS

if [ -n "$BWCOMPILEONLY" ]; then
  exit 0
fi

verbose_cmd mv wrapper "${TARGET_DIR}/bin/${TARGETTRIPLE}-wrapper"

# ---------------------------------------------------------------------------
# Symlink generation
# ---------------------------------------------------------------------------

# Install all wrapper links for one program.
#
# Usage:
#   install_program_links <program> <supported-archs> \
#     [enable_standalone] [enable_shortcuts]
#
# One target-prefixed link is created for every whitespace-separated
# architecture in supported-archs. ARM64 accepts both arm64 and aarch64 and
# creates both spellings. enable_standalone adds an unprefixed link first;
# enable_shortcuts adds o32/o64/o64h/oa64/oa64e links and rejects
# architectures for which no shortcut name is defined.
#
# Example: create Clang target links and architecture shortcuts:
#   install_program_links clang "$SUPPORTED_ARCHS" enable_shortcuts
#
# Example: create osxcross first, followed by its target-prefixed links:
#   install_program_links osxcross "$SUPPORTED_ARCHS" enable_standalone
#
# Example: GCC uses its own architecture list:
#   install_program_links gcc "$GCC_TARGET_ARCHS" enable_shortcuts
function install_program_links
{
  local program=$1
  local supported_archs=$2
  local standalone_enabled=""
  local shortcuts_enabled=""
  local option arch shortcut
  shift 2

  # Parse the optional link modes.
  for option in "$@"; do
    case "$option" in
      enable_standalone) standalone_enabled=enabled ;;
      enable_shortcuts) shortcuts_enabled=enabled ;;
      *)
        echo "Unknown install_program_links option: $option" 1>&2
        return 2
        ;;
    esac
  done

  # Create the unprefixed link before all architecture-specific links.
  if [ "$standalone_enabled" = enabled ]; then
    verbose_cmd create_symlink "${TARGETTRIPLE}-wrapper" "$program"
  fi

  # Create target-prefixed links directly from the supported architectures.
  for arch in $supported_archs; do
    case "$arch" in
      arm64 | aarch64)
        # Clang uses arm64 while GCC uses aarch64 for the same architecture.
        # Install both target spellings so either compiler convention works.
        verbose_cmd create_symlink \
          "${TARGETTRIPLE}-wrapper" "aarch64-apple-${TARGET}-$program"
        verbose_cmd create_symlink \
          "${TARGETTRIPLE}-wrapper" "arm64-apple-${TARGET}-$program"
        ;;
      *)
        verbose_cmd create_symlink \
          "${TARGETTRIPLE}-wrapper" "$arch-apple-${TARGET}-$program"
        ;;
    esac
  done

  # Create shortcuts only when explicitly requested.
  if [ "$shortcuts_enabled" != enabled ]; then
    return 0
  fi

  # Create the short architecture aliases used by compiler wrappers.
  for arch in $supported_archs; do
    case "$arch" in
      i386) shortcut=o32 ;;
      x86_64) shortcut=o64 ;;
      x86_64h) shortcut=o64h ;;
      arm64 | aarch64) shortcut=oa64 ;;
      arm64e) shortcut=oa64e ;;
      *)
        echo "Unsupported architecture for shortcut link: '$arch'" 1>&2
        return 2
        ;;
    esac

    verbose_cmd create_symlink \
      "${TARGETTRIPLE}-wrapper" "$shortcut-$program"
  done
}

# ---------------------------------------------------------------------------
# Symlink installation
# ---------------------------------------------------------------------------

pushd "${TARGET_DIR}/bin" &>/dev/null

if [ $TARGETCOMPILER = "clang" ]; then
  install_program_links clang "$SUPPORTED_ARCHS" enable_shortcuts
  install_program_links clang++ "$SUPPORTED_ARCHS" enable_shortcuts
  install_program_links clang++-libc++ "$SUPPORTED_ARCHS" enable_shortcuts
  install_program_links clang++-stdc++ "$SUPPORTED_ARCHS" enable_shortcuts
  install_program_links clang++-gstdc++ "$SUPPORTED_ARCHS" enable_shortcuts
elif [ $TARGETCOMPILER = "gcc" ]; then
  install_program_links gcc "$GCC_TARGET_ARCHS" enable_shortcuts
  install_program_links g++ "$GCC_TARGET_ARCHS" enable_shortcuts
  install_program_links g++-libc++ "$GCC_TARGET_ARCHS" enable_shortcuts
fi

install_program_links cc "$SUPPORTED_ARCHS"
install_program_links c++ "$SUPPORTED_ARCHS"

install_program_links osxcross "$SUPPORTED_ARCHS" enable_standalone
install_program_links osxcross-conf "$SUPPORTED_ARCHS" enable_standalone
install_program_links osxcross-env "$SUPPORTED_ARCHS" enable_standalone
install_program_links osxcross-cmp "$SUPPORTED_ARCHS" enable_standalone
install_program_links osxcross-man "$SUPPORTED_ARCHS" enable_standalone
install_program_links pkg-config "$SUPPORTED_ARCHS"

# Darwin provides these tools itself. Other hosts need wrapper links.
if [ "$PLATFORM" != "Darwin" ]; then
  if which dsymutil &>/dev/null; then
    # If dsymutil is in PATH then it's most likely a recent
    # LLVM dsymutil version. In this case don't wrap it.
    # Just create target symlinks.

    for ARCH in $SUPPORTED_ARCHS; do
      verbose_cmd create_symlink "$(which dsymutil)" "$ARCH-apple-$TARGET-dsymutil"
    done
  else
    install_program_links dsymutil enable_standalone
  fi

  install_program_links sw_vers "$SUPPORTED_ARCHS" enable_standalone

  install_program_links xcrun "$SUPPORTED_ARCHS" enable_standalone
  install_program_links xcodebuild "$SUPPORTED_ARCHS" enable_standalone
fi

popd &>/dev/null
popd &>/dev/null
