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

typedef std::vector<std::string> string_vector;

inline bool endsWith(const std::string &str, const std::string &end) {
  return str.size() >= end.size() &&
         str.compare(str.size() - end.size(), end.size(), end) == 0;
}

size_t constexpr constexprStrLen(const char *str) {
  return *str ? 1 + constexprStrLen(str + 1) : 0;
}

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
  bool printprefix;
public:
  static constexpr char endl() { return '\n'; }
  bool isendl(char c) { return c == '\n'; }
  template<typename T>
  bool isendl(T&&) { return false; }
  template<typename T>
  Message &operator<<(T &&v) {
    if (printprefix) {
      os << Color(FG_DARK_GRAY) << "osxcross: " << color << msg << ": "
         << Color(FG_DEFAULT);
      printprefix = false;
    }
    if (isendl(v)) {
      printprefix = true;
      os << std::endl;
    } else {
      os << v;
    }
    return *this;
  }
  Message(const char *msg, Color color = FG_RED, std::ostream &os = std::cerr)
      : msg(msg), color(color), os(os), printprefix(true) {}
} warn("warning"), err("error"), dbg("debug", FG_LIGHT_MAGENTA),
  info("info", FG_LIGHT_MAGENTA), warninfo("info", FG_LIGHT_MAGENTA);

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

constexpr char PATHDIV = '/';

// Reads the whole file into content; false if it cannot be opened.
bool getFileContent(const std::string &file, std::string &content);

bool fileExists(const std::string &file);
bool dirExists(const std::string &dir);
// Names of all entries of a directory, including "." and "..", in the
// order of readdir(); empty if the directory cannot be opened.
std::vector<std::string> listFiles(const std::string &dir);

