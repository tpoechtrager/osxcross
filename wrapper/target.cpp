/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014, 2015 by Thomas Poechtrager                     *
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
#include <cstdlib>
#include <climits>
#include <cassert>

#include "tools.h"
#include "target.h"

namespace target {

OSVersion Target::getSDKOSNum() const {
  if (target.size() < 7)
    return OSVersion();

  int n = atoi(target.c_str() + 6);
  return OSVersion(10, 4 + (n - 8));
}

bool Target::getSDKPath(std::string &path) const {
  OSVersion SDKVer = getSDKOSNum();

  path = execpath;
  path += "/../SDK/MacOSX";
  path += SDKVer.shortStr();

  if (SDKVer <= OSVersion(10, 4))
    path += "u";

  path += ".sdk";
  return dirExists(path);
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

  if (!OSNum.Num())
    OSNum = getSDKOSNum();

  return stdlib != libstdcxx && hasLibCXX() && !isGCC() &&
         OSNum >= OSVersion(10, 9);
}

bool Target::isLibCXX() const {
  return stdlib == StdLib::libcxx || libCXXIsDefaultCXXLib();
}

bool Target::isLibSTDCXX() const { return stdlib == StdLib::libstdcxx; }

bool Target::isC(bool r) {
  if (!r && isCXX())
    return false;

  if (langGiven() && lang[0] != 'o' &&
      (!strcmp(lang, "c") || !strcmp(lang, "c-header")))
    return true;

  if (haveSourceFile()) {
    if (!strcmp(getFileExtension(sourcefile), ".c"))
      return true;
  }

  return compiler.find("++") == std::string::npos && !isObjC(true);
}

bool Target::isCXX() {
  bool CXXCompiler = compiler.find("++") != std::string::npos;

  if (!langGiven() && CXXCompiler && !isObjC(true))
    return true;

  if (langGiven() && !strncmp(lang, "c++", 3))
    return true;

  constexpr const char *CXXFileExts[] = { ".C",   ".cc", ".cpp", ".CPP",
                                          ".c++", ".cp", ".cxx" };

  if (haveSourceFile()) {
    const char *ext = getFileExtension(sourcefile);

    if (*ext) {
      for (auto &cxxfe : CXXFileExts) {
        if (!strcmp(ext, cxxfe))
          return true;
      }
    }
  }

  return CXXCompiler && !isC(true) && !isObjC(true);
}

bool Target::isObjC(bool r) {
  if (!r && isCXX())
    return false;

  if (langGiven() && lang[0] == 'o')
    return true;

  if (haveSourceFile()) {
    const char *ext = getFileExtension(sourcefile);

    if (!strcmp(ext, ".m") || !strcmp(ext, ".mm"))
      return true;
  }

  return false;
}

const char *Target::getLangName() {
  if (isC())
    return "C";
  else if (isCXX())
    return "C++";
  else if (isObjC())
    return "Obj-C";
  else
    return "unknown";
}

bool Target::isCXX11orNewer() const {
  if (!langStdGiven())
    return false;

  constexpr const char *STD[] = { "c++0x", "gnu++0x", "c++11", "gnu++11",
                                  "c++1y", "gnu++1y", "c++14", "gnu++14",
                                  "c++1z", "gnu++1z" };

  for (auto std : STD) {
    if (!strcmp(langstd, std))
      return true;
  }

  return false;
}

const std::string Target::getFullCompilerName() const {
  std::string compiler;

  if (isGCC()) {
    compiler = execpath;
    compiler += "/";
    compiler += getTriple();
    compiler += "-";
  }

  if (isGCC())
    compiler += "base-";

  compiler += this->compiler;
  return compiler;
}

bool Target::findClangIntrinsicHeaders(std::string &path) {
  std::string clangbin;
  static std::stringstream dir;

  assert(isClang());

  getPathOfCommand(compiler.c_str(), clangbin);

  if (clangbin.empty())
    return false;

  static ClangVersion *clangversion;
  static std::string pathtmp;

  clangversion = &this->clangversion;

  clear(dir);
  *clangversion = ClangVersion();
  pathtmp.clear();

  auto check = []()->bool {

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

  dir << clangbin << "/../lib/clang";

  if (!check()) {
    clear(dir);

#ifdef __APPLE__
    constexpr const char *OSXIntrinDirs[] = {
      "/Library/Developer/CommandLineTools/usr/lib/clang",
      "/Applications/Contents/Developer/Toolchains/"
      "XcodeDefault.xctoolchain/usr/lib/clang"
    };

    for (auto intrindir : OSXIntrinDirs) {
      dir << intrindir;
      if (check()) {
        break;
      }
      clear(dir);
    }
#endif

    if (!dir.rdbuf()->in_avail()) {
      dir << clangbin << "/../include/clang";

      if (!check())
        return false;
    }
  }

  path.swap(pathtmp);
  return *clangversion != ClangVersion();
}

void Target::setupGCCLibs(Arch arch) {
  assert(stdlib == StdLib::libstdcxx);
  fargs.push_back("-nodefaultlibs");

  std::string SDKPath;
  std::stringstream GCCLibSTDCXXPath;
  std::stringstream GCCLibPath;

  const bool dynamic = !!getenv("OSXCROSS_GCC_NO_STATIC_RUNTIME");

  getSDKPath(SDKPath);

  GCCLibPath << SDKPath << "/../../lib/gcc/"
             << otriple << "/" << gccversion.Str();

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

  if (!isKnownCompiler()) {
    std::cerr << "warning: unknown compiler '" << compiler << "'" << std::endl;
  }

  if (!getSDKPath(SDKPath)) {
    std::cerr << "cannot find Mac OS X SDK (expected in: " << SDKPath << ")"
              << std::endl;
    return false;
  }

  if (targetarch.empty())
    targetarch.push_back(arch);

  if (!langStdGiven()) {
    if (isC())
      langstd = getDefaultCStandard();
    else if (isCXX())
      langstd = getDefaultCXXStandard();
  }

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

  if (!OSNum.Num()) {
    if (haveArch(Arch::x86_64h)) {
      OSNum = OSVersion(10, 8); // Default to 10.8 for x86_64h
      if (SDKOSNum < OSNum) {
        std::cerr << getArchName(arch) << " requires the SDK from "
                  << OSNum.Str() << " (or later)" << std::endl;
        return false;
      }
    } else if (stdlib == StdLib::libcxx) {
      OSNum = OSVersion(10, 7); // Default to 10.7 for libc++
    } else {
      OSNum = getDefaultMinTarget();
    }
  }

  if (OSNum > SDKOSNum) {
    std::cerr << "targeted OS X Version must be <= " << SDKOSNum.Str()
              << " (SDK)" << std::endl;
    return false;
  } else if (OSNum < OSVersion(10, 4)) {
    std::cerr << "targeted OS X Version must be >= 10.4" << std::endl;
    return false;
  }

  if (haveArch(Arch::x86_64h) && OSNum < OSVersion(10, 8)) {
    std::cerr << getArchName(Arch::x86_64h) << " requires "
              << "'-mmacosx-version-min=10.8' (or later)" << std::endl;
    return false;
  }

  if (stdlib == StdLib::unset) {
    if (libCXXIsDefaultCXXLib()) {
      stdlib = StdLib::libcxx;
    } else {
      stdlib = StdLib::libstdcxx;
    }
  } else if (stdlib == StdLib::libcxx) {
    if (!hasLibCXX()) {
      std::cerr << "libc++ requires the SDK from 10.7 (or later)" << std::endl;
      return false;
    }

    if (OSNum.Num() && OSNum < OSVersion(10, 7)) {
      std::cerr << "libc++ requires '-mmacosx-version-min=10.7' (or later)"
                << std::endl;
      return false;
    }
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
      std::cerr << "cannot find " << getStdLibString(stdlib) << " headers"
                << std::endl;
      return false;
    }
    break;
  }
  case StdLib::libstdcxx: {
    if (isGCC() && /*isCXX11orNewer()*/ true)
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
        std::cerr << "'-oc-use-gcc-libs' requires gcc to be installed "
                     "(./build_gcc.sh)" << std::endl;
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

    if (!dirExists(CXXHeaderPath)) {
      std::cerr << "cannot find " << getStdLibString(stdlib) << " headers"
                << std::endl;
      return false;
    }

    break;
  }
  case StdLib::unset:
    abort();
  }

