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

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <strings.h>
#include <cstdlib>
#include <climits>
#include <cassert>
#include <sys/stat.h>
#include <unistd.h>

#include "tools.h"
#include "target.h"

namespace target {

Target::Target()
    : vendor(getDefaultVendor()), SDK(getenv("OSXCROSS_SDKROOT")),
      arch(Arch::x86_64), target(getDefaultTarget()), stdlib(StdLib::unset),
      usegcclibs(), wliblto(-1), compiler(getDefaultCompilerIdentifier()),
      compilername(getDefaultCompilerName()), language() {
  if (!getExecutablePath(execpath, sizeof(execpath)))
    abort();

  const char *SDKSearchDir = getSDKSearchDir();

  if (!SDK && SDKSearchDir[0])
    overrideDefaultSDKPath(SDKSearchDir);
}

OSVersion Target::getSDKOSNum() const {
  if (SDK) {
    std::string SDKPath = SDK;

    while (SDKPath.size() && SDKPath[SDKPath.size() - 1] == PATHDIV)
      SDKPath.erase(SDKPath.size() - 1, 1);

    const char *SDKName = getFileName(SDKPath);

    if (strncasecmp(SDKName, "MacOSX", 6))
      return OSVersion();

    return parseOSVersion(SDKName + 6);
  } else {
    if (target.size() < 7)
      return OSVersion();

    double n = atof(target.c_str() + 6);

    if (n >= 20.0f) {
      int major = 11 + ((int)n % 20);
      int minor = (((n - (int)n) * 10.0) - 1.0) + 0.1;
      return OSVersion(major, minor);
    } else {
      return OSVersion(10, (int)n - 4);
    }
  }
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
    } else {
      err << "'" << defaultSDKPath << "' broken symlink" << err.endl();
      exit(EXIT_FAILURE);
    }
  } else {
    // Choose the latest SDK

    static OSVersion latestSDKVersion;
    static std::string latestSDK;

    latestSDKVersion = OSVersion();
    latestSDK.clear();

    listFiles(SDKSearchDir, nullptr, [](const char *SDK) {
      if (!strncasecmp(SDK, "MacOSX", 6)) {
        OSVersion SDKVersion = parseOSVersion(SDK + 6);
        if (SDKVersion > latestSDKVersion) {
          latestSDKVersion = SDKVersion;
          latestSDK = SDK;
        }
      }
      return false;
    });

    if (!latestSDKVersion.Num()) {
      err << "no SDK found in '" << SDKSearchDir << "'" << err.endl();
      exit(EXIT_FAILURE);
    }

    std::string SDKPath;

    SDKPath = SDKSearchDir;
    SDKPath += PATHDIV;
    SDKPath += latestSDK;

    SDK = strdup(SDKPath.c_str()); // intentionally leaked
  }
}

