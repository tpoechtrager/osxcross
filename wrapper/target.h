/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2026 by Thomas Poechtrager                      *
 *  t.poechtrager@gmail.com                                            *
 *                                                                     *
 *  This program is free software; you can redistribute it and/or      *
 *  modify it under the terms of the GNU General Public License        *
 *  as published by the Free Software Foundation; either version 2     *
 *  of the License, or (at your option) any later version.             *
 *                                                                     *
 *  This program is distributed in the hope that it will be useful,    *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
 *  GNU General Public License for more details.                       *
 *                                                                     *
 *  You should have received a copy of the GNU General Public License  *
 *  along with this program; if not, write to the Free Software        *
 *  Foundation, Inc.,                                                  *
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.      *
 ***********************************************************************/

#ifndef OSXCROSS_WRAPPER_TARGET_H
#define OSXCROSS_WRAPPER_TARGET_H

#include <cstring>
#include <string>
#include <vector>

#include "tools.h"

namespace target {

using namespace tools;

//
// Default values for the Target struct
//

constexpr const char *getDefaultVendor() { return "apple"; }
constexpr const char *getDefaultTarget() { return OSXCROSS_TARGET; }

constexpr const char *getDefaultCompilerName() {
  return "clang";
}

constexpr Compiler getDefaultCompilerIdentifier() {
  return Compiler::CLANG;
}

constexpr const char *getDefaultCXXCompilerName() {
  return "clang++";
}

constexpr Compiler getDefaultCXXCompilerIdentifier() {
  return Compiler::CLANGXX;
}

constexpr const char *getSupportedArchsString(bool gcc = false) {
  return gcc ? OSXCROSS_GCC_TARGET_ARCHS
             : OSXCROSS_SUPPORTED_ARCHS;
}

// The architectures in SUPPORTED_ARCHS or, with GCC, GCC_TARGET_ARCHS; the
// default architecture is the first one.
std::vector<Arch> getSupportedArchs(bool gcc = false);
Arch getDefaultArch(bool gcc = false);

constexpr const char *getLinkerVersion() { return OSXCROSS_LINKER_VERSION; }

constexpr const char *getBuildDir() { return OSXCROSS_BUILD_DIR; }

constexpr const char *getLibLTOPath() {
#ifdef OSXCROSS_LIBLTO_PATH
  return OSXCROSS_LIBLTO_PATH;
#else
  return nullptr;
#endif
}

constexpr const char *getOSXCrossVersion() {
#ifdef OSXCROSS_VERSION
  return OSXCROSS_VERSION[0] ? OSXCROSS_VERSION : "unknown";
#else
  return "unknown";
#endif
}

#ifdef OSXCROSS_OSX_VERSION_MIN
inline OSVersion getDefaultMinTarget() {
  if (!strcmp(OSXCROSS_OSX_VERSION_MIN, "default"))
    return OSVersion();

  return parseOSVersion(OSXCROSS_OSX_VERSION_MIN);
}
#else
constexpr OSVersion getDefaultMinTarget() { return OSVersion(); }
#endif

// Build flavor

constexpr const char *getBuildFlavor() {
#ifdef OSXCROSS_BUILD_FLAVOR
  return OSXCROSS_BUILD_FLAVOR[0] ? OSXCROSS_BUILD_FLAVOR : "unknown";
#else
  return "unknown";
#endif
}

class BuildFlavor {
public:
  BuildFlavor(const char *value = getBuildFlavor()) : type(parse(value)) {}

  bool isStable() const { return type == STABLE; }
  bool isLatest() const { return type == LATEST; }
  bool isLLVM() const { return type == LLVM; }
  bool isValid() const { return type != UNKNOWN_FLAVOR; }

private:
  enum {
    STABLE,
    LATEST,
    LLVM,
    UNKNOWN_FLAVOR
  };

  static int parse(const char *value) {
    if (!value || !*value)
      return UNKNOWN_FLAVOR;
    if (!strcmp(value, "stable"))
      return STABLE;
    if (!strcmp(value, "latest"))
      return LATEST;
    if (!strcmp(value, "llvm"))
      return LLVM;
    return UNKNOWN_FLAVOR;
  }

  int type;
};

//
// Target
//

// Build a target triple such as x86_64-apple-darwin27.3. With
// useAarch64InsteadOfArm64, arm64 is spelled aarch64 (GCC's name).
std::string makeTriple(Arch arch, const char *vendor, const std::string &target,
                       bool useAarch64InsteadOfArm64 = false);

// The macOS version in the name of an SDK directory: 15.4 for
// "/path/to/MacOSX15.4.sdk"; 0.0 if the name does not start with "MacOSX".
OSVersion getSDKVersionFromPath(std::string sdkPath);

struct Target {
  Target();

