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

#ifndef OSXCROSS_WRAPPER_TOOLS_H
#define OSXCROSS_WRAPPER_TOOLS_H

#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace tools {

//
// Misc helper tools
//

typedef std::vector<std::string> StringVector;

inline bool endsWith(const std::string &str, const std::string &end) {
  return str.size() >= end.size() &&
         str.compare(str.size() - end.size(), end.size(), end) == 0;
}

size_t constexpr constexprStrLen(const char *str) {
  return *str ? 1 + constexprStrLen(str + 1) : 0;
}

// Appends the Unicode code point code to out, encoded as UTF-8.
void appendUTF8(std::string &out, unsigned code);

//
// Debugging
//

// The debug level set with OCDEBUG: 0 if it is unset, not a number or
// negative, otherwise its value (1 = debug messages, 2 = more of them).
int getDebugLevel();

//
// Terminal text colors
//

bool isTerminal();

// http://stackoverflow.com/a/17469726

enum ColorCode {
  FG_DEFAULT = 39,
  FG_BLACK = 30,
  FG_RED = 31,
  FG_GREEN = 32,
  FG_YELLOW = 33,
  FG_BLUE = 34,
  FG_MAGENTA = 35,
  FG_CYAN = 36,
  FG_LIGHT_GRAY = 37,
  FG_DARK_GRAY = 90,
  FG_LIGHT_RED = 91,
  FG_LIGHT_GREEN = 92,
  FG_LIGHT_YELLOW = 93,
  FG_LIGHT_BLUE = 94,
  FG_LIGHT_MAGENTA = 95,
  FG_LIGHT_CYAN = 96,
  FG_WHITE = 97,
  BG_RED = 41,
  BG_GREEN = 42,
  BG_BLUE = 44,
  BG_DEFAULT = 49
};

class Color {
  ColorCode cc;
public:
  Color(ColorCode cc) : cc(cc) {}
  friend std::ostream &
  operator<<(std::ostream &os, const Color &color) {
    if (isTerminal())
      return os << "\033[" << color.cc << "m";
    return os;
  }
};

//
// Error message helper
//

static class Message {
private:
  const char *msg;
  Color color;
  std::ostream &os;
  bool printPrefix;
public:
  static constexpr char endl() { return '\n'; }
  bool isEndl(char c) { return c == '\n'; }
  template<typename T>
  bool isEndl(T&&) { return false; }
  template<typename T>
  Message &operator<<(T &&v) {
    if (printPrefix) {
      os << Color(FG_DARK_GRAY) << "osxcross: " << color << msg << ": "
         << Color(FG_DEFAULT);
      printPrefix = false;
    }
    if (isEndl(v)) {
      printPrefix = true;
      os << std::endl;
    } else {
      os << v;
    }
    return *this;
  }
  Message(const char *msg, Color color = FG_RED, std::ostream &os = std::cerr)
      : msg(msg), color(color), os(os), printPrefix(true) {}
} warn("warning"), err("error"), dbg("debug", FG_LIGHT_MAGENTA),
  info("info", FG_LIGHT_MAGENTA), warnInfo("info", FG_LIGHT_MAGENTA),
  note("note", FG_LIGHT_MAGENTA);

//
// Executable path
//

// Directory that contains the running executable, without a trailing '/'.
// Returns an empty string on failure.
std::string getExecutableDir();
// Name of the parent process, for example "tcsh"; "unknown" if it cannot
// be determined.
std::string getParentProcessName();

//
// Environment
//

// Puts val in front of the ':'-separated list in the environment variable
// var ("val:<old value>"), or sets var to val if it is unset.
void prependEnvVariable(const char *var, const std::string &val);
// Backslash before the characters a shell or a ':'-separated list would
// interpret: " ' \ $ ( ) space ; :
std::string escapePath(const std::string &path);
// Elements of a ':'-separated list such as PATH; empty elements are skipped.
std::vector<std::string> splitPath(const std::string &path);
// Joins the elements with ':', each one escaped with escapePath().
std::string joinPath(const std::vector<std::string> &path);
bool hasPath(const std::vector<std::string> &path, const std::string &dir);

//
// Files and directories
//

constexpr char PathDiv = '/';

// Reads the whole file into content; false if it cannot be opened.
bool getFileContent(const std::string &file, std::string &content);
// Reads everything from fd into content; false on a read error, which can
// leave part of the data in content.
bool getFileContent(int fd, std::string &content);
// Writes all of content to fd; false on a write error, with errno set.
bool writeFileContent(int fd, const std::string &content);

// The directory for temporary files: TMPDIR if it is a writable directory,
// otherwise /tmp.
std::string getTempDir();

bool fileExists(const std::string &file);
bool dirExists(const std::string &dir);
// True if file is a regular file that the current user may execute. A symlink
// is judged by its target; a broken symlink is not executable.
bool isExecutableFile(const std::string &file);
// Names of all entries of a directory, including "." and "..", in the
// order of readdir(); empty if the directory cannot be opened.
std::vector<std::string> listFiles(const std::string &dir);

// Searches the PATH directories for an executable file (see
// isExecutableFile()) named <file> and returns its path, or an empty string
// if there is none. If resolve is true, the path is resolved with realpath(),
// so it has no symlinks. If accept() rejects a match, the search goes on.
std::string
findExecutableInPath(const std::string &file, bool resolve = true,
                     bool (*accept)(const std::string &path) = nullptr);
