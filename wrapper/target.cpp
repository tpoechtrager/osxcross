/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2016 by Thomas Poechtrager                      *
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

#include "compat.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include <strings.h>

#include "tools.h"
#include "target.h"

namespace target {

std::vector<Arch> getSupportedArchs(bool GCC) {
  std::vector<Arch> result;
  std::istringstream iss(getSupportedArchsString(GCC));
  std::string token;

  while (iss >> token)
    result.push_back(parseArch(token.c_str()));

  return result;
}

Arch getDefaultArch(bool GCC) {
  std::istringstream iss(getSupportedArchsString(GCC));
  std::string first;
  iss >> first;
  return parseArch(first.c_str());
}

Target::Target()
    : vendor(getDefaultVendor()), supportedarchs(getSupportedArchs()),
      arch(getDefaultArch()), target(getDefaultTarget()), stdlib(StdLib::unset),
      usegcclibs(), compiler(getDefaultCompilerIdentifier()),
      compilername(getDefaultCompilerName()), language() {
  execpath = getExecutableDir();

  if (execpath.empty()) {
    err << "cannot determine the directory of the osxcross executable"
        << err.endl();
    exit(EXIT_FAILURE);
  }

  // OSXCROSS_SDK_SEARCH_DIR was removed; tell those who still set it.
  const char *SDKSearchDir = getenv("OSXCROSS_SDK_SEARCH_DIR");

  if (SDKSearchDir && SDKSearchDir[0]) {
    err << "OSXCROSS_SDK_SEARCH_DIR is no longer supported; "
           "set OSXCROSS_SDKROOT to the SDK directory" << err.endl();
    exit(EXIT_FAILURE);
  }
}

// The macOS version of the SDK in use: with an SDK override, taken from the
// name of its directory (15.4 for "/path/to/MacOSX15.4.sdk"); otherwise the
// version that belongs to the Darwin target (15.4 for "darwin24.4"), which
// the default SDK is named after (see getDefaultSDKPath()).
OSVersion Target::getSDKOSNum() const {
  const char *SDKOverride = getenv("OSXCROSS_SDKROOT");

  if (SDKOverride && SDKOverride[0]) {
    std::string SDKPath = SDKOverride;

    // Ignore trailing slashes, as in "MacOSX15.4.sdk/".
    while (!SDKPath.empty() && SDKPath.back() == PATHDIV)
      SDKPath.pop_back();

    // MacOSX.sdk may be a symlink to a versioned SDK directory.
    // Resolve it only to determine the version, preserving the selected SDK.
    if (!strcasecmp(getFileName(SDKPath), "MacOSX.sdk")) {
      char *resolved = realpath(SDKPath.c_str(), nullptr);
      if (resolved) {
        SDKPath = resolved;
        free(resolved);
      }
    }

    const char *SDKName = getFileName(SDKPath);

    if (strncasecmp(SDKName, "MacOSX", 6))
      return OSVersion();

    return parseOSVersion(SDKName + 6);
  }

  // The Darwin version follows "darwin", for example "24.4" in "darwin24.4".
  constexpr size_t darwinLen = constexprStrLen("darwin");
  if (target.size() <= darwinLen) return OSVersion();
  const char *darwinVersion = target.c_str() + darwinLen;

  const OSVersion darwin = parseOSVersion(darwinVersion);
  const int major = darwin.major;
  const int minor = darwin.minor;

  // Darwin 27 and later correspond directly to macOS 27 and later.
  if (major >= 27)
    return OSVersion(major, minor);

  // Darwin 25 corresponds to macOS 26.
  // Darwin 26 was skipped.
  if (major >= 25)
    return OSVersion(major + 1, minor);

  // MacOS 11-15 (Darwin 20-24)
  if (major >= 20) {
    // Adjust for early versions where the minor version was offset by -1

    // Darwin 23.0 => macOS 14.0
    // Darwin 23.1 => macOS 14.1

    // Darwin 22.1 => macOS 13.0
    // Darwin 22.2 => macOS 13.1

    // ...

    return OSVersion(11 + (major - 20), major < 23 ? minor - 1 : minor);
  }

  // MacOS 10 (Darwin 4 => 10.0, ..., Darwin 19 => 10.15)
  return OSVersion(10, major - 4);
}

// The default SDK: <execpath>/../SDK/MacOSX<version>.sdk with the macOS
// version that belongs to the target (see getSDKOSNum()), or with only its
// major version ("MacOSX15.sdk" instead of "MacOSX15.0.sdk").
std::string Target::getDefaultSDKPath(bool majorVersionOnly) const {
  std::string path = execpath + "/../SDK/MacOSX";
  const OSVersion SDKVersion = getSDKOSNum();
  path += majorVersionOnly ? SDKVersion.majorStr() : SDKVersion.shortStr();

  // The 10.4 SDK is named MacOSX10.4u.sdk (universal).
  if (SDKVersion <= OSVersion(10, 4))
    path += "u";

  return path + ".sdk";
}

// The SDK in use: the SDK override if one is set, otherwise the default SDK.
// Prints an error and returns false if its directory does not exist.
bool Target::getSDKPath(std::string &path) const {
  const char *SDKOverride = getenv("OSXCROSS_SDKROOT");

  if (SDKOverride && SDKOverride[0]) {
    path = SDKOverride;
  } else {
    path = getDefaultSDKPath(false);

    // The default SDK for macOS 15.0 may also be named MacOSX15.sdk.
    if (!dirExists(path) && getSDKOSNum().minor == 0)
      path = getDefaultSDKPath(true);
  }

  if (!dirExists(path)) {
    err << "cannot find macOS SDK (expected in: " << path << ")"
        << err.endl();
    return false;
  }

  return true;
}

bool Target::getMacPortsDir(std::string &path) const {
  path = execpath;
  path += "/../macports";
  return dirExists(path);
}

bool Target::getMacPortsSysRootDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs";
  return dirExists(path);
}

bool Target::getMacPortsPkgConfigDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/lib/pkgconfig";
  return dirExists(path);
}

bool Target::getMacPortsIncludeDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/include";
  return dirExists(path);
}

bool Target::getMacPortsLibDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/lib";
  return dirExists(path);
}

bool Target::getMacPortsFrameworksDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/Library/Frameworks";
  return dirExists(path);
}

bool Target::archSupported(const Arch arch) {
  return std::find(supportedarchs.begin(), supportedarchs.end(), arch) != supportedarchs.end();
}

bool Target::checkArchs() {
  for (auto a : targetarchs) {
    if (!archSupported(a)) {
      err << "architecture '" << getArchName(a) << "' is not supported!" << err.endl();
      return false;
    }
  }

  return true;
}

void Target::addArch(const Arch arch) {
  auto &v = targetarchs;
  for (size_t i = 0; i < v.size(); ++i) {
    if (v[i] == arch) {
      v.erase(v.begin() + i);
      addArch(arch);
      return;
    }
  }
  v.push_back(arch);
}

bool Target::haveArch(const Arch arch) {
  for (auto a : targetarchs) {
    if (arch == a)
      return true;
  }
  return false;
}

bool Target::hasLibCXX() const { return getSDKOSNum() >= OSVersion(10, 7); }

bool Target::libCXXIsDefaultCXXLib() const {
  OSVersion OSNum = this->OSNum;
  OSVersion SDKOSNum = getSDKOSNum();

  if (!OSNum.Num())
    OSNum = SDKOSNum;

  return stdlib != libstdcxx && hasLibCXX() && !isGCC() &&
         (OSNum >= OSVersion(10, 9) || SDKOSNum >= OSVersion(10, 14));
}

bool Target::isCXX() {
  if (isKnownCompiler())
    return (compiler == Compiler::CLANGXX || compiler == Compiler::GXX);

  return endsWith(compilername, "++");
}

bool Target::isGCH() {
  if (!language)
    return false;

  return !strcmp(language, "c-header") ||
         !strcmp(language, "c++-header") ||
         !strcmp(language, "objective-c-header") ||
         !strcmp(language, "objective-c++-header");
}


bool Target::isClang() const {
  return (compiler == Compiler::CLANG || compiler == Compiler::CLANGXX);
}

bool Target::isGCC() const {
  return (compiler == Compiler::GCC || compiler == Compiler::GXX);
}

bool Target::isKnownCompiler() const {
  return compiler != Compiler::UNKNOWN;
}


const std::string &Target::buildDefaultTriple(std::string &triple,
  bool GCC, bool useAarch64InsteadOfArm64) const {
  triple = makeTriple(getDefaultArch(GCC), getDefaultVendor(),
                      getDefaultTarget(), useAarch64InsteadOfArm64);
  return triple;
}

std::string Target::buildToolPath(const std::string &tool) const {
  return buildToolPath(triple, tool);
}

std::string Target::buildToolPath(const std::string &toolTriple,
                                  const std::string &tool) const {
  std::string path = execpath;
  path += PATHDIV;
  path += toolTriple;
  path += "-";
  path += tool;
  return path;
}

