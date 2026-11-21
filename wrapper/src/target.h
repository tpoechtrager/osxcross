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

#include "tools/tools.h"

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

// The oldest macOS version that runs the architecture: 11.0 for arm64 and
// arm64e, 10.8 for x86_64h; 0.0 (no limit) for the others.
constexpr OSVersion getMinDeploymentTargetForArch(Arch arch) {
  return arch == Arch::ARM64 || arch == Arch::ARM64E ? OSVersion(11, 0)
         : arch == Arch::X86_64H                     ? OSVersion(10, 8)
                                                     : OSVersion();
}

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

#ifdef OSXCROSS_DEFAULT_DEPLOYMENT_TARGET
inline OSVersion getDefaultDeploymentTarget() {
  if (!strcmp(OSXCROSS_DEFAULT_DEPLOYMENT_TARGET, "default"))
    return OSVersion();

  return parseOSVersion(OSXCROSS_DEFAULT_DEPLOYMENT_TARGET);
}
#else
constexpr OSVersion getDefaultDeploymentTarget() { return OSVersion(); }
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

// Information about an SDK, such as its path and version.
struct SDKInfo {
public:
  SDKInfo() : from() {}

  // Where a value of the SDK comes from, for the debug output.
  enum class From {
    UNKNOWN_FROM,   // not set
    SETTINGS_JSON,  // SDKSettings.json
    SETTINGS_PLIST, // SDKSettings.plist
    DIRECTORY_NAME, // the SDK directory name, e.g. MacOSX10.13.sdk
    SDK_VERSION     // derived from the SDK version, e.g. 10.13.99
  };

private:
  // A value together with where it comes from. It is a T, so it is read
  // like one. Only Target::readSDKInfo() sets it, and always together
  // with its source.
  template <typename T> struct Value : T {
    From from = From::UNKNOWN_FROM;

  private:
    friend struct Target;

    void set(const T &value, From valueFrom) {
      T::operator=(value);
      from = valueFrom;
    }
  };

public:
  std::string path;              // the SDK directory
  const char *from;              // where it comes from: the option, such
                                 // as "-sdk", "SDKROOT",
                                 // "OSXCROSS_SDKROOT" or "OSXCross" for an
                                 // SDK in OSXCross' SDK directory
  Value<OSVersion> version;      // its macOS version; 0.0: unknown
  // The deployment targets it accepts, 12.0 and 27.3.99 for SDK 27.3. The
  // minimum is 0.0 if the SDK states none. The maximum is version with the
  // patch level raised to 99 if the SDK states none; 0.0 if version is
  // unknown.
  Value<OSVersion> minDeploymentTarget;
  Value<OSVersion> maxDeploymentTarget;
  Value<std::string> displayName;   // "macOS 15.4"
  Value<std::string> canonicalName; // "macosx15.4", as for xcrun -sdk
};

struct Target {
  Target();

  // A source that can give the SDK: a command-line option such as -isysroot
  // or an environment variable such as SDKROOT. determineSDK() checks the
  // sources in order; the first one that does not skip decides.
  struct SDKSource {
    enum class Use {
      SKIP,        // the source gives nothing, check the next one
      DEFAULT_SDK, // the installed default SDK
      PATH         // value is the SDK directory
    };

    // Decides what the value of a given source means: skip it, use the
    // default SDK, or use the value as the SDK directory. Without a check,
    // an empty value is skipped and any other value is the SDK directory.
    typedef Use (*Check)(const std::string &value);

    // An environment variable: nullptr, as from getenv(), means not set.
    SDKSource(const char *option, const char *value, Check check = nullptr)
        : option(option), given(value != nullptr), value(value ? value : ""),
          check(check) {}

    // A command-line option: an empty value means it was not given.
    SDKSource(const char *option, const std::string &value,
              Check check = nullptr)
        : option(option), given(!value.empty()), value(value), check(check) {}

    const char *option; // "-isysroot", "-sdk", "SDKROOT", ...: for messages
    bool given;         // false: skipped
    std::string value;
    Check check;
  };