// accept() for findExecutableInPath(): rejects ccache, for example a
// /usr/lib/ccache/clang symlink that resolves to ccache.
bool ignoreCCACHE(const std::string &path);

void stripFileName(std::string &path);

const char *getFileName(const char *file);
inline const char *getFileName(const std::string &file) {
  return getFileName(file.c_str());
}

// Directory part of a file path: everything before the last '/', "/" for
// a file in the root directory and "." for a name without '/'.
std::string getDirName(const std::string &file);

//
// OSVersion
//

#undef major
#undef minor
#undef patch

struct OSVersion {
  constexpr OSVersion(int major, int minor, int patch = 0)
      : major(major), minor(minor), patch(patch) {}
  constexpr OSVersion() : major(), minor(), patch() {}

  constexpr int num() const {
    return major * 10000 + minor * 100 + patch;
  }

  constexpr bool operator>(const OSVersion &osNum) const {
    return num() > osNum.num();
  }

  constexpr bool operator>=(const OSVersion &osNum) const {
    return num() >= osNum.num();
  }

  constexpr bool operator<(const OSVersion &osNum) const {
    return num() < osNum.num();
  }

  constexpr bool operator<=(const OSVersion &osNum) const {
    return num() <= osNum.num();
  }

  constexpr bool operator==(const OSVersion &osNum) const {
    return num() == osNum.num();
  }

  constexpr bool operator!=(const OSVersion &osNum) const {
    return num() != osNum.num();
  }

  // True if versionStr is this version written with as many parts as
  // versionStr has, e.g. "10", "10.9" or "10.9.0" for 10.9.0, but not
  // "10.09".
  bool matchesString(const char *versionStr) const;

  std::string str() const { return shortStr() + "." + std::to_string(patch); }

  std::string shortStr() const {
    return std::to_string(major) + "." + std::to_string(minor);
  }

  std::string majorStr() const { return std::to_string(major); }

  // For messages: Show the patch level only if it is not 0 (12.0, 27.3.1).
  std::string displayStr() const { return patch ? str() : shortStr(); }

  std::string numStr() const { return std::to_string(num()); }

  int major;
  int minor;
  int patch;
};

static_assert(OSVersion(10, 6) != OSVersion(10, 5), "");

// The version at the start of osVer: 10.4 for "10.4u.sdk". With strict,
// osVer must be one to three numbers separated by dots and nothing else;
// otherwise the result is 0.0, for example for "10.9beta" or "10,14".
OSVersion parseOSVersion(const char *osVer, bool strict = false);

typedef OSVersion GCCVersion;
inline GCCVersion parseGCCVersion(const char *ver) {
  return parseOSVersion(ver);
}

typedef OSVersion ClangVersion;
inline ClangVersion parseClangVersion(const char *ver) {
  return parseOSVersion(ver);
}

typedef OSVersion LLVMVersion;
inline LLVMVersion parseLLVMVersion(const char *ver) {
  return parseOSVersion(ver);
}

//
// Compiler Identifier
//

#undef Compiler
#undef CLANG
#undef CLANGXX
#undef GCC
#undef GXX

enum Compiler : int {
  CLANG,
  CLANGXX,
  GCC,
  GXX,
  UNKNOWN_COMPILER
};

inline Compiler getCompilerIdentifier(const char *compilerName) {
  if (!strncmp(compilerName, "clang++", 7))
    return Compiler::CLANGXX;
  if (!strncmp(compilerName, "clang", 5))
    return Compiler::CLANG;
  if (!strncmp(compilerName, "g++", 3))
    return Compiler::GXX;
  if (!strncmp(compilerName, "gcc", 3))
    return Compiler::GCC;
  return Compiler::UNKNOWN_COMPILER;
}

inline bool isClang(const Compiler compiler) {
  return compiler == Compiler::CLANG || compiler == Compiler::CLANGXX;
}

inline bool isGCC(const Compiler compiler) {
  return compiler == Compiler::GCC || compiler == Compiler::GXX;
}

inline bool isKnownCompiler(const Compiler compiler) {
  return compiler != Compiler::UNKNOWN_COMPILER;
}

//
// Arch
//

enum Arch {
  ARM64,
  AARCH64,
  ARM64E,
  I386,
  I486,
  I586,
  I686,
  X86_64,
  X86_64H, // Haswell
  UNKNOWN_ARCH
};

constexpr const char *ArchNames[] = {
  "arm64",
  "aarch64",
  "arm64e",
  "i386",
  "i486",
  "i586",
  "i686",
  "x86_64",
  "x86_64h",
  "unknown"
};

constexpr const char *getArchName(Arch arch) {
  return ArchNames[arch];
}

inline Arch parseArch(const char *arch) {
  if (!strcmp(arch, "aarch64")) // treat aarch64 as arm64
    return Arch::ARM64;

  size_t i = 0;
  for (auto archName : ArchNames) {
    if (!strcmp(arch, archName)) {
      return static_cast<Arch>(i);
    }
    ++i;
  }

  return Arch::UNKNOWN_ARCH;
}

//
// Standard Library
//

enum Stdlib {
  UNSET,
  LIBCXX,
  LIBSTDCXX
};

constexpr const char *StdlibNames[] = {
  "default",
  "libc++",
  "libstdc++"
};

constexpr const char *getStdlibString(Stdlib stdlib) {
  return StdlibNames[stdlib];
}

} // namespace tools

#endif // OSXCROSS_WRAPPER_TOOLS_H
