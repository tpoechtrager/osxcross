/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2025 by Thomas Poechtrager                      *
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
  
  class prog {
  public:
    typedef int (*f1)();
    typedef int (*f2)(int, char **);
    typedef int (*f3)(int, char **, Target &);
    typedef int (*f4)(Target &);
  
    constexpr prog(const char *name, f1 fun)
        : name(name), fun1(fun), fun2(), fun3(), fun4(), type(1) {}
  
    constexpr prog(const char *name, f2 fun)
        : name(name), fun1(), fun2(fun), fun3(), fun4(), type(2) {}
  
    constexpr prog(const char *name, f3 fun)
        : name(name), fun1(), fun2(), fun3(fun), fun4(), type(3) {}
  
    constexpr prog(const char *name, f4 fun)
        : name(name), fun1(), fun2(), fun3(), fun4(fun), type(4) {}
  
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
      }
      __builtin_unreachable();
    }
  
    bool operator==(const char *name) const { return !strcmp(name, this->name); }
  
    template<class T>
    bool operator==(const T &name) const { return name == this->name; }
  
    const char *name;
  
  private:
    f1 fun1;
    f2 fun2;
    f3 fun3;
    f4 fun4;
    int type;
  };
  
  int sw_vers(int argc, char **argv, target::Target &target);
  int xcrun(int argc, char **argv, Target &target);
  int xcodebuild(int argc, char **argv, Target &target);
  int dsymutil(int argc, char **argv, target::Target &target);
  int ld(int argc, char **argv, target::Target &target);
  
  namespace osxcross {
  int version();
  int env(int argc, char **argv);
  int conf(Target &target);
  int cmp(int argc, char **argv);
  int man(int argc, char **argv, Target &target);
  int pkg_config(int argc, char **argv, Target &target);
  } // namespace osxcross
  
  namespace llvm {
  int lipo(int argc, char **argv);
  int ld(int argc, char **argv, target::Target &target);
  int as(int argc, char **argv);
  int execute(const char *toolName, int argc, char **argv);
  
  template<const char *Name>
  int wrap(int argc, char **argv) {
    return execute(Name, argc, argv);
  }
  
  static constexpr char dsymutil[]       = "dsymutil";
  static constexpr char otool[]          = "llvm-otool";
  static constexpr char nm[]             = "llvm-nm";
  static constexpr char ar[]             = "llvm-ar";
  static constexpr char libtool[]        = "llvm-libtool-darwin";
  static constexpr char readtapi[]       = "llvm-readtapi";
  static constexpr char objdump[]        = "llvm-objdump";
  static constexpr char strip[]          = "llvm-strip";
  static constexpr char strings[]        = "llvm-strings";
  static constexpr char size[]           = "llvm-size";
  static constexpr char symbolizer[]     = "llvm-symbolizer";
  static constexpr char cov[]            = "llvm-cov";
  static constexpr char profdata[]       = "llvm-profdata";
  static constexpr char ranlib[]         = "llvm-ranlib";
  static constexpr char readobj[]        = "llvm-readobj";
  static constexpr char readelf[]        = "llvm-readelf";
  static constexpr char dwarfdump[]      = "llvm-dwarfdump";
  static constexpr char cxxfilt[]        = "llvm-cxxfilt";
  static constexpr char objcopy[]        = "llvm-objcopy";
  static constexpr char config[]         = "llvm-config";
  static constexpr char dis[]            = "llvm-dis";
  static constexpr char link[]           = "llvm-link";
  static constexpr char lto[]            = "llvm-lto";
  static constexpr char lto2[]           = "llvm-lto2";
  static constexpr char bcanalyzer[]     = "llvm-bcanalyzer";
  static constexpr char bitcode_strip[]  = "llvm-bitcode-strip";
  } // namespace llvm
  
  static int dummy() { return 0; }
  
  constexpr prog programs[] = {
    { "sw_vers", sw_vers },
    { "xcrun", xcrun },
    { "xcodebuild", xcodebuild },
  
    // LLVM/Xcode
    { "dsymutil",      llvm::wrap<llvm::dsymutil> },
    { "ld",            llvm::ld },
    { "otool",         llvm::wrap<llvm::otool> },
    { "lipo",          llvm::lipo },
    { "nm",            llvm::wrap<llvm::nm> },
    { "ar",            llvm::wrap<llvm::ar> },
    { "libtool",       llvm::wrap<llvm::libtool> },
    { "ranlib",        llvm::wrap<llvm::ranlib> },
    { "readtapi",      llvm::wrap<llvm::readtapi> },
    { "objdump",       llvm::wrap<llvm::objdump> },
    { "strip",         llvm::wrap<llvm::strip> },
    { "strings",       llvm::wrap<llvm::strings> },
    { "size",          llvm::wrap<llvm::size> },
    { "symbolizer",    llvm::wrap<llvm::symbolizer> },
    { "cov",           llvm::wrap<llvm::cov> },
    { "profdata",      llvm::wrap<llvm::profdata> },
    { "readobj",       llvm::wrap<llvm::readobj> },
    { "readelf",       llvm::wrap<llvm::readelf> },
    { "dwarfdump",     llvm::wrap<llvm::dwarfdump> },
    { "cxxfilt",       llvm::wrap<llvm::cxxfilt> },
    { "objcopy",       llvm::wrap<llvm::objcopy> },
    { "config",        llvm::wrap<llvm::config> },
    { "as",            llvm::as },
    { "dis",           llvm::wrap<llvm::dis> },
    { "link",          llvm::wrap<llvm::link> },
    { "lto",           llvm::wrap<llvm::lto> },
    { "lto2",          llvm::wrap<llvm::lto2> },
    { "bcanalyzer",    llvm::wrap<llvm::bcanalyzer> },
    { "bitcode-strip", llvm::wrap<llvm::bitcode_strip> },
  
    // osxcross tools
    { "osxcross",        osxcross::version },
    { "osxcross-env",    osxcross::env },
    { "osxcross-conf",   osxcross::conf },
    { "osxcross-cmp",    osxcross::cmp },
    { "osxcross-man",    osxcross::man },
    { "pkg-config",      osxcross::pkg_config },
  
    // wrapper/dummy
    { "wrapper", dummy }
  };
    
  template <class T> const prog *getprog(const T &name) {
    for (auto &p : programs) {
      if (p == name)
        return &p;
    }
    return nullptr;
  }
  
  } // namespace program
  