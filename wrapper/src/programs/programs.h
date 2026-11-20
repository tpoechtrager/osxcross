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

#ifndef OSXCROSS_WRAPPER_PROGRAMS_PROGRAMS_H
#define OSXCROSS_WRAPPER_PROGRAMS_PROGRAMS_H

#include <cstdlib>
#include <string>

#include "tools/process.h"

namespace target {
struct Target;
}

namespace program {

using target::Target;

// A program the wrapper provides: either a built-in function or an LLVM
// tool that is executed with the same arguments.
class Prog {
public:
  typedef int (*Function)(int argc, char **argv, Target &target);

  constexpr Prog(const char *name, Function fun)
      : name(name), fun(fun), tool(nullptr) {}

  constexpr Prog(const char *name, const char *tool)
      : name(name), fun(nullptr), tool(tool) {}

  __attribute__((noreturn))
  void operator()(int argc, char **argv, Target &target) const {
    if (fun)
      exit(fun(argc, argv, target));

    exit(tools::process::execProgram(tool, argc, argv));
  }

  const char *name;

private:
  Function fun;
  const char *tool;
};

namespace macos {
int sw_vers(int argc, char **argv, Target &target);
} // namespace macos

namespace xcode {
int xcrun(int argc, char **argv, Target &target);
int xcodebuild(int argc, char **argv, Target &target);
} // namespace xcode

namespace llvm {
int lipo(int argc, char **argv, Target &target);
int ld(int argc, char **argv, Target &target);

namespace clang {
int as(int argc, char **argv, Target &target);
}
} // namespace llvm

namespace osxcross {
int version(int argc, char **argv, Target &target);
int env(int argc, char **argv, Target &target);
int conf(int argc, char **argv, Target &target);
} // namespace osxcross

namespace host {
int man(int argc, char **argv, Target &target);
int pkg_config(int argc, char **argv, Target &target);
} // namespace host

// The program with this name, for example "xcrun" or "otool"; nullptr if
// there is none.
const Prog *getProg(const std::string &name);

} // namespace program

#endif // OSXCROSS_WRAPPER_PROGRAMS_PROGRAMS_H
