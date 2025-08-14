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

constexpr const char *getSupportedArchsString() { return OSXCROSS_SUPPORTED_ARCHS; }

static inline std::vector<Arch> getSupportedArchs() {
  std::vector<Arch> result;
  std::istringstream iss(OSXCROSS_SUPPORTED_ARCHS);
  std::string token;
  while (iss >> token) {
    result.push_back(parseArch(token.c_str()));
  }
  return result;
}

static inline Arch getDefaultArch() {
  std::istringstream iss(OSXCROSS_SUPPORTED_ARCHS);
  std::string first;
  iss >> first;
  return parseArch(first.c_str());
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

#ifdef OSXCROSS_OSX_VERSION_MIN
inline OSVersion getDefaultMinTarget() {
  if (!strcmp(OSXCROSS_OSX_VERSION_MIN, "default"))
    return OSVersion();

  return parseOSVersion(OSXCROSS_OSX_VERSION_MIN);
}
#else
constexpr OSVersion getDefaultMinTarget() { return OSVersion(); }
#endif

inline const char *getSDKSearchDir() {
  const char *SDKSearchDir = getenv("OSXCROSS_SDK_SEARCH_DIR");

#ifdef OSXCROSS_SDK_SEARCH_DIR
  if (!SDKSearchDir)
    SDKSearchDir = OSXCROSS_SDK_SEARCH_DIR;
#endif

  return SDKSearchDir ? SDKSearchDir : "";
}

//
// Target
//

struct Target {
  Target();

  OSVersion getSDKOSNum() const;
  void overrideDefaultSDKPath(const char *SDKSearchDir);
  bool getSDKPath(std::string &path, bool MacOSX10_16Fix = false, bool majorVersionOnly = false) const;

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

  const std::string &getDefaultTriple(std::string &triple) const;
  const std::string &getTriple() const { return triple; }

  void setCompilerPath();
  bool findClangIntrinsicHeaders(std::string &path);

  void setupGCCLibs(Arch arch);
  bool setup();

  const char *vendor;
  const char *SDK;
  std::vector<Arch> supportedarchs;
  Arch arch;
  std::vector<Arch> targetarchs;
  std::string target;
  OSVersion OSNum;
  StdLib stdlib;
  ClangVersion clangversion;
  GCCVersion gccversion;
  bool usegcclibs;
  int wliblto;
  Compiler compiler;
  std::string compilerpath;     // /usr/bin/clang | [...]/target/bin/*-gcc
  std::string compilername;     // clang | gcc
  std::string compilerexecname; // clang | *-apple-darwin-gcc
  std::string triple;
  std::string otriple;
  string_vector fargs;
  string_vector args;
  const char *language;
  char execpath[PATH_MAX + 1];
  std::string intrinsicpath;
};

} // namespace target