  // The SDK sources of the compiler and the tools around it, in this order:
  // -isysroot and --sysroot (commandLineIsysroot, commandLineSysroot; empty
  // outside the compiler), extra (such as -syslibroot of the ld shim),
  // SDKROOT as Apple's clang reads it, OSXCROSS_SDKROOT.
  std::vector<SDKSource>
  compilerSDKSources(std::vector<SDKSource> extra = {}) const;

  // Read the SDK in the directory path; from says where path comes from (see
  // SDKInfo::from). It need not be the SDK in use: xcodebuild -showsdks, for
  // example, needs every SDK.
  //
  // The version comes from the SDK's SDKSettings.json; without one, from
  // SDKSettings.plist, and without that, from the name of the directory
  // (15.4 for MacOSX15.4.sdk; 0.0 if the name has none). DisplayName and
  // CanonicalName come from SDKSettings.json as well;
  // without them, they are made from the version ("macOS 15.4",
  // "macosx15.4").
  static SDKInfo readSDKInfo(const std::string &path, const char *from);

  // Determine the SDK in use, sdk. The first of sources that gives one wins
  // (see SDKSource), else the default SDK:
  // <execDir>/../SDK/MacOSX<version>.sdk, named after the macOS version that
  // belongs to the target. sources come from compilerSDKSources(); xcrun and
  // xcodebuild call this through xcode::determineSDK().
  //
  // The values of the SDK come from readSDKInfo(). Prints an error, unless
  // quiet, and returns false if the directory does not exist; sdk is set
  // anyway.
  bool determineSDK(const std::vector<SDKSource> &sources,
                    bool quiet = false);

  // Pass the SDK of the last determineSDK() on to the programs that this
  // process starts. SDKROOT is set to it as an absolute path, because the
  // compiler wrapper, like Apple's clang, ignores a relative one; Apple's
  // tools and build scripts read it as well. OSXCROSS_SDKROOT is removed, so
  // that it cannot override the SDK. Call it only after determineSDK()
  // succeeded.
  void passSDKToChildProcesses() const;

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

  // <execDir>/.., where ./build_gcc.sh installs GCC. Not relative to the
  // SDK: an SDK given with -isysroot and the like can be anywhere.
  std::string gccInstallDir() const;
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
  std::string toolName;       // the name the wrapper was called as,
                              // without path and target: "gcc" for
                              // x86_64-apple-darwin20.1-gcc and o64-gcc
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
  // SDK: the options of the compiler that can give it (see
  // compilerSDKSources()), and the result of determineSDK()
  //
  std::string commandLineIsysroot; // -isysroot; empty: not given
  std::string commandLineSysroot;  // --sysroot; empty: not given
  SDKInfo sdk;                     // the SDK in use

  //
  // Compiler
  //
  Compiler compiler;
  std::string compilerName;  // clang | gcc
  std::string compilerPath;  // /usr/bin/clang | [...]/target/bin/*-gcc
  std::string compilerArgv0; // clang | *-apple-darwin-gcc
  ClangVersion clangVersion; // see findClangIntrinsicHeadersAndVersion()
  // -resource-dir, -nostdinc (--no-standard-includes) or -nobuiltininc
  // given. They are passed on to clang and decide which builtin headers it
  // uses, so setup() does not add the intrinsic headers itself.
  bool skipIntrinsicHeaders;

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
  bool linking;                    // false: -c, -E, -fsyntax-only etc.
                                   // given (see main.cpp)
  std::string intrinsicPathOption; // -foc-intrinsic-path=
  StringVector wrapperArgs;        // built by setup(); [0] is compilerArgv0
  StringVector userArgs;           // the user's arguments, after wrapperArgs
  bool useResponseFile;            // a response file (@<file>) of the user
                                   // was replaced with its arguments, so
                                   // the compiler gets its arguments in one
                                   // as well
};

} // namespace target

#endif // OSXCROSS_WRAPPER_TARGET_H
