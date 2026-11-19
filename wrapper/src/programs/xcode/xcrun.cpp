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
#include "target.h"
#include "progs.h"
#include "programs/xcode/sdk.h"

using namespace tools;
using namespace target;

namespace program {
namespace xcode {
namespace {

bool isStandaloneXcrun = false;
bool showCommand = false;

// Like Apple's xcrun, the options are read first, up to the name of the
// program to run. Then only the last action runs, with the SDK of the last
// -sdk option.
const char *sdkOption = nullptr;            // the last -sdk; nullptr: not given
int (*action)(Target *, char **) = nullptr; // the last action
char **actionArgs = nullptr;                // and its arguments

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

bool getToolPath(Target *target, std::string &toolPath, std::string tool) {
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
      target->buildDefaultTriple(triple, isGCC);
    } else {
      // A target-prefixed xcrun must resolve tools for that exact target.
      // For example, arm64-apple-darwin27-xcrun -f gcc must fail when
      // no ARM64 GCC installation exists.
      target->setTriple();
      triple = target->getTriple();
    }

    toolPath = target->buildToolPath(triple, tool);
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

int help(Target * = nullptr, char ** = nullptr) {
  std::cerr << "https://developer.apple.com/library/mac/documentation/Darwin/"
               "Reference/ManPages/man1/xcrun.1.html" << std::endl;
  return 0;
}

int version(Target *, char **) {
  std::cout << "xcrun version: 0." << std::endl;
  return 0;
}

int sdk(Target *, char **argv) {
  sdkOption = argv[0];
  return 0;
}

// Options that xcrun accepts but that change nothing here.
int ignore(Target *, char **) { return 0; }

int log(Target *, char **) {
  showCommand = true;
  return 0;
}

int find(Target *target, char **argv) {
  const std::string toolName = argv[0];
  std::string toolPath;
  if (!getToolPath(target, toolPath, toolName))
    return 1;
  std::cout << toolPath << std::endl;
  return 0;
}

int run(Target *target, char **argv) {
  std::string toolPath;
  std::string command;

  const std::string toolName = argv[0];

  if (!getToolPath(target, toolPath, toolName))
    exit(1);

  // Like Apple's xcrun, pass the SDK on to the program.
  target->passSDKToChildProcesses();

  std::vector<std::string> args;
  args.push_back(toolPath);

  for (char **arg = &argv[1]; *arg; ++arg)
    args.push_back(*arg);

  if (showCommand) {
    for (size_t i = 0; i < args.size(); ++i) {
      std::cout << args[i];
      if (i != args.size() - 1)
        std::cout << " ";
    }
    std::cout << std::endl;
  }

  execProgram(args[0], args);
  exit(1);

  // Silence -Wreturn-type warnings in case exit() is not marked as
  // "no-return" for whatever reason.
  __builtin_unreachable();
}

int showSDKPath(Target *target, char **) {
  std::cout << target->sdk.path << std::endl;
  return 0;
}

int showSDKVersion(Target *target, char **) {
  std::cout << target->sdk.version.shortStr() << std::endl;
  return 0;
}

int showPlatformPath(Target *target, char **) {
  std::cout << target->execDir << "/.." << std::endl;
  return 0;
}

} // anonymous namespace

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

  ArgParser<int (*)(Target *, char **), 21> argParser = {{
    {"h", help, 0, true},
    {"help", help, 0, true},
    {"version", version, 0, true},
    {"v", ignore},
    {"verbose", ignore},
    {"k", ignore},
    {"kill-cache", ignore},
    {"n", ignore},
    {"no-cache", ignore},
    {"sdk", sdk, 1},
    {"toolchain", ignore, 1},
    {"l", log},
    {"log", log},
    {"f", find, 1, true},
    {"find", find, 1, true},
    {"r", run, 1, true},
    {"run", run, 1, true},
    {"show-sdk-path", showSDKPath, 0, true},
    {"show-sdk-version", showSDKVersion, 0, true},
    {"show-sdk-platform-version", showSDKVersion, 0, true},
    {"show-sdk-platform-path", showPlatformPath, 0, true}
  }};

  if (argc == 1) {
    help();
    return 1;
  }

  for (int i = 1; i < argc; ++i) {
    auto b = argParser.parseArg(argc, argv, i);

    if (!b) {
      if (argv[i][0] == '-') {
        err << "xcrun: unknown argument: '" << argv[i] << "'" << err.endl();
        return 2;
      }

      // A program name, as in "xcrun clang ...".
      action = run;
      actionArgs = &argv[i];
      break;
    }

    // Actions run after the loop; only the last one counts.
    if (b->action) {
      action = b->fun;
      actionArgs = &argv[i + 1];

      // -r, --run: the remaining arguments belong to the program.
      if (action == run)
        break;
    } else {
      b->fun(&target, &argv[i + 1]);
    }

    i += b->numArgs;
  }

  if (!action)
    return 0;

  if (!determineSDK(target, sdkOption))
    return 1;

  return action(&target, actionArgs);
}

} // namespace xcode
} // namespace program