  fargs.push_back(getFullCompilerName());

  if (isClang()) {
    std::string tmp;

    fargs.push_back("-target");
    fargs.push_back(getTriple());

    tmp = "-mlinker-version=";
    tmp += getLinkerVersion();

    fargs.push_back(tmp);
    tmp.clear();

#ifndef __APPLE__
    if (!findClangIntrinsicHeaders(tmp)) {
      std::cerr << "cannot find clang intrinsic headers, please report this "
                   "issue to the OSXCross project" << std::endl;
    } else {
      fargs.push_back("-isystem");
      fargs.push_back(tmp);
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

      if (stdlib == StdLib::libstdcxx && usegcclibs && targetarch.size() < 2) {
        // Use libs from './build_gcc' installation
        setupGCCLibs(targetarch[0]);
      }

#ifndef __APPLE__
      // TODO: Need a way to distinguish between vanilla and Xcode clang
      // versions.

      if (clangversion >= ClangVersion(3, 7, 0) &&
          !getenv("OSXCROSS_NO_DEFINE_SIZED_DEALLOCATION")) {
        // Will run into linker errors otherwise with not so recent libc++
        // and libstdc++ versions.
        if (!usegcclibs || gccversion < GCCVersion(5, 0, 0)) {
          fargs.push_back("-Xclang");
          fargs.push_back("-fdefine-sized-deallocation");
        }
      }
#endif
    }
  } else if (isGCC()) {

    if (isLibCXX()) {
      if (!langStdGiven())
        langstd = "c++0x";
      else if (!isCXX11orNewer()) {
        std::cerr << "warning: libc++ requires -std=c++11 (or later) with gcc"
                  << std::endl;
      }
    }

    if (isCXX() && isLibCXX()) {
      fargs.push_back("-nostdinc++");
      fargs.push_back("-nodefaultlibs");

      if (!isGCH()) {
        fargs.push_back("-lc");
        fargs.push_back("-lc++");
        fargs.push_back(OSNum <= OSVersion(10, 4) ? "-lgcc_s.10.4"
                                                  : "-lgcc_s.10.5");
      }
    } else if (!isLibCXX() && !isGCH() &&
               !getenv("OSXCROSS_GCC_NO_STATIC_RUNTIME")) {
      fargs.push_back("-static-libgcc");
      fargs.push_back("-static-libstdc++");
    }

    fargs.push_back("-Wl,-no_compact_unwind");
  }