void Target::setCompilerPath() {
  if (isGCC()) {
    compilerpath = buildToolPath("base-" + compilername);
    compilerexecname = getTriple() + "-" + compilername;
  } else {
    if (!compilerpath.empty()) {
      compilerpath += "/";
      compilerpath += compilername;
      compilerexecname = compilername;
    } else {
      compilerpath = findExecutableInPath(compilername, true, ignoreCCACHE);

      if (compilerpath.empty())
        compilerpath = compilername;

      compilerexecname += compilername;
    }
  }
}

bool Target::findClangIntrinsicHeadersAndVersion(std::string &path) {
  assert(isClang());

  auto warnHeadersNotFound = [](const std::string &reason) {
    warn << "cannot find clang intrinsic headers or determine the clang "
            "version: " << reason << warn.endl();
    return false;
  };

  auto warnVersionNotFound = [](const std::string &reason) {
    warn << "cannot determine the clang version: " << reason << warn.endl();
  };

  if (compilerpath.empty())
    return warnHeadersNotFound("compiler path is empty");

  static constexpr const char *IntrinHeader = "xmmintrin.h";

  std::string clangbindir = compilerpath;
  stripFileName(clangbindir);

  // Every successful lookup ends here.
  auto found = [&](const std::string &intrindir, const ClangVersion &cv) {
    path = intrindir;
    clangversion = cv;
    return true;
  };

  // Returns <dir>/include or <dir>, whichever contains xmmintrin.h, or an
  // empty string.
  auto findHeaderDir = [](const std::string &dir) -> std::string {
    for (const std::string &d : { dir + "/include", dir }) {
      if (fileExists(d + "/" + IntrinHeader))
        return d;
    }

    return std::string();
  };

  // Looks for <dir>/<version>/include/xmmintrin.h or
  // <dir>/<version>/xmmintrin.h; the highest version wins.
  auto tryDir = [&](const std::string &dir) {
    ClangVersion bestversion;
    std::string bestpath;

    for (const std::string &file : listFiles(dir)) {
      if (file[0] == '.' || !dirExists(dir + "/" + file))
        continue;

      const ClangVersion cv = parseClangVersion(file.c_str());

      if (cv == ClangVersion())
        continue;

      const std::string intrindir = findHeaderDir(dir + "/" + file);

      if (!intrindir.empty() && cv > bestversion) {
        bestversion = cv;
        bestpath = intrindir;
      }
    }

    if (bestversion == ClangVersion())
      return false;

    return found(bestpath, bestversion);
  };

  std::vector<std::string> dirs;

  // -foc-intrinsic-path is an explicit choice, so it is checked first. It
  // has always been appended to the clang bin directory as it is; that
  // stays, and an absolute path or a relative path without a leading '/'
  // is tried as well.
  if (!intrinsicpath.empty()) {
    dirs.push_back(clangbindir + intrinsicpath);

    if (intrinsicpath[0] == '/')
      dirs.push_back(intrinsicpath);
    else
      dirs.push_back(clangbindir + "/" + intrinsicpath);
  }

  dirs.push_back(clangbindir + "/../lib/clang");

#ifdef __linux__
#ifdef __x86_64__
  // opensuse uses lib64 instead of lib on x86_64
  dirs.push_back(clangbindir + "/../lib64/clang");
  // Fedora installs clang into /usr/lib64/llvm*
  dirs.push_back(clangbindir + "/../../../lib/clang/");
  // Gentoo installs clang into /usr/lib/llvm/version
  dirs.push_back(clangbindir + "/../../../clang/");
#elif __i386__
  dirs.push_back(clangbindir + "/../lib32/clang");
#endif
#endif

  dirs.push_back(clangbindir + "/../include/clang");
  dirs.push_back(clangbindir + "/usr/include/clang");

  // OSXCROSS_FIND_INTRINSICS_VIA_PRINT_RESOURCE_DIR skips the guesses and
  // goes straight to the last resort below (for tests).
  const bool forceresourcedir =
      getenv("OSXCROSS_FIND_INTRINSICS_VIA_PRINT_RESOURCE_DIR") != nullptr;

  if (!forceresourcedir) {
    for (const std::string &dir : dirs) {
      if (tryDir(dir))
        return true;
    }
  }

  // Last resort: ask the compiler for its resource directory. This covers
  // layouts the guesses above do not know, e.g. NixOS, where bin/clang is a
  // wrapper script in a different store path than the headers. It is only
  // the last resort because it starts clang as a separate process (twice if
  // the version has to come from --version), which is much slower than
  // looking at a few directories, and this runs on every wrapper call.
  if (getDebugLevel() > 0) {
    dbg << (forceresourcedir
                ? "OSXCROSS_FIND_INTRINSICS_VIA_PRINT_RESOURCE_DIR is set"
                : "clang intrinsic headers not found in the known directories; "
                  "clang version could not be determined from them")
        << "; asking '" << compilerpath << " -print-resource-dir'"
        << dbg.endl();
  }

  std::string resourcedir;

  if (!getProgramOutput({compilerpath, "-print-resource-dir"}, resourcedir))
    return warnHeadersNotFound("'" + compilerpath + " -print-resource-dir' failed");

  while (!resourcedir.empty() &&
         (resourcedir.back() == '\n' || resourcedir.back() == '\r'))
    resourcedir.pop_back();

  if (resourcedir.empty())
    return warnHeadersNotFound("'" + compilerpath +
                               " -print-resource-dir' returned an empty "
                               "resource directory");

  const std::string intrindir = findHeaderDir(resourcedir);

  if (intrindir.empty())
    return warnHeadersNotFound("resource directory '" + resourcedir +
                               "' returned by '" + compilerpath +
                               " -print-resource-dir' contains no intrinsic headers");

  // The resource directory is usually <prefix>/lib/clang/<version>.
  ClangVersion cv = parseClangVersion(getFileName(resourcedir));

  if (cv == ClangVersion()) {
    // No version in the path (NixOS: .../resource-root), so parse
    // "... clang version X.Y.Z ..." or "Apple LLVM version X.Y.Z" from
    // --version. -dumpversion is not used because Clang < 14 prints 4.2.1.
    static const char *const needles[] = {
      "clang version ", "Apple LLVM version "
    };
    std::string version;

    if (!getProgramOutput({compilerpath, "--version"}, version)) {
      warnVersionNotFound("'" + compilerpath + " --version' failed");
    } else {
      for (const char *needle : needles) {
        const size_t pos = version.find(needle);

        if (pos != std::string::npos) {
          cv = parseClangVersion(version.c_str() + pos + strlen(needle));
          if (cv != ClangVersion())
            break;
        }
      }

      if (cv == ClangVersion())
        warnVersionNotFound("unrecognized output from '" + compilerpath +
                            " --version'");
    }
  }

  return found(intrindir, cv);
}