bool Target::getSDKPath(std::string &path, bool MacOSX10_16Fix, bool majorVersionOnly) const {
  OSVersion SDKVer = getSDKOSNum();

  if (SDK) {
    path = SDK;
  } else {
    if (MacOSX10_16Fix)
      SDKVer = OSVersion(10, 16);
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
    // Some early 11.0 SDKs are misnamed as 10.16
    if (SDKVer == OSVersion(11, 0) && !MacOSX10_16Fix)
      return getSDKPath(path, true);

    if (SDKVer.minor == 0 && !majorVersionOnly)
      return getSDKPath(path, false, true);

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

void Target::addArch(const Arch arch) {
  auto &v = targetarch;
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
  for (auto a : targetarch) {
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


const std::string &Target::getDefaultTriple(std::string &triple) const {
  triple = getArchName(Arch::x86_64);
  triple += "-";
  triple += getDefaultVendor();
  triple += "-";
  triple += getDefaultTarget();
  return triple;
}

void Target::setCompilerPath() {
  if (isGCC()) {
    compilerpath = execpath;
    compilerpath += "/";
    compilerpath += getTriple();
    compilerpath += "-";
    compilerpath += "base-";
    compilerpath += compilername;

    compilerexecname = getTriple();
    compilerexecname += "-";
    compilerexecname += compilername;
  } else {
    if (!compilerpath.empty()) {
      compilerpath += "/";
      compilerpath += compilername;
    } else {
      if (!realPath(compilername.c_str(), compilerpath, ignoreCCACHE))
        compilerpath = compilername;

      compilerexecname += compilername;
    }
  }
}

bool Target::findClangIntrinsicHeaders(std::string &path) {
  static std::stringstream dir;

  assert(isClang());

  if (compilerpath.empty())
    return false;

  std::string clangbindir = compilerpath;
  stripFileName(clangbindir);

  static ClangVersion *clangversion;
  static std::string pathtmp;

  clangversion = &this->clangversion;

  clear(dir);
  *clangversion = ClangVersion();
  pathtmp.clear();

  auto tryDir = [&]()->bool {
    listFiles(dir.str().c_str(), nullptr, [](const char *file) {
      if (file[0] != '.' && isDirectory(file, dir.str().c_str())) {
        ClangVersion cv = parseClangVersion(file);

        if (cv != ClangVersion()) {
          static std::stringstream tmp;
          clear(tmp);

          auto checkDir = [&](std::stringstream &dir) {
            static std::string intrindir;
            auto &file = dir;

            intrindir = dir.str();
            file << "/xmmintrin.h";

            if (fileExists(file.str())) {
              if (cv > *clangversion) {
                *clangversion = cv;
                pathtmp.swap(intrindir);
              }
              return true;
            }

            return false;
          };

          tmp << dir.str() << "/" << file << "/include";

          if (!checkDir(tmp)) {
            clear(tmp);
            tmp << dir.str() << "/" << file;
            checkDir(tmp);
          }
        }
        return true;
      }
      return true;
    });
    return *clangversion != ClangVersion();
  };

#define TRYDIR(basedir, subdir)                                                \
do {                                                                           \
  dir << basedir << subdir;                                                    \
  if (tryDir()) {                                                              \
    path.swap(pathtmp);                                                        \
    return true;                                                               \
  }                                                                            \
  clear(dir);                                                                  \
} while (0)

#define TRYDIR2(libdir) TRYDIR(clangbindir, libdir)
#define TRYDIR3(libdir) TRYDIR(std::string(), libdir)

#ifdef __CYGWIN__
#ifdef __x86_64__
  TRYDIR2("/../lib/clang/x86_64-pc-cygwin");
#else
  TRYDIR2("/../lib/clang/i686-pc-cygwin");
#endif
#endif

  TRYDIR2("/../lib/clang");

#ifdef __linux__
#ifdef __x86_64__
  // opensuse uses lib64 instead of lib on x86_64
  TRYDIR2("/../lib64/clang");
#elif __i386__
  TRYDIR2("/../lib32/clang");
#endif
#endif

#ifdef __APPLE__
  constexpr const char *OSXIntrinDirs[] = {
    "/Library/Developer/CommandLineTools/usr/lib/clang",
    "/Applications/Contents/Developer/Toolchains/"
    "XcodeDefault.xctoolchain/usr/lib/clang"
  };

  for (auto intrindir : OSXIntrinDirs)
    TRYDIR3(intrindir);
#endif

  TRYDIR2("/../include/clang");
  TRYDIR2("/usr/include/clang");

  if (!intrinsicpath.empty()) {
    TRYDIR2(intrinsicpath);
  }

  return false;
#undef TRYDIR
#undef TRYDIR2
#undef TRYDIR3
}

void Target::setupGCCLibs(Arch arch) {
  assert(stdlib == StdLib::libstdcxx);
  fargs.push_back("-nodefaultlibs");

  std::string SDKPath;
  std::stringstream GCCLibSTDCXXPath;
  std::stringstream GCCLibPath;

  const bool dynamic = !!getenv("OSXCROSS_GCC_NO_STATIC_RUNTIME");

  getSDKPath(SDKPath);

  GCCLibPath << SDKPath << "/../../lib/gcc/" << otriple << "/"
             << gccversion.Str();

  GCCLibSTDCXXPath << SDKPath << "/../../" << otriple << "/lib";

  switch (arch) {
  case Arch::i386:
  case Arch::i486:
  case Arch::i586:
  case Arch::i686:
    GCCLibPath << "/" << getArchName(Arch::i386);
    GCCLibSTDCXXPath << "/" << getArchName(i386);
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
      static std::stringstream tmp;
      clear(tmp);
      tmp << path.str() << "/lib" << lib << ".a";
      fargs.push_back(tmp.str());
    }
  };

  fargs.push_back("-Qunused-arguments");

  addLib(GCCLibSTDCXXPath, "stdc++");
  addLib(GCCLibSTDCXXPath, "supc++");
  addLib(GCCLibPath, "gcc");
  addLib(GCCLibPath, "gcc_eh");

  fargs.push_back("-lc");
  fargs.push_back("-Wl,-no_compact_unwind");
}

bool Target::setup() {
  std::string SDKPath;
  OSVersion SDKOSNum = getSDKOSNum();

  if (!isKnownCompiler())
    warn << "unknown compiler '" << compilername << "'" << warn.endl();

  if (!getSDKPath(SDKPath))
    return false;

  if (targetarch.empty())
    targetarch.push_back(arch);

  triple = getArchName(arch);
  triple += "-";
  triple += vendor;
  triple += "-";
  triple += target;

  otriple = getArchName(Arch::x86_64);
  otriple += "-";
  otriple += vendor;
  otriple += "-";
  otriple += target;

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

    if (!OSNum.Num())
      OSNum = defaultMinTarget;
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

    if (haveArch(Arch::i386)) {
        err << "macOS SDK '>= 10.14' does not support i386 anymore"
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
      CXXHeaderPath += otriple;
      CXXHeaderPath += "/include/c++";

      static std::vector<GCCVersion> v;
      v.clear();

      listFiles(CXXHeaderPath.c_str(), nullptr, [](const char *path) {
        if (path[0] != '.')
          v.push_back(parseGCCVersion(path));
        return false;
      });

      if (v.empty()) {
        err << "'-foc-use-gcc-libstdc++' requires gcc to be installed "
               "(./build_gcc.sh)" << err.endl();
        return false;
      }

      std::sort(v.begin(), v.end());
      gccversion = v[v.size() - 1];

      CXXHeaderPath += "/";
      CXXHeaderPath += gccversion.Str();

      addCXXPath(otriple);
    } else {
      // Use SDK libs
      std::string tmp;

      if (SDKOSNum <= OSVersion(10, 5))
        CXXHeaderPath += "/usr/include/c++/4.0.0";
      else
        CXXHeaderPath += "/usr/include/c++/4.2.1";

      tmp = getArchName(arch);
      tmp += "-apple-";
      tmp += target;
      addCXXPath(tmp);
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

    tmp = "-mlinker-version=";
    tmp += getLinkerVersion();

    fargs.push_back(tmp);
    tmp.clear();

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

      if (stdlib == StdLib::libstdcxx && usegcclibs && targetarch.size() < 2 &&
          !isGCH()) {
        // Use libs from './build_gcc' installation
        setupGCCLibs(targetarch[0]);
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

    if (!isGCH())
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
    if (clangversion < ClangVersion(11, 0) &&
        OSNum >= OSVersion(11, 0)) {
      // Clang <= 10 can't parse -mmacosx-version-min=11.x
      tmp += "10.16";
    } else {
      tmp += OSNum.Str();
    }
    fargs.push_back(tmp);
  }

  for (auto arch : targetarch) {
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
      isArm = true;
      // falls through
    case Arch::arm64e:
      isArm = true;
      // falls through
    case Arch::x86_64:
    case Arch::x86_64h:
      if (isGCC()) {
        if (arch != Arch::x86_64 && arch != Arch::i386) {
          err << "gcc does not support architecture '" << getArchName(arch)
              << "'" << err.endl();
          return false;
        }

        if (targetarch.size() > 1)
          break;

        if (!isArm)
          fargs.push_back(is32bit ? "-m32" : "-m64");
      } else if (isClang()) {
        if (usegcclibs && targetarch.size() > 1)
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
      if (wliblto == -1)
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

  // Silence 'operator new[]' warning in ld64
  if (isgcclibstdcxx)
    setenv("OSXCROSS_GCC_LIBSTDCXX", "1", 1);

  return true;
}
} // namespace target
