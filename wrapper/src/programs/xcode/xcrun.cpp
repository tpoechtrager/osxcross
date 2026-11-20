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

#include "compat.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "tools/tools.h"
#include "tools/process.h"
#include "target.h"
#include "programs/programs.h"
#include "programs/argument-parser.h"
#include "programs/xcode/sdk.h"

using namespace tools;
using namespace target;

namespace program {
namespace xcode {
namespace {

// Like Apple's xcrun, errors in the options exit with 64.
constexpr int UsageError = 64;

bool isStandaloneXcrun = false;
bool showCommand = false;

// Like Apple's xcrun, the options are read first, up to the name of the
// program to run. Then only the last action runs, with the SDK of the last
// -sdk option.
const char *sdkOption = nullptr; // the last -sdk; nullptr: not given

bool isXcodeToolName(const char *tool) {
  constexpr const char *xcodeTools[] = {
    "ar", "as", "c89", "c99", "c11", "c++", "cc", "checksyms", "clang",
    "clang++", "dsymutil", "codesign_allocate", "dyldinfo", "gcc", "g++",
    "gcov", "gprof", "indr", "install_name_tool", "ld", "libtool", "lipo",
    "machocheck", "nm", "nmedit", "ObjectDump", "objdump", "otool",
    "pagestuff", "pkg-config", "ranlib", "redo_prebinding",
    "seg_addr_table", "seg_hack", "size", "strings", "strip", "sw_vers",
    "unwinddump", "xcodebuild", "xcrun", "bitcode_strip",
    "vtool", "makerelocs", "mtoc", "mtor"
  };

  for (const char *xcodeTool : xcodeTools)
    if (!strcmp(tool, xcodeTool))
      return true;

  return false;
}

bool getToolPath(Target &target, std::string &toolPath, std::string tool) {
  const char *fileName = getFileName(tool);
  bool isXcodeTool = isXcodeToolName(fileName);
  const bool useExplicitPath = tool[0] == PathDiv && !isXcodeTool;

  toolPath.clear();

  if (useExplicitPath)
    toolPath = tool;
  else if (tool[0] == PathDiv)
    tool = fileName; // map /usr/bin/clang (etc.) to <triple>-clang

  if (toolPath.empty()) {
    std::string triple;

    if (isStandaloneXcrun) {
      // Standalone xcrun uses the default target architecture.
      // For GCC tools, prefer the default GCC architecture so xcrun can find
      // GCC even when it is not available for the Clang default architecture.
      bool isGCC = (tool.rfind("gcc", 0) == 0 || tool.rfind("g++", 0) == 0);
      target.buildDefaultTriple(triple, isGCC);
    } else {
      // A target-prefixed xcrun must resolve tools for that exact target.
      // For example, arm64-apple-darwin27-xcrun -f gcc must fail when
      // no ARM64 GCC installation exists.
      target.setTriple();
      triple = target.getTriple();
    }

    toolPath = target.buildToolPath(triple, tool);
  }

  // Match Apple's xcrun: -find accepts an existing absolute path even if
  // it is not executable. Candidates found by tool name must be executable.
  const bool found = useExplicitPath ? fileExists(toolPath)
                                     : isExecutableFile(toolPath);
  if (!found) {
    // Fall back to system executables so 'xcrun git status' etc. works.
    // Do not resolve symlinks, so that 'xcrun -f sh' prints /usr/bin/sh
    // and not /usr/bin/bash.
    if (!isXcodeTool) {
      toolPath = findExecutableInPath(tool, false);

      if (!toolPath.empty())
        return true;
    }

    err << "xcrun: cannot find '" << tool << "' executable" << err.endl();
    return false;
  }

  return true;
}

int help(Target &, char **) {
  std::cerr << "https://developer.apple.com/library/mac/documentation/Darwin/"
               "Reference/ManPages/man1/xcrun.1.html" << std::endl;
  return 0;
}

int version(Target &, char **) {
  std::cout << "xcrun version: 0." << std::endl;
  return 0;
}

int sdk(Target &, char **args) {
  sdkOption = args[0];
  return 0;
}

int log(Target &, char **) {
  showCommand = true;
  return 0;
}

// No tool name, as in "xcrun -f", "xcrun -r" or "xcrun -sdk macosx".
int noToolName(Target &, char **) {
  err << "xcrun: no utility name specified" << err.endl();
  return UsageError;
}

// args is nullptr for "-f" without a tool name.
int find(Target &target, char **args) {
  if (!args)
    return noToolName(target, args);

  const std::string toolName = args[0];
  std::string toolPath;
  if (!getToolPath(target, toolPath, toolName))
    return 1;
  std::cout << toolPath << std::endl;
  return 0;
}

int run(Target &target, char **args) {
  std::string toolPath;

  const std::string toolName = args[0];

  if (!getToolPath(target, toolPath, toolName))
    exit(1);

  // Like Apple's xcrun, pass the SDK on to the program.
  target.passSDKToChildProcesses();

  std::vector<std::string> programArgs;
  programArgs.push_back(toolPath);

  for (char **arg = &args[1]; *arg; ++arg)
    programArgs.push_back(*arg);

  if (showCommand) {
    for (size_t i = 0; i < programArgs.size(); ++i) {
      std::cout << programArgs[i];
      if (i != programArgs.size() - 1)
        std::cout << " ";
    }
    std::cout << std::endl;
  }

  process::execProgram(programArgs[0], programArgs);
  exit(1);

  // Silence -Wreturn-type warnings in case exit() is not marked as
  // "no-return" for whatever reason.
  __builtin_unreachable();
}

int showSDKPath(Target &target, char **) {
  std::cout << target.sdk.path << std::endl;
  return 0;
}

int showSDKVersion(Target &target, char **) {
  std::cout << target.sdk.version.shortStr() << std::endl;
  return 0;
}

int showPlatformPath(Target &target, char **) {
  std::cout << target.execDir << "/.." << std::endl;
  return 0;
}

// Every action needs the SDK; like Apple's xcrun, report a missing one
// before the action runs.
int prepare(Target &target, char **) {
  return determineSDK(target, sdkOption) ? 0 : 1;
}

} // unnamed namespace

int xcrun(int argc, char **argv, Target &target) {
  isStandaloneXcrun = !endsWith(argv[0], "-xcrun");

  if (getenv("xcrun_log"))
    showCommand = true;

  if (!getenv("OSXCROSS_XCRUN_NO_ENV_WARNING")) {
    constexpr const char *envVars[] = {
      "DEVELOPER_DIR", "TOOLCHAINS", "xcrun_verbose"
    };

    for (const char *envVar : envVars) {
      if (getenv(envVar)) {
        warn << "xcrun: ignoring environment variable "
            << "'" << envVar << "'" << warn.endl();
      }
    }
  }

  using Kind = ArgumentParser::Kind;
  using Value = ArgumentParser::Value;

  const ArgumentParser parser = {
    "xcrun", UsageError,
    false, // -sdk and --sdk alike
    {
      {"h", help, Kind::IMMEDIATE_ACTION},
      {"help", help, Kind::IMMEDIATE_ACTION},
      {"version", version, Kind::IMMEDIATE_ACTION},
      {"v", Kind::IGNORE},
      {"verbose", Kind::IGNORE},
      {"k", Kind::IGNORE},
      {"kill-cache", Kind::IGNORE},
      {"n", Kind::IGNORE},
      {"no-cache", Kind::IGNORE},
      {"sdk", sdk, Kind::SETTING, Value::ONE},
      {"toolchain", Kind::IGNORE, Value::ONE},
      {"l", log},
      {"log", log},
      {"f", find, Kind::FALLBACK_ACTION, Value::OPERAND},
      {"find", find, Kind::FALLBACK_ACTION, Value::OPERAND},
      // Runs the tool named by the operand, which is the default. Before
      // the operand, -r cancels -f; after it, -r alone is left and fails.
      {"r", noToolName, Kind::FALLBACK_ACTION},
      {"run", noToolName, Kind::FALLBACK_ACTION},
      {"show-sdk-path", showSDKPath, Kind::ACTION},
      {"show-sdk-version", showSDKVersion, Kind::ACTION},
      {"show-sdk-platform-version", showSDKVersion, Kind::ACTION},
      {"show-sdk-platform-path", showPlatformPath, Kind::ACTION}
    },
    // A program name, as in "xcrun clang ...": the remaining arguments
    // belong to the program.
    {nullptr, run, Kind::ACTION, Value::REST},
    prepare, noToolName
  };

  if (argc == 1) {
    help(target, nullptr);
    return UsageError;
  }

  return parser.execute(argc, argv, target);
}

} // namespace xcode
} // namespace program