  auto addCXXHeaderPath = [&](const std::string &path) {
    fargs.push_back(isClang() ? "-cxx-isystem" : "-isystem");
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

  if (langGiven() && !usegcclibs) {
    // usegcclibs: delay it to later
    fargs.push_back("-x");
    fargs.push_back(lang);
  }

  if (langStdGiven()) {
    std::string tmp;
    tmp = "-std=";
    tmp += langstd;
    fargs.push_back(tmp);
  }

  if (OSNum.Num()) {
    std::string tmp;
    tmp = "-mmacosx-version-min=";
    tmp += OSNum.Str();
    fargs.push_back(tmp);
  }

  for (auto arch : targetarch) {
    bool is32bit = false;

    switch (arch) {
    case Arch::i386:
    case Arch::i486:
    case Arch::i586:
    case Arch::i686:
      is32bit = true;
    case Arch::x86_64:
    case Arch::x86_64h:
      if (isGCC()) {
        if (targetarch.size() > 1)
          break;

        fargs.push_back(is32bit ? "-m32" : "-m64");

        if (arch == Arch::x86_64h) {
          std::cerr << getArchName(arch) << " requires clang" << std::endl;
          return false;
          // fargs.push_back("-march=core-avx2");
          // fargs.push_back("-Wl,-arch,x86_64h");
        }
      } else if (isClang()) {
        if (usegcclibs && targetarch.size() > 1)
          break;
        fargs.push_back("-arch");
        fargs.push_back(getArchName(arch));
      }
      break;
    default:
      std::cerr << "unsupported architecture " << getArchName(arch) << ""
                << std::endl;
      return false;
    }
  }

  if (haveOutputName() &&
      (targetarch.size() <= 1 || (!isGCC() && !usegcclibs))) {
    fargs.push_back("-o");
    fargs.push_back(outputname);
  }

  return true;
}
} // namespace target