// The GCC runtime is linked statically, unless the user asks for the shared
// one.
static bool useStaticGCCRuntime() {
  return !getenv("OSXCROSS_GCC_NO_STATIC_RUNTIME");
}

// Links the libstdc++ and libgcc installed by ./build_gcc.sh (clang++-gstdc++)
// instead of the default libraries. They are found relative to the SDK:
//
//   <SDK>/../../<gcctriple>/lib[/i386]                  libstdc++, libsupc++
//   <SDK>/../../lib/gcc/<gcctriple>/<version>[/i386]    libgcc, libemutls_w ...
//
// The i386 libraries are a multilib of the x86_64 GCC installation.
void Target::addGCCRuntimeLibs(const std::string &SDKPath, Arch arch) {
  assert(stdlib == StdLib::libstdcxx && usegcclibs);
  assert(!gcctriple.empty());

  const std::string multilib = arch == Arch::i386 ? "/i386" : "";
  const std::string libgccDir = SDKPath + "/../../lib/gcc/" + gcctriple + "/" +
                                gccversion.Str() + multilib;
  const std::string libstdcxxDir = SDKPath + "/../../" + gcctriple + "/lib" +
                                   multilib;
  const bool dynamic = !useStaticGCCRuntime();

  fargs.push_back("-nodefaultlibs");

  if (dynamic) {
    fargs.push_back("-L");
    fargs.push_back(libgccDir);
    fargs.push_back("-L");
    fargs.push_back(libstdcxxDir);
  }

  auto addLib = [&](const std::string &dir, const char *lib) {
    if (dynamic) {
      fargs.push_back("-l");
      fargs.push_back(lib);
    } else {
      fargs.push_back(dir + "/lib" + lib + ".a");
    }
  };

  fargs.push_back("-Qunused-arguments");

  addLib(libstdcxxDir, "stdc++");
  addLib(libstdcxxDir, "supc++");
  addLib(libgccDir, "gcc");
  // Fix #484: use the system unwinder with modern Darwin GCC runtimes.
  // https://github.com/tpoechtrager/osxcross/issues/484
  // Older GCC installations do not provide the weak emulated-TLS archive.
  const bool useSystemUnwinder = fileExists(libgccDir + "/libemutls_w.a");
  if (useSystemUnwinder) {
    addLib(libgccDir, "emutls_w");
    // Heap trampoline support is not present in every GCC version.
    if (fileExists(libgccDir + "/libheapt_w.a"))
      addLib(libgccDir, "heapt_w");
  } else {
    addLib(libgccDir, "gcc_eh");
  }

  fargs.push_back("-lc");

  // Fix #484: Clang can emit compact-unwind-only frames.
  // Preserve them for the system unwinder; the legacy GCC unwinder needs
  // DWARF instead. ld64.lld does not implement -no_compact_unwind.
  if (!useSystemUnwinder && !buildFlavor.IsLLVM())
    fargs.push_back("-Wl,-no_compact_unwind");
}

