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
#include <sys/stat.h>

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

  detectSDK();
}

// OSXCROSS_SDKROOT selects the SDK; otherwise OSXCROSS_SDK_SEARCH_DIR may
// provide one. Without either, the SDK in <execpath>/../SDK is used.
void Target::detectSDK() {
  const char *SDKRoot = getenv("OSXCROSS_SDKROOT");
  SDK = SDKRoot ? SDKRoot : "";

  const char *SDKSearchDir = getSDKSearchDir();

  if (SDK.empty() && SDKSearchDir[0])
    overrideDefaultSDKPath(SDKSearchDir);
}

// The macOS version of the selected SDK, taken from the name of its
// directory (15.4 for "/path/to/MacOSX15.4.sdk"); if no SDK is selected,
// the version that belongs to the Darwin target (15.4 for "darwin24.4"),
// because the default SDK in <execpath>/../SDK is named after it.
OSVersion Target::getSDKOSNum() const {
  if (!SDK.empty()) {
    std::string SDKPath = SDK;

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

void Target::overrideDefaultSDKPath(const char *SDKSearchDir) {
  std::string defaultSDKPath;

  defaultSDKPath = SDKSearchDir;
  defaultSDKPath += PATHDIV;
  defaultSDKPath += "default";

  struct stat st;

  if (!lstat(defaultSDKPath.c_str(), &st)) {
    if (!S_ISLNK(st.st_mode)) {
      err << "'" << defaultSDKPath << "' must be a symlink to an SDK"
          << err.endl();
      exit(EXIT_FAILURE);
    }

    if (char *resolved = realpath(defaultSDKPath.c_str(), nullptr)) {
      SDK = resolved;
      free(resolved);
    } else {
      err << "'" << defaultSDKPath << "' broken symlink" << err.endl();
      exit(EXIT_FAILURE);
    }
  } else {
    // Choose the latest SDK

    OSVersion latestSDKVersion;
    std::string latestSDK;

    for (const std::string &entry : listFiles(SDKSearchDir)) {
      if (strncasecmp(entry.c_str(), "MacOSX", 6))
        continue;

      const OSVersion SDKVersion = parseOSVersion(entry.c_str() + 6);

      if (SDKVersion > latestSDKVersion) {
        latestSDKVersion = SDKVersion;
        latestSDK = entry;
      }
    }

    if (!latestSDKVersion.Num()) {
      err << "no SDK found in '" << SDKSearchDir << "'" << err.endl();
      exit(EXIT_FAILURE);
    }

    std::string SDKPath;

    SDKPath = SDKSearchDir;
    SDKPath += PATHDIV;
    SDKPath += latestSDK;

    SDK = SDKPath;
  }
}

bool Target::getSDKPath(std::string &path, bool majorVersionOnly) const {
  OSVersion SDKVer = getSDKOSNum();

  if (!SDK.empty()) {
    path = SDK;
  } else {
    path = execpath;
    path += "/../SDK/MacOSX";
    if (majorVersionOnly) {
      path += SDKVer.majorStr();
    } else {
      path += SDKVer.shortStr();
    }
    if (SDKVer <= OSVersion(10, 4))
      path += "u";
    path += ".sdk";
  }

  if (!dirExists(path)) {
    if (SDKVer.minor == 0 && !majorVersionOnly)
      return getSDKPath(path, true);

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

bool Target::findClangIntrinsicHeaders(std::string &path) {
  assert(isClang());

  if (compilerpath.empty())
    return false;

  std::string clangbindir = compilerpath;
  stripFileName(clangbindir);

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

      for (const std::string &intrindir :
           { dir + "/" + file + "/include", dir + "/" + file }) {
        if (fileExists(intrindir + "/xmmintrin.h")) {
          if (cv > bestversion) {
            bestversion = cv;
            bestpath = intrindir;
          }
          break;
        }
      }
    }

    clangversion = bestversion;

    if (bestversion == ClangVersion())
      return false;

    path = bestpath;
    return true;
  };

  std::vector<std::string> dirs;

#ifdef __CYGWIN__
#ifdef __x86_64__
  dirs.push_back(clangbindir + "/../lib/clang/x86_64-pc-cygwin");
#else
  dirs.push_back(clangbindir + "/../lib/clang/i686-pc-cygwin");
#endif
#endif

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

#ifdef __APPLE__
  dirs.push_back("/Library/Developer/CommandLineTools/usr/lib/clang");
  dirs.push_back("/Applications/Contents/Developer/Toolchains/"
                 "XcodeDefault.xctoolchain/usr/lib/clang");
#endif

  dirs.push_back(clangbindir + "/../include/clang");
  dirs.push_back(clangbindir + "/usr/include/clang");

  if (!intrinsicpath.empty())
    dirs.push_back(clangbindir + intrinsicpath);

  for (const std::string &dir : dirs) {
    if (tryDir(dir))
      return true;
  }

  return false;
}

void Target::setupGCCLibs(Arch arch) {
  assert(stdlib == StdLib::libstdcxx);
  fargs.push_back("-nodefaultlibs");

  std::string SDKPath;
  std::stringstream GCCLibSTDCXXPath;
  std::stringstream GCCLibPath;

  const bool dynamic = !!getenv("OSXCROSS_GCC_NO_STATIC_RUNTIME");
  // The i386 runtime is a multilib of the x86_64 GCC installation.
  const std::string GCCTriple = makeTriple(
      arch == Arch::arm64 ? Arch::aarch64 : Arch::x86_64, vendor, target);

  getSDKPath(SDKPath);

  GCCLibPath << SDKPath << "/../../lib/gcc/" << GCCTriple << "/"
             << gccversion.Str();

  GCCLibSTDCXXPath << SDKPath << "/../../" << GCCTriple << "/lib";

  switch (arch) {
  case Arch::i386:
  case Arch::i486:
  case Arch::i586:
  case Arch::i686:
    GCCLibPath << "/" << getArchName(Arch::i386);
    GCCLibSTDCXXPath << "/" << getArchName(Arch::i386);
    break;
  default:
    ;
  }

  if (dynamic) {
    fargs.push_back("-L");
    fargs.push_back(GCCLibPath.str());
    fargs.push_back("-L");
    fargs.push_back(GCCLibSTDCXXPath.str());
  }

  auto addLib = [&](const std::stringstream &path, const char *lib) {
    if (dynamic) {
      fargs.push_back("-l");
      fargs.push_back(lib);
    } else {
      fargs.push_back(path.str() + "/lib" + lib + ".a");
    }
  };

  fargs.push_back("-Qunused-arguments");

  addLib(GCCLibSTDCXXPath, "stdc++");
  addLib(GCCLibSTDCXXPath, "supc++");
  addLib(GCCLibPath, "gcc");
  // Fix #484: use the system unwinder with modern Darwin GCC runtimes.
  // https://github.com/tpoechtrager/osxcross/issues/484
  // Older GCC installations do not provide the weak emulated-TLS archive.
  const bool useSystemUnwinder =
      fileExists(GCCLibPath.str() + "/libemutls_w.a");
  if (useSystemUnwinder) {
    addLib(GCCLibPath, "emutls_w");
    // Heap trampoline support is not present in every GCC version.
    if (fileExists(GCCLibPath.str() + "/libheapt_w.a"))
      addLib(GCCLibPath, "heapt_w");
  } else {
    addLib(GCCLibPath, "gcc_eh");
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
  if (targetarchs.empty())
    addArch(arch);

  if (!checkArchs()) {
    return false;
  }

  std::string SDKPath;
  OSVersion SDKOSNum = getSDKOSNum();

  if (!isKnownCompiler())
    warn << "unknown compiler '" << compilername << "'" << warn.endl();

  if (!getSDKPath(SDKPath))
    return false;

  setTriple();
  setCompilerPath();

  constexpr struct {
    Arch arch;
    OSVersion SDKVer;
    bool lower;
  } RequiredSDKVersion[] = {
    { Arch::i386,    {10, 13}, true },
    { Arch::x86_64h, {10, 8} },
    { Arch::arm64,   {11, 0} },
    { Arch::arm64e,  {11, 0} },
  };

  for (auto &RequiredSDK : RequiredSDKVersion) {
    if (haveArch(RequiredSDK.arch)) {

      if (RequiredSDK.lower) {
        if (SDKOSNum > RequiredSDK.SDKVer) {
          err << "Architecture '" << getArchName(RequiredSDK.arch) << "' requires "
              << "macOS " << RequiredSDK.SDKVer.shortStr() << " SDK (or earlier)"
              << err.endl();
          return false;
        }
      } else {
        if (SDKOSNum < RequiredSDK.SDKVer) {
          err << "Architecture '" << getArchName(RequiredSDK.arch) << "' requires "
              << "macOS " << RequiredSDK.SDKVer.shortStr() << " SDK (or later)"
              << err.endl();
          return false;
        }
      }

    }
  }

  if (!OSNum.Num()) {
    OSVersion defaultMinTarget = getDefaultMinTarget();

    if (defaultMinTarget != OSVersion()) {
      // Default version is given via -DOSXCROSS_OSX_VERSION_MIN=XX

      if (haveArch(Arch::arm64) || haveArch(Arch::arm64e)) {
        // Default to >= 11.0 for arm64
        OSNum = std::max(defaultMinTarget, OSVersion(11, 0));
      }

      if (haveArch(Arch::x86_64h)) {
        // Default to >= 10.8 for x86_64h
        OSNum = std::max(OSNum, std::max(defaultMinTarget, OSVersion(10, 8)));
      }

      if (stdlib == StdLib::libcxx) {
        // Default to >= 10.7 for libc++
        OSNum = std::max(OSNum, std::max(defaultMinTarget, OSVersion(10, 7)));
      }
    }

    if (!OSNum.Num()) {
      // Default min version = -DOSXCROSS_OSX_VERSION_MIN=XX or SDK version
      OSNum = defaultMinTarget != OSVersion() ? defaultMinTarget : SDKOSNum;
    }
  }

  if (stdlib == StdLib::unset) {
    if (libCXXIsDefaultCXXLib()) {
      stdlib = StdLib::libcxx;
    } else {
      stdlib = StdLib::libstdcxx;
    }
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

  if (SDKOSNum >= OSVersion(10, 14)) {
    if (!isGCC() && !usegcclibs && stdlib == StdLib::libstdcxx) {
        err << "macOS SDK '>= 10.14' does not support libstdc++ anymore"
            << err.endl();
        return false;
    }
  }

  if (OSNum > SDKOSNum) {
    err << "targeted macOS version must be <= " << SDKOSNum.Str() << " (SDK)"
        << err.endl();
    return false;
  } else if (OSNum < OSVersion(10, 4)) {
    err << "targeted macOS version must be >= 10.4" << err.endl();
    return false;
  }

  std::string CXXHeaderPath = SDKPath;
  string_vector AdditionalCXXHeaderPaths;

  auto addCXXPath = [&](const std::string &path) {
    std::string tmp;
    tmp = CXXHeaderPath;
    tmp += "/";
    tmp += path;
    AdditionalCXXHeaderPaths.push_back(tmp);
  };

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
      gccversion = v[v.size() - 1];

      CXXHeaderPath += "/";
      CXXHeaderPath += gccversion.Str();

      addCXXPath(CXXBuildTriple);
    } else {
      // Use SDK libs
      if (SDKOSNum <= OSVersion(10, 5))
        CXXHeaderPath += "/usr/include/c++/4.0.0";
      else
        CXXHeaderPath += "/usr/include/c++/4.2.1";

      addCXXPath(makeTriple(arch, vendor, target));
    }

    addCXXPath("backward");

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

  fargs.push_back(compilerexecname);

  std::string ClangIntrinsicPath;

  if (isClang()) {
    std::string tmp;

    fargs.push_back("-target");
    fargs.push_back(getTriple());

    const char *linkerVersion = getLinkerVersion();

    // -mlinker-version= is only relevant for ld64.
    if (!buildFlavor.IsLLVM() && linkerVersion && linkerVersion[0]) {
      tmp = "-mlinker-version=";
      tmp += linkerVersion;
      fargs.push_back(tmp);
      tmp.clear();
    }

#ifndef __APPLE__
    if (!findClangIntrinsicHeaders(ClangIntrinsicPath)) {
      warn << "cannot find clang intrinsic headers; please report this "
              "issue to the OSXCross project" << warn.endl();
    } else {
      if (haveArch(Arch::x86_64h) && clangversion < ClangVersion(3, 5)) {
        err << "'" << getArchName(Arch::x86_64h) << "' requires clang 3.5 "
            << "(or later)" << err.endl();
        return false;
      }
    }

    tmp.clear();
#endif

    fargs.push_back("-isysroot");
    fargs.push_back(SDKPath);

    if (isCXX()) {
      tmp = "-stdlib=";
      tmp += getStdLibString(stdlib);
      fargs.push_back(tmp);

      if (stdlib == StdLib::libcxx ||
          (stdlib == StdLib::libstdcxx && usegcclibs)) {
        fargs.push_back("-nostdinc++");
        fargs.push_back("-Qunused-arguments");
      }

      if (stdlib == StdLib::libstdcxx && usegcclibs) {
        // Match the TLS ABI of the libstdc++ built by build_gcc.sh.
        // User arguments follow fargs, so -fno-emulated-tls can override this.
        fargs.push_back("-femulated-tls");
      }

      if (stdlib == StdLib::libstdcxx && usegcclibs && targetarchs.size() < 2 &&
          !isGCH()) {
        // Use libs from './build_gcc' installation
        setupGCCLibs(targetarchs[0]);
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
               !getenv("OSXCROSS_GCC_NO_STATIC_RUNTIME")) {
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

  for (auto &path : AdditionalCXXHeaderPaths)
    addCXXHeaderPath(path);

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

  if (isClang() && !ClangIntrinsicPath.empty()) {
    fargs.push_back("-isystem");
    fargs.push_back(ClangIntrinsicPath);
  }

  if (OSNum.Num()) {
    std::string tmp;
    tmp = "-mmacosx-version-min=";
    if (isClang() && clangversion < ClangVersion(11, 0) &&
        OSNum >= OSVersion(11, 0)) {
      // Clang <= 10 can't parse -mmacosx-version-min=11.x
      warn << "your clang installation is outdated and can't parse '-mmacosx-version-min=" << OSNum.shortStr() << "'. "
           << "setting it to 10.16."  << warn.endl();
      tmp += "10.16";
    } else {
      tmp += OSNum.Str();
    }
    fargs.push_back(tmp);
  }

  for (auto arch : targetarchs) {
    bool is32bit = false;
    bool isArm = false;

    switch (arch) {
    case Arch::i386:
    case Arch::i486:
    case Arch::i586:
    case Arch::i686:
      is32bit = true;
      // falls through
    case Arch::arm64:
    case Arch::arm64e:
    case Arch::x86_64:
    case Arch::x86_64h:
      isArm = arch == Arch::arm64 || arch == Arch::arm64e;
      if (isGCC()) {
        if (arch != Arch::x86_64 && arch != Arch::i386 &&
            arch != Arch::arm64) {
          err << "gcc does not support architecture '" << getArchName(arch)
              << "'" << err.endl();
          return false;
        }

        if (targetarchs.size() > 1)
          break;

        if (!isArm)
          fargs.push_back(is32bit ? "-m32" : "-m64");
      } else if (isClang()) {
        if (usegcclibs && targetarchs.size() > 1)
          break;
        fargs.push_back("-arch");
        fargs.push_back(getArchName(arch));
      }
      break;
    default:
      err << "unsupported architecture: '" << getArchName(arch) << "'"
          << err.endl();
      return false;
    }
  }

#ifdef __ANDROID__
  // Workaround for Termux
  std::string LDSysRoot = "-Wl,-syslibroot,";
  LDSysRoot += SDKPath;
  fargs.push_back(LDSysRoot);
#endif

  if (isClang()) {
    if (buildFlavor.IsLLVM() &&
        std::find(args.begin(), args.end(), "-c") == args.end() &&
        std::find(args.begin(), args.end(), "-E") == args.end() &&
        std::find(args.begin(), args.end(), "-S") == args.end()) {
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

      while (fargs.size() > 1)
        fargs.erase(fargs.end() - 1);
    }
  }

  bool isgcclibstdcxx =
      (isGCC() || (isClang() && usegcclibs && stdlib == StdLib::libstdcxx));

  if (OSNum <= OSVersion(10, 5)) {
    bool error = false;
    bool nowarning = false;

    if (isgcclibstdcxx) {
      err << "building for macOS '<= 10.5' with GCC (or clang++-gstdc++) "
             "is no longer supported" << err.endl();
      error = true;
    } else if (isClang()) {
      nowarning = !!getenv("OSXCROSS_NO_10_5_DEPRECATION_WARNING");
      if (!nowarning)
        warn << "building for macOS '<= 10.5' "
                "is no longer supported"  << warn.endl();
    }

    if (!nowarning)
      info << "use 'osxcross-1.1' branch instead" << info.endl();

    if (error)
      return false;
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