// First match for <file> in the PATH directories, resolved with realpath()
// unless resolve is false. If accept() rejects a match, the search goes on.
// Returns an empty string if nothing is found.
std::string findExecutableInPath(const std::string &file, bool resolve = true,
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

// Runs args[0] (looked up in PATH unless it contains a '/') with args, without
// a shell, and stores its standard output in output. stdin and stderr are
// redirected to /dev/null. Returns true if the program exited with status 0.
bool getProgramOutput(const std::vector<std::string> &args,
                      std::string &output);

//
// Argument Parsing
//

// Parser for compiler options such as -I, -arch, and -stdlib.
template <typename T, size_t size = 0> struct OptParser {
  // Defines both how an option receives its value and which spellings match.
  enum class ValueMode {
    none,             // Exact flag without a value, e.g. "-m64".
    separate,         // Exact option followed by its value, e.g. "-arch arm64".
    joinedOrSeparate, // Joined or separate value, e.g. "-Ifoo" or "-I foo".
    joinedWithEquals  // Joined using '=', e.g. "-stdlib=libc++".
  };

  // Tells the caller whether matched argument tokens must be forwarded.
  enum class Forwarding {
    discard, // Consume the option in the wrapper.
    keep     // Keep the option and its value for the wrapped program.
  };

  struct Option {
    const char *name;
    const size_t namelen;
    const T fun;
    const ValueMode valueMode;
    const Forwarding forwarding;

    constexpr Option(const char *name, T fun,
                     const ValueMode valueMode = ValueMode::none,
                     const Forwarding forwarding = Forwarding::discard)
        : name(name), namelen(constexprStrLen(name)), fun(fun),
          valueMode(valueMode), forwarding(forwarding) {}

    bool matches(const char *arg) const {
      if (strncmp(arg, name, namelen))
        return false;

      const char *suffix = arg + namelen;

      switch (valueMode) {
      case ValueMode::none:
      case ValueMode::separate:
        return !*suffix;
      case ValueMode::joinedOrSeparate:
        return true;
      case ValueMode::joinedWithEquals:
        // Match the bare option too, allowing the caller to diagnose a
        // missing "=<value>" instead of treating the option as unknown.
        return !*suffix || *suffix == '=';
      }

      return false;
    }
  };

  const Option options[size];

  // Returns the first option whose spelling accepts arg. Table order therefore
  // determines precedence if option spellings overlap.
  const Option *parse(const char *arg) const {
    for (const Option &option : options) {
      if (option.matches(arg))
        return &option;
    }

    return nullptr;
  }

  const bool debug;

  // Prints the resolved option, its optional value, and its forwarding policy
  // when debugging was enabled while constructing the parser.
  void printDebug(const Option &option, const char *arg,
                  const char *value) const {
    if (!debug)
      return;

    dbg << "parsed option '" << option.name << "'";

    if (value)
      dbg << " with value '" << value << "'";

    dbg << " from argument '" << arg << "' ("
        << (option.forwarding == Forwarding::keep ? "forwarded" : "consumed")
        << ")" << dbg.endl();
  }
};

template <typename T, size_t size = 0> struct ArgParser {
  const struct Bind {
    const char *name;
    T fun;
    int numArgs;
  } binds[size];

  const Bind *parseArg(int argc, char **argv, int numArg = 1) const {
    const char *arg = argv[numArg];

    if (*arg != '-')
      return nullptr;

    while (*arg == '-')
      ++arg;

    for (const Bind &bind : binds) {
      if (!strcmp(arg, bind.name)) {
        if (argc - numArg <= bind.numArgs) {
          err << "too few arguments for '-" << bind.name << "'" << err.endl();
          return nullptr;
        }

        return &bind;
      }
    }

    return nullptr;
  }
};

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

  constexpr int Num() const {
    return major * 10000 + minor * 100 + patch;
  }

  constexpr bool operator>(const OSVersion &OSNum) const {
    return Num() > OSNum.Num();
  }

  constexpr bool operator>=(const OSVersion &OSNum) const {
    return Num() >= OSNum.Num();
  }

  constexpr bool operator<(const OSVersion &OSNum) const {
    return Num() < OSNum.Num();
  }

  constexpr bool operator<=(const OSVersion &OSNum) const {
    return Num() <= OSNum.Num();
  }

  constexpr bool operator==(const OSVersion &OSNum) const {
    return Num() == OSNum.Num();
  }

  constexpr bool operator!=(const OSVersion &OSNum) const {
    return Num() != OSNum.Num();
  }

  // True if str is this version written with as many parts as str has,
  // e.g. "10", "10.9" or "10.9.0" for 10.9.0, but not "10.09".
  bool matchesString(const char *str) const;

  std::string Str() const { return shortStr() + "." + std::to_string(patch); }

  std::string shortStr() const {
    return std::to_string(major) + "." + std::to_string(minor);
  }

  std::string majorStr() const { return std::to_string(major); }

  std::string numStr() const { return std::to_string(Num()); }

  int major;
  int minor;
  int patch;
};

static_assert(OSVersion(10, 6) != OSVersion(10, 5), "");

OSVersion parseOSVersion(const char *OSVer);

typedef OSVersion GCCVersion;
inline GCCVersion parseGCCVersion(const char *ver) { return parseOSVersion(ver); }

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
  UNKNOWN // Upper-case to avoid clash with "enum Arch"
};

inline Compiler getCompilerIdentifier(const char *compilername) {
  if (!strncmp(compilername, "clang++", 7))
    return Compiler::CLANGXX;
  if (!strncmp(compilername, "clang", 5))
    return Compiler::CLANG;
  if (!strncmp(compilername, "g++", 3))
    return Compiler::GXX;
  if (!strncmp(compilername, "gcc", 3))
    return Compiler::GCC;
  return Compiler::UNKNOWN;
}

//
// Arch
//

enum Arch {
  arm64,
  aarch64,
  arm64e,
  i386,
  i486,
  i586,
  i686,
  x86_64,
  x86_64h, // Haswell
  unknown
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
    return Arch::arm64;

  size_t i = 0;
  for (auto archname : ArchNames) {
    if (!strcmp(arch, archname)) {
      return static_cast<Arch>(i);
    }
    ++i;
  }

  return Arch::unknown;
}

//
// Standard Library
//

enum StdLib {
  unset,
  libcxx,
  libstdcxx
};

constexpr const char *StdLibNames[] = {
  "default",
  "libc++",
  "libstdc++"
};

constexpr const char *getStdLibString(StdLib stdlib) {
  return StdLibNames[stdlib];
}

} // namespace tools

#endif // OSXCROSS_WRAPPER_TOOLS_H