  // Determine the SDK in use, sdkPath and sdkVersion. The first one given
  // wins:
  //
  //   -isysroot, --sysroot  commandLineIsysroot, commandLineSysroot
  //   SDKROOT               if it is an absolute path to an existing
  //                         directory other than "/", as with Apple's clang
  //   OSXCROSS_SDKROOT      if it is not empty
  //   the installed SDK     <execDir>/../SDK/MacOSX<version>.sdk, named after
  //                         the macOS version that belongs to the target
  //
  // The version is taken from the name of the directory (15.4 for
  // MacOSX15.4.sdk; 0.0 if the name has none). Prints an error, unless
  // quiet, and returns false if the directory does not exist; sdkVersion is
  // set anyway.
  bool determineSDK(bool quiet = false);

  bool getMacPortsDir(std::string &path) const;
  bool getMacPortsSysRootDir(std::string &path) const;
  bool getMacPortsPkgConfigDir(std::string &path) const;
  bool getMacPortsIncludeDir(std::string &path) const;
  bool getMacPortsLibDir(std::string &path) const;
  bool getMacPortsFrameworksDir(std::string &path) const;

  bool archSupported(const Arch arch);
  bool checkArchs();
  void addArch(const Arch arch);
  bool haveArch(const Arch arch);

  bool hasLibCXX() const;
  bool libCXXIsDefaultCXXLib() const;

  bool isCXX();
  bool isGCH();

  bool isClang() const;
  bool isGCC() const;

  bool isKnownCompiler() const;

  const std::string &buildDefaultTriple(
    std::string &triple,
    bool gcc = false, bool useAarch64InsteadOfArm64 = false) const;

  const std::string &getTriple() const { return triple; }

  // <execDir>/<triple>-<tool>, by default for the current triple.
  std::string buildToolPath(const std::string &tool) const;
  std::string buildToolPath(const std::string &toolTriple,
                            const std::string &tool) const;

  void setCompilerPath();
  bool findClangIntrinsicHeadersAndVersion(std::string &path);

  void addGCCRuntimeLibs(Arch arch);
  void setTriple(bool useAarch64InsteadOfArm64 = false);
  bool setup();

  //
  // Fixed per wrapper build and installation
  //
  BuildFlavor buildFlavor;          // stable | latest | llvm
  const char *vendor;               // "apple"
  std::vector<Arch> supportedArchs; // SUPPORTED_ARCHS of this build
  std::string execDir;              // directory of the running executable

  //
  // Target (from the tool name and the command line)
  //
  std::string targetOS;       // darwin20.1, the OS part of the triple
  Arch primaryArch;           // from the tool name (x86_64-..., o64-...);
                              // GCC: also -arch, -m32, -m64
  std::vector<Arch> archs;    // -arch, -m32, -m64 (Clang); if empty,
                              // setup() adds primaryArch
  std::string triple;         // <primaryArch>-<vendor>-<targetOS>
  OSVersion deploymentTarget; // -mmacosx-version-min= or
                              // MACOSX_DEPLOYMENT_TARGET; 0: not set,
                              // setup() sets the default

  //
  // SDK (see determineSDK())
  //
  std::string commandLineIsysroot; // -isysroot; empty: not given
  std::string commandLineSysroot;  // --sysroot; empty: not given
  std::string sdkPath;             // the SDK in use
  OSVersion sdkVersion;            // its macOS version; 0.0: unknown

  //
  // Compiler
  //
  Compiler compiler;
  std::string compilerName;  // clang | gcc
  std::string compilerPath;  // /usr/bin/clang | [...]/target/bin/*-gcc
  std::string compilerArgv0; // clang | *-apple-darwin-gcc
  ClangVersion clangVersion; // see findClangIntrinsicHeadersAndVersion()

  //
  // C++ standard library
  //
  Stdlib stdlib;         // -stdlib= or the tool name; setup() resolves unset
  bool useGCCLibs;       // -foc-use-gcc-libstdc++, clang++-gstdc++
  GCCVersion gccVersion; // of the GCC installed by ./build_gcc.sh
  std::string gccTriple; // of the GCC installed by ./build_gcc.sh

  //
  // Command line
  //
  const char *language;            // -x <language>; nullptr: not given
  std::string intrinsicPathOption; // -foc-intrinsic-path=
  StringVector wrapperArgs;        // built by setup(); [0] is compilerArgv0
  StringVector userArgs;           // the user's arguments, after wrapperArgs
};

} // namespace target

#endif // OSXCROSS_WRAPPER_TARGET_H
