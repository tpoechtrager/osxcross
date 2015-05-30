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

namespace target {

using namespace tools;

//
// Default values for the Target struct
//

constexpr const char *getDefaultVendor() { return "apple"; }
constexpr const char *getDefaultTarget() { return OSXCROSS_TARGET; }
constexpr const char *getDefaultCompiler() { return "clang"; }
constexpr const char *getDefaultCXXCompiler() { return "clang++"; }
constexpr const char *getLinkerVersion() { return OSXCROSS_LINKER_VERSION; }

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

inline const char *getDefaultCStandard() {
  return getenv("OSXCROSS_C_STANDARD");
}
inline const char *getDefaultCXXStandard() {
  return getenv("OSXCROSS_CXX_STANDARD");
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

//
// Target
//

struct Target {
  Target()
      : vendor(getDefaultVendor()), target(getDefaultTarget()),
        stdlib(StdLib::unset), usegcclibs(), nocodegen(),
        compilername(getDefaultCompiler()), lang(), langstd(), sourcefile(),
        outputname() {
    if (!getExecutablePath(execpath, sizeof(execpath)))
      abort();
  }

  OSVersion getSDKOSNum() const;
  bool getSDKPath(std::string &path) const;

  bool getMacPortsDir(std::string &path) const;
  bool getMacPortsSysRootDir(std::string &path) const;
  bool getMacPortsPkgConfigDir(std::string &path) const;
  bool getMacPortsIncludeDir(std::string &path) const;
  bool getMacPortsLibDir(std::string &path) const;
  bool getMacPortsFrameworksDir(std::string &path) const;

  void addArch(const Arch arch);
  bool haveArch(const Arch arch);

  bool hasLibCXX() const;
  bool libCXXIsDefaultCXXLib() const;
  bool isLibCXX() const;
  bool isLibSTDCXX() const;

  bool haveSourceFile() { return sourcefile != nullptr; }
  bool haveOutputName() { return outputname != nullptr; }

  bool isC(bool r = false);
  bool isCXX();
  bool isObjC(bool r = false);

  bool isGCH() {
    if (haveOutputName()) {
      const char *ext = getFileExtension(outputname);
      return !strcmp(ext, ".gch");
    }
    return false;
  }

  bool isClang() const {
    return !strncmp(getFileName(compilername.c_str()), "clang", 5);
  }

  bool isGCC() const {
    const char *c = getFileName(compilername.c_str());
    return (!strncmp(c, "gcc", 3) || !strncmp(c, "g++", 3));
  }

  bool isKnownCompiler() const { return isClang() || isGCC(); }

  bool langGiven() const { return lang != nullptr; }
  bool langStdGiven() const { return langstd != nullptr; }

  const char *getLangName();
  bool isCXX11orNewer() const;

  const std::string &getTriple() const { return triple; }

  void setCompilerPath();
  bool findClangIntrinsicHeaders(std::string &path);

  void setupGCCLibs(Arch arch);
  bool setup();

  const char *vendor;
  Arch arch;
  std::vector<Arch> targetarch;
  std::string target;
  OSVersion OSNum;
  StdLib stdlib;
  ClangVersion clangversion;
  GCCVersion gccversion;
  bool usegcclibs;
  bool nocodegen;
  std::string compilerpath;     // /usr/bin/clang | [...]/target/bin/*-gcc
  std::string compilername;     // clang | gcc
  std::string compilerexecname; // clang | *-apple-darwin-gcc
  std::string triple;
  std::string otriple;
  const char *lang;
  const char *langstd;
  string_vector fargs;
  string_vector args;
  const char *sourcefile;
  const char *outputname;
  char execpath[PATH_MAX + 1];
};

} // namespace target
