/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2025-2026 by Thomas Poechtrager                      *
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

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include "tools/tools.h"
#include "programs/programs.h"

namespace program {

using tools::dbg;
using tools::err;

// Replace the current process with path; args[0] becomes its argv[0].
int execProgram(const std::string &path,
                const std::vector<std::string> &args) {
  std::vector<char *> argv;
  argv.reserve(args.size() + 1);

  for (const std::string &arg : args)
    argv.push_back(const_cast<char *>(arg.c_str()));

  argv.push_back(nullptr);
  execvp(path.c_str(), argv.data());

  const int error = errno;
  err << "cannot execute '" << path << "': " << strerror(error) << err.endl();
  return 1;
}

// Like above, but pass on argv[1..] and use path as argv[0].
int execProgram(const std::string &path, int argc, char **argv) {
  std::vector<std::string> args(1, path);

  if (argc > 1)
    args.insert(args.end(), argv + 1, argv + argc);

  return execProgram(path, args);
}

// Debug output: the original command line and the one passed to
// execProgram().
void printExecProgramArgs(int argc, char **argv,
                          const std::vector<std::string> &args) {
  std::string in;
  std::string out;

  for (int i = 0; i < argc; ++i) {
    in += argv[i];
    in += " ";
  }

  for (const std::string &arg : args) {
    out += arg;
    out += " ";
  }

  dbg << "--> " << in << dbg.endl();
  dbg << "<-- " << out << dbg.endl();
}

namespace {

int dummy(int, char **, Target &) { return 0; }

constexpr Prog Programs[] = {
  // Built-in tools
  { "sw_vers",          macos::sw_vers },
  { "xcrun",            xcode::xcrun },
  { "xcodebuild",       xcode::xcodebuild },

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
  { "osxcross-man",     host::man },
  { "pkg-config",       host::pkg_config },

  // Dummy tool. No-op.
  { "wrapper",          dummy }
};

} // unnamed namespace

const Prog *getProg(const std::string &name) {
  for (const Prog &p : Programs) {
    if (name == p.name)
      return &p;
  }

  return nullptr;
}

//
// ArgumentParser
//

int ArgumentParser::execute(int argc, char **argv, Target &target) const {
  Handler action = nullptr; // the last action given
  char **actionArgs = nullptr;

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    const bool isOption = arg[0] == '-';
    const Option *option = isOption ? findOption(arg) : &positional;

    if (!option || !option->handler) {
      err << program << ": unknown argument: '" << arg << "'" << err.endl();
      return usageError;
    }

    // A positional argument is its own value; the values of an option
    // follow it.
    char **args = isOption ? &argv[i + 1] : &argv[i];
    const int numArgs = isOption ? argc - i - 1 : argc - i;
    bool lastArg = false;

    switch (option->value) {
    case Value::NONE:
      break;
    case Value::ONE:
      if (!numArgs) {
        err << program << ": too few arguments for '" << arg << "'"
            << err.endl();
        return usageError;
      }

      if (isOption)
        ++i;
      break;
    case Value::OPTIONAL:
      if (!numArgs)
        args = nullptr;
      else if (isOption)
        ++i;
      break;
    case Value::REST:
      if (!numArgs) {
        err << program << ": too few arguments for '" << arg << "'"
            << err.endl();
        return usageError;
      }

      lastArg = true;
      break;
    }

    switch (option->kind) {
    case Kind::SETTING:
      if (int status = option->handler(target, args))
        return status;
      break;
    case Kind::ACTION:
      action = option->handler;
      actionArgs = args;
      break;
    case Kind::IMMEDIATE_ACTION:
      return option->handler(target, args);
    }

    if (lastArg)
      break;
  }

  if (!action)
    return defaultAction ? defaultAction(target, nullptr) : 0;

  if (prepare) {
    if (int status = prepare(target, nullptr))
      return status;
  }

  return action(target, actionArgs);
}

// The option for arg, such as "-sdk"; nullptr if there is none.
const ArgumentParser::Option *
ArgumentParser::findOption(const char *arg) const {
  const char *name = arg + 1;

  if (singleDash) {
    if (*name == '-')
      return nullptr;
  } else {
    while (*name == '-')
      ++name;
  }

  for (const Option &option : options) {
    if (!strcmp(name, option.name))
      return &option;
  }

  return nullptr;
}

} // namespace program
