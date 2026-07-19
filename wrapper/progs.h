/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2020 by Thomas Poechtrager                      *
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
struct Target;
}

namespace program {

using target::Target;

int executeExternalTool(const char *toolName, int argc, char **argv);
void printExternalToolArgs(int argc, char **argv, std::vector<char *> &args);

class prog {
public:
  typedef int (*f1)();
  typedef int (*f2)(int, char **);
  typedef int (*f3)(int, char **, Target &);
  typedef int (*f4)(Target &);

  constexpr prog(const char *name, f1 fun)
      : name(name), fun1(fun), tool(nullptr), type(1) {}

  constexpr prog(const char *name, f2 fun)
      : name(name), fun2(fun), tool(nullptr), type(2) {}

  constexpr prog(const char *name, f3 fun)
      : name(name), fun3(fun), tool(nullptr), type(3) {}

  constexpr prog(const char *name, f4 fun)
      : name(name), fun4(fun), tool(nullptr), type(4) {}

  constexpr prog(const char *name, const char *tool)
      : name(name), fun1(nullptr), tool(tool), type(5) {}

  __attribute__((noreturn))
  void operator()(int argc, char **argv, Target &target) const {
    switch (type) {
    case 1:
      exit(fun1());
    case 2:
      exit(fun2(argc, argv));
    case 3:
      exit(fun3(argc, argv, target));
    case 4:
      exit(fun4(target));
    case 5:
      exit(executeExternalTool(tool, argc, argv));
    }

    __builtin_unreachable();
  }

  bool operator==(const char *name) const {
    return !strcmp(name, this->name);
  }

  template <class T>
  bool operator==(const T &name) const {
    return name == this->name;
  }

  const char *name;

private:
  union {
    f1 fun1;
    f2 fun2;
    f3 fun3;
    f4 fun4;
  };

  const char *tool;
  int type;
};

int sw_vers(int argc, char **argv, target::Target &target);
int xcrun(int argc, char **argv, Target &target);
int xcodebuild(int argc, char **argv, Target &target);

namespace llvm {
int lipo(int argc, char **argv, Target &target);
int ld(int argc, char **argv, Target &target);

namespace clang {
int as(int argc, char **argv, Target &target);
}
} // namespace llvm

namespace osxcross {
int version();
int env(int argc, char **argv);
int conf(Target &target);
int man(int argc, char **argv, Target &target);
int pkg_config(int argc, char **argv, Target &target);
} // namespace osxcross

static int dummy() { return 0; }

constexpr prog programs[] = {
  // Built-in tools
  { "sw_vers",          sw_vers },
  { "xcrun",            xcrun },
  { "xcodebuild",       xcodebuild },

  // LLVM tools

  // These are LLVM tools where we must modify the passed arguments to it.
  { "ld",               llvm::ld },
  { "lipo",             llvm::lipo },
  { "as",               llvm::clang::as },

  // Used 'as-is', besides making them believe
  // they are invoked as "llvm-<tool>" instead of "<tool>".
  { "otool",            "llvm-otool" },

  { "nm",               "llvm-nm" },
  { "ar",               "llvm-ar" },
  { "libtool",          "llvm-libtool-darwin" },
  { "install_name_tool","llvm-install-name-tool" },
  { "ranlib",           "llvm-ranlib" },
  { "readtapi",         "llvm-readtapi" },
  { "objdump",          "llvm-objdump" },
  { "strip",            "llvm-strip" },
  { "strings",          "llvm-strings" },
  { "size",             "llvm-size" },
  { "symbolizer",       "llvm-symbolizer" },
  { "cov",              "llvm-cov" },
  { "profdata",         "llvm-profdata" },
  { "readobj",          "llvm-readobj" },
  { "readelf",          "llvm-readelf" },
  { "dwarfdump",        "llvm-dwarfdump" },
  { "cxxfilt",          "llvm-cxxfilt" },
  { "objcopy",          "llvm-objcopy" },
  { "config",           "llvm-config" },
  { "dis",              "llvm-dis" },
  { "link",             "llvm-link" },
  { "lto",              "llvm-lto" },
  { "lto2",             "llvm-lto2" },
  { "bcanalyzer",       "llvm-bcanalyzer" },
  { "bitcode_strip",    "llvm-bitcode-strip" },

  // OSXCross tools
  { "osxcross",         osxcross::version },
  { "osxcross-env",     osxcross::env },
  { "osxcross-conf",    osxcross::conf },

  // Tools where we must modify the passed arguments to it.
  { "osxcross-man",     osxcross::man },
  { "pkg-config",       osxcross::pkg_config },

  // Dummy tool. No-op.
  { "wrapper",          dummy }
};

template <class T> const prog *getprog(const T &name) {
  for (auto &p : programs) {
    if (p == name)
      return &p;
  }
  return nullptr;
}

} // namespace program
