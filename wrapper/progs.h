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

#ifndef OSXCROSS_WRAPPER_PROGS_H
#define OSXCROSS_WRAPPER_PROGS_H

#include <cstdlib>
#include <string>
#include <vector>

namespace target {
struct Target;
}

namespace program {

using target::Target;

// Execute path (looked up in PATH if it contains no slash) with args;
// args[0] is the name the program sees as argv[0]. Only returns on failure,
// after printing an error message, and then returns 1.
int execProgram(const std::string &path,
                const std::vector<std::string> &args);

// Same, with argv[1] to argv[argc - 1] as arguments; the program sees path
// as argv[0].
int execProgram(const std::string &path, int argc, char **argv);

// Debug output: the original command line (argv) and the one passed to
// execProgram() (args).
void printExecProgramArgs(int argc, char **argv,
                          const std::vector<std::string> &args);

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

    exit(execProgram(tool, argc, argv));
  }

  const char *name;

private:
  Function fun;
  const char *tool;
};

int sw_vers(int argc, char **argv, Target &target);
int xcrun(int argc, char **argv, Target &target);
int xcodebuild(int argc, char **argv, Target &target);

int applySDKRootFromEnvironment();

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
int man(int argc, char **argv, Target &target);
int pkg_config(int argc, char **argv, Target &target);
} // namespace osxcross

// The program with this name, for example "xcrun" or "otool"; nullptr if
// there is none.
const Prog *getProg(const std::string &name);

} // namespace program

#endif // OSXCROSS_WRAPPER_PROGS_H