std::string makeTriple(Arch arch, const char *vendor, const std::string &target,
                       bool useAarch64InsteadOfArm64) {
  if (arch == Arch::arm64 && useAarch64InsteadOfArm64)
    arch = Arch::aarch64;

  std::string triple = getArchName(arch);
  triple += "-";
  triple += vendor;
  triple += "-";
  triple += target;
  return triple;
}

void Target::setTriple(bool useAarch64InsteadOfArm64) {
  triple = makeTriple(arch, vendor, target, useAarch64InsteadOfArm64);
}

bool Target::setup() {
  //
  // Architectures, SDK, triple and compiler
  //

  if (targetarchs.empty())
    addArch(arch);

  if (!checkArchs())
    return false;

  std::string SDKPath;
  const OSVersion SDKOSNum = getSDKOSNum();

  if (!isKnownCompiler())
    warn << "unknown compiler '" << compilername << "'" << warn.endl();

  if (!getSDKPath(SDKPath))
    return false;

  setTriple();
  setCompilerPath();

  // SDK versions the architectures need: i386 requires the 10.13 SDK or an
  // earlier one (orEarlier), the others a minimum SDK version.
  constexpr struct {
    Arch arch;
    OSVersion SDKVer;
    bool orEarlier;
  } RequiredSDKVersion[] = {
    { Arch::i386,    {10, 13}, true },
    { Arch::x86_64h, {10, 8} },
    { Arch::arm64,   {11, 0} },
    { Arch::arm64e,  {11, 0} },
  };

  for (auto &RequiredSDK : RequiredSDKVersion) {
    if (!haveArch(RequiredSDK.arch))
      continue;

    const bool SDKSupported = RequiredSDK.orEarlier
                                  ? SDKOSNum <= RequiredSDK.SDKVer
                                  : SDKOSNum >= RequiredSDK.SDKVer;

    if (!SDKSupported) {
      err << "Architecture '" << getArchName(RequiredSDK.arch) << "' requires "
          << "macOS " << RequiredSDK.SDKVer.shortStr() << " SDK "
          << (RequiredSDK.orEarlier ? "(or earlier)" : "(or later)")
          << err.endl();
      return false;
    }
  }

  //
  // Deployment target (-mmacosx-version-min)
  //

  if (!OSNum.Num()) {
    const OSVersion defaultMinTarget = getDefaultMinTarget();

    if (defaultMinTarget != OSVersion()) {
      // Default version is given via -DOSXCROSS_OSX_VERSION_MIN=XX,
      // raised to what the architectures and the C++ library need.
      OSNum = defaultMinTarget;

      if (haveArch(Arch::arm64) || haveArch(Arch::arm64e)) {
        // Default to >= 11.0 for arm64
        OSNum = std::max(OSNum, OSVersion(11, 0));
      }

      if (haveArch(Arch::x86_64h)) {
        // Default to >= 10.8 for x86_64h
        OSNum = std::max(OSNum, OSVersion(10, 8));
      }

      if (stdlib == StdLib::libcxx) {
        // Default to >= 10.7 for libc++
        OSNum = std::max(OSNum, OSVersion(10, 7));
      }
    } else {
      // No -DOSXCROSS_OSX_VERSION_MIN=XX: default to the SDK version
      OSNum = SDKOSNum;
    }
  }

  //
  // C++ standard library
  //

  if (stdlib == StdLib::unset) {
    stdlib = libCXXIsDefaultCXXLib() ? StdLib::libcxx : StdLib::libstdcxx;
  } else if (stdlib == StdLib::libcxx) {
    if (!hasLibCXX()) {
      err << "libc++ requires macOS SDK 10.7 (or later)" << err.endl();
      return false;
    }

    if (OSNum.Num() && OSNum < OSVersion(10, 7)) {
      err << "libc++ requires '-mmacosx-version-min=10.7' (or later)"
          << err.endl();
      return false;
    }
  }

  if (SDKOSNum >= OSVersion(10, 14) && stdlib == StdLib::libstdcxx &&
      !isGCC() && !usegcclibs) {
    err << "macOS SDK '>= 10.14' does not support libstdc++ anymore"
        << err.endl();
    return false;
  }

  if (OSNum > SDKOSNum) {
    err << "targeted macOS version must be <= " << SDKOSNum.Str() << " (SDK)"
        << err.endl();
    return false;
  }

  if (OSNum < OSVersion(10, 4)) {
    err << "targeted macOS version must be >= 10.4" << err.endl();
    return false;
  }

  // The libstdc++ and libgcc installed by ./build_gcc.sh
  // (clang++-gstdc++, -foc-use-gcc-libstdc++).
  const bool useGCCLibstdcxx = stdlib == StdLib::libstdcxx && usegcclibs;

  //
  // C++ standard library headers
  //
  // They are added as -isystem <CXXHeaderPath> and, for each entry of
  // CXXHeaderSubDirs, as -isystem <CXXHeaderPath>/<subdir>.
  //

  std::string CXXHeaderPath = SDKPath;
  string_vector CXXHeaderSubDirs;

  switch (stdlib) {
  case StdLib::libcxx: {
    CXXHeaderPath += "/usr/include/c++/v1";
    if (!dirExists(CXXHeaderPath)) {
      err << "cannot find " << getStdLibString(stdlib) << " headers"
          << err.endl();
      return false;
    }
    break;
  }
  case StdLib::libstdcxx: {
    // GCC brings its own libstdc++ headers; CXXHeaderPath stays the SDK
    // directory.
    if (isGCC())
      break;

    if (usegcclibs) {
      // Use libs from './build_gcc.sh' installation

      CXXHeaderPath += "/../../";

      // Build the target triple name we BUILD for - not what triple has been invoked.
      // i.e. x86_64-apple-darwin27-clang++-gstdc++ -arch arm64
      //      -> results in aarch64-apple-darwin27

      Arch CXXBuildArch;

      if (targetarchs.size() > 1) { // TODO: should probably check earlier for this
        err << "clang++-gstdc++ does not support multiple architectures in "
              "one invocation"
            << err.endl();
        return false;
      }

      switch (targetarchs[0]) {
      case Arch::arm64:
        CXXBuildArch = Arch::aarch64;
        break;
      case Arch::i386:
      case Arch::x86_64:
        // i386 is a multilib of the x86_64 GCC installation;
        CXXBuildArch = Arch::x86_64;
        break;
      default:
        err << "clang++-gstdc++ does not support architecture '"
            << getArchName(targetarchs[0]) << "'" << err.endl();
        return false;
      }
      const std::string CXXBuildTriple =
          makeTriple(CXXBuildArch, vendor, target);

      CXXHeaderPath += CXXBuildTriple;
      CXXHeaderPath += "/include/c++";

      // The highest installed GCC version wins.
      std::vector<GCCVersion> v;

      for (const std::string &entry : listFiles(CXXHeaderPath)) {
        if (entry[0] != '.')
          v.push_back(parseGCCVersion(entry.c_str()));
      }

      if (v.empty()) {
        err << "'-foc-use-gcc-libstdc++' requires gcc to be installed "
               "(./build_gcc.sh)" << err.endl();
        return false;
      }

      std::sort(v.begin(), v.end());
      gccversion = v.back();
      gcctriple = CXXBuildTriple;

      CXXHeaderPath += "/";
      CXXHeaderPath += gccversion.Str();

      CXXHeaderSubDirs.push_back(CXXBuildTriple);
    } else {
      // Use SDK libs
      if (SDKOSNum <= OSVersion(10, 5))
        CXXHeaderPath += "/usr/include/c++/4.0.0";
      else
        CXXHeaderPath += "/usr/include/c++/4.2.1";

      CXXHeaderSubDirs.push_back(makeTriple(arch, vendor, target));
    }

    CXXHeaderSubDirs.push_back("backward");

    if (!dirExists(CXXHeaderPath)) {
      err << "cannot find " << getStdLibString(stdlib) << " headers"
          << err.endl();
      return false;
    }

    break;
  }
  case StdLib::unset:
    abort();
  }

  //
  // Compiler arguments
  //
  // fargs come first, the user's arguments (args) follow them, so the user
  // can override most of what is set here.
  //

  fargs.push_back(compilerexecname);

  std::string ClangIntrinsicPath;

  if (isClang()) {
    fargs.push_back("-target");
    fargs.push_back(getTriple());

    const char *linkerVersion = getLinkerVersion();

    // -mlinker-version= is only relevant for ld64.
    if (!buildFlavor.IsLLVM() && linkerVersion && linkerVersion[0])
      fargs.push_back(std::string("-mlinker-version=") + linkerVersion);

    if (findClangIntrinsicHeadersAndVersion(ClangIntrinsicPath)) {
      // The resource-directory lookup warns when the version is unknown.
      if (haveArch(Arch::x86_64h) && clangversion != ClangVersion() &&
          clangversion < ClangVersion(3, 5)) {
        err << "'" << getArchName(Arch::x86_64h) << "' requires clang 3.5 "
            << "(or later)" << err.endl();
        return false;
      }
    }

    fargs.push_back("-isysroot");
    fargs.push_back(SDKPath);

    if (isCXX()) {
      fargs.push_back(std::string("-stdlib=") + getStdLibString(stdlib));

      if (stdlib == StdLib::libcxx || useGCCLibstdcxx) {
        // The C++ headers are added with -isystem below.
        // -Qunused-arguments: -nostdinc++ is unused when linking.
        fargs.push_back("-nostdinc++");
        fargs.push_back("-Qunused-arguments");
      }

      if (useGCCLibstdcxx) {
        // Match the TLS ABI of the libstdc++ built by build_gcc.sh.
        // User arguments follow fargs, so -fno-emulated-tls can override this.
        fargs.push_back("-femulated-tls");

        // Use libs from './build_gcc.sh' installation
        if (!isGCH())
          addGCCRuntimeLibs(SDKPath, targetarchs[0]);
      }
    }
  } else if (isGCC()) {
    if (isCXX() && stdlib == StdLib::libcxx) {
      fargs.push_back("-nostdinc++");
      fargs.push_back("-nodefaultlibs");

      if (!isGCH()) {
        fargs.push_back("-lc");
        fargs.push_back("-lc++");
        if (SDKOSNum <= OSVersion(10, 13)) {
          // SDK 10.14 does not have -lgcc_s anymore
          fargs.push_back("-lgcc_s.10.5");
        }
      }
    } else if (stdlib != StdLib::libcxx && !isGCH() &&
               useStaticGCCRuntime()) {
      fargs.push_back("-static-libgcc");
      fargs.push_back("-static-libstdc++");
    }

    if (!buildFlavor.IsLLVM() && !isGCH())
      fargs.push_back("-Wl,-no_compact_unwind");
  }

  auto addCXXHeaderPath = [&](const std::string &path) {
    fargs.push_back("-isystem");
    fargs.push_back(path);
  };

  addCXXHeaderPath(CXXHeaderPath);

  for (auto &subDir : CXXHeaderSubDirs)
    addCXXHeaderPath(CXXHeaderPath + "/" + subDir);

  if (getenv("OSXCROSS_MP_INC")) {
    std::string MacPortsIncludeDir;
    std::string MacPortsLibraryDir;
    std::string MacPortsFrameworksDir;

    // Add them to args (instead of fargs),
    // so the user's -I / -L / -F is prefered.

    if (getMacPortsIncludeDir(MacPortsIncludeDir)) {
      args.push_back("-isystem");
      args.push_back(MacPortsIncludeDir);

      if (getMacPortsLibDir(MacPortsLibraryDir)) {
        if (isClang())
          args.push_back("-Qunused-arguments");

        args.push_back("-L");
        args.push_back(MacPortsLibraryDir);
      }

      if (getMacPortsFrameworksDir(MacPortsFrameworksDir)) {
        args.push_back("-iframework");
        args.push_back(MacPortsFrameworksDir);
      }
    }
  }

  // Clang's intrinsic headers (see findClangIntrinsicHeadersAndVersion()).
  if (isClang() && !ClangIntrinsicPath.empty()) {
    fargs.push_back("-isystem");
    fargs.push_back(ClangIntrinsicPath);
  }

  if (OSNum.Num()) {
    std::string versionMin = OSNum.Str();

    if (isClang() && clangversion < ClangVersion(11, 0) &&
        OSNum >= OSVersion(11, 0)) {
      // Clang <= 10 can't parse -mmacosx-version-min=11.x
      warn << "your clang installation is outdated and can't parse "
              "'-mmacosx-version-min=" << OSNum.shortStr() << "'. "
           << "setting it to 10.16." << warn.endl();
      versionMin = "10.16";
    }

    fargs.push_back("-mmacosx-version-min=" + versionMin);
  }

  for (auto arch : targetarchs) {
    switch (arch) {
    case Arch::i386:
    case Arch::i486:
    case Arch::i586:
    case Arch::i686:
    case Arch::arm64:
    case Arch::arm64e:
    case Arch::x86_64:
    case Arch::x86_64h:
      break;
    default:
      err << "unsupported architecture: '" << getArchName(arch) << "'"
          << err.endl();
      return false;
    }

    if (isGCC()) {
      if (arch != Arch::x86_64 && arch != Arch::i386 &&
          arch != Arch::arm64) {
        err << "gcc does not support architecture '" << getArchName(arch)
            << "'" << err.endl();
        return false;
      }

      if (targetarchs.size() > 1)
        continue;

      // arm64 needs neither -m32 nor -m64.
      if (arch != Arch::arm64)
        fargs.push_back(arch == Arch::i386 ? "-m32" : "-m64");
    } else if (isClang()) {
      // -foc-use-gcc-libstdc++ does not support multiple architectures.
      if (usegcclibs && targetarchs.size() > 1)
        continue;

      fargs.push_back("-arch");
      fargs.push_back(getArchName(arch));
    }
  }

#ifdef __ANDROID__
  // Workaround for Termux
  std::string LDSysRoot = "-Wl,-syslibroot,";
  LDSysRoot += SDKPath;
  fargs.push_back(LDSysRoot);
#endif

  if (isClang()) {
    auto hasArg = [&](const char *arg) {
      return std::find(args.begin(), args.end(), arg) != args.end();
    };

    // Link with ld64.lld; not needed when only compiling (-c), preprocessing
    // (-E) or generating assembly (-S).
    if (buildFlavor.IsLLVM() && !hasArg("-c") && !hasArg("-E") &&
        !hasArg("-S")) {
      fargs.push_back("-fuse-ld=lld");
    }

    if (getenv("OSXCROSS_PRETEND_TO_BE_APPLE_CLANG")) {
      fargs.push_back("-D__apple_build_version__=1");
    }

    if (SDKOSNum >= OSVersion(14, 0) && clangversion < ClangVersion(17, 0)) {
      // MacOS 14 SDK uses __ENVIRONMENT_OS_VERSION_MIN_REQUIRED__ in AvailabilityInternal.h
      fargs.push_back("-D__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__=" + OSNum.numStr());
    }
    if (clangversion >= ClangVersion(3, 8)) {
      //
      // Silence:
      // warning: libLTO.dylib relative to clang installed dir not found;
      //          using 'ld' default search path instead
      //
      // '-flto' will of course work nevertheless, it's just a buggy
      // cross-compilation warning.
      //
      // User arguments follow fargs, so -Wliblto can override this default.
      fargs.push_back("-Wno-liblto");
    }

    if (getenv("OSXCROSS_ENABLE_WERROR_IMPLICIT_FUNCTION_DECLARATION"))
      fargs.push_back("-Werror=implicit-function-declaration");
  } else if (isGCC()) {
    if (args.empty() || (args.size() == 1 && args[0] == "-v")) {
      //
      // HACK:
      // Discard all arguments besides the first one
      // (which is <arch>-apple-darwinXX-gcc) to fix the issue
      // described in #135.
      //

      fargs.resize(1);
    }
  }

  //
  // Legacy deployment targets and the environment of the invoked tools
  //

  // GCC or clang++-gstdc++: GCC's libstdc++ is used.
  const bool isgcclibstdcxx = isGCC() || (isClang() && useGCCLibstdcxx);

  if (OSNum <= OSVersion(10, 5)) {
    auto suggestOldBranch = []() {
      info << "use 'osxcross-1.1' branch instead" << info.endl();
    };

    if (isgcclibstdcxx) {
      err << "building for macOS '<= 10.5' with GCC (or clang++-gstdc++) "
             "is no longer supported" << err.endl();
      suggestOldBranch();
      return false;
    }

    if (isClang()) {
      // OSXCROSS_NO_10_5_DEPRECATION_WARNING silences the warning and the
      // hint.
      if (!getenv("OSXCROSS_NO_10_5_DEPRECATION_WARNING")) {
        warn << "building for macOS '<= 10.5' "
                "is no longer supported" << warn.endl();
        suggestOldBranch();
      }
    } else {
      // Unknown compiler: only the hint.
      suggestOldBranch();
    }
  }

  if (buildFlavor.IsLLVM() && isGCC()) {
    // The LLVM as wrapper needs GCC's selected deployment target.
    setenv("OSXCROSS_AS_TARGET_VERSION", OSNum.shortStr().c_str(), 1);
  } else if (isgcclibstdcxx) {
    // Silence 'operator new[]' warning in ld64.
    setenv("OSXCROSS_GCC_LIBSTDCXX", "1", 1);
  }

  return true;
}
} // namespace target
