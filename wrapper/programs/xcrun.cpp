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

#include "compat.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <strings.h>

#include "tools.h"
#include "target.h"
#include "progs.h"

using namespace tools;
using namespace target;

namespace program {
namespace {

bool isStandaloneXcrun = false;
bool showCommand = false;

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

  toolPath.clear();

  if (tool[0] == PathDiv) {
    if (isXcodeTool)
      tool = fileName; // map /usr/bin/clang (etc.) to <triple>-clang
    else
      toolPath = tool;
  }

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

  if (!fileExists(toolPath)) {
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
  const char *sdkArg = argv[0];

  if (!strcmp(sdkArg, "macosx") || !strcmp(sdkArg, "macosx.internal"))
    return 0;

  // Ignore '/'.
  if (sdkArg[0] == PathDiv && !sdkArg[1])
    return 0;

  std::string sdkPath = sdkArg;

  while (sdkPath.size() && sdkPath[sdkPath.size() - 1] == PathDiv)
    sdkPath.erase(sdkPath.size() - 1, 1);

  const char *sdkName = getFileName(sdkPath);

  if (!strncasecmp(sdkName, "MacOSX", 6)) {
    if (!dirExists(sdkArg)) {
      err << "xcrun: '-sdk': directory '" << sdkArg << "' does not exist"
          << err.endl();
      return 1;
    }

    // The in-process queries (--show-sdk-path, --show-sdk-version) and the
    // tools that xcrun starts read OSXCROSS_SDKROOT. Set SDKROOT as well, as
    // Apple's xcrun does, so that a stale SDKROOT does not reach the tools:
    // a nested xcrun would prefer it over OSXCROSS_SDKROOT.
    setenv("OSXCROSS_SDKROOT", sdkArg, 1);
    setenv("SDKROOT", sdkArg, 1);

    return 0;
  }

  // Ignore empty argument.
  if (sdkArg[0]) {
    err << "xcrun: '-sdk': expected macOS SDK" << err.endl();
    return 1;
  }

  return 0;
}

int log(Target *, char **) {
  showCommand = true;
  return 0;
}

int find(Target *target, char **argv) {
  if (argv[1])
    return 1;
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
  std::string sdkPath;
  if (!target->getSDKPath(sdkPath))
    return 1;
  std::cout << sdkPath << std::endl;
  return 0;
}

int showSDKVersion(Target *target, char **) {
  std::cout << target->getSDKOSNum().shortStr() << std::endl;
  return 0;
}

int showPlatformPath(Target *target, char **) {
  std::cout << target->execDir << "/.." << std::endl;
  return 0;
}

} // anonymous namespace

int applySDKRootFromEnvironment() {
  // SDKROOT is the SDK selector used by Apple's tools. Apple's xcrun
  // passes an explicitly selected SDK to developer tools through SDKROOT.
  // OSXCROSS_SDKROOT is our own SDK override, read by the compiler wrapper.
  // Translate the incoming SDKROOT selection into that internal override.
  //
  // If SDKROOT is unset, preserve OSXCROSS_SDKROOT. Otherwise SDKROOT
  // takes precedence: clear the old override and let sdk() validate it.
  // Empty SDKROOT, "/", "macosx" and "macosx.internal" select the default
  // SDK by leaving OSXCROSS_SDKROOT unset.
  //
  // For an explicit SDK path, sdk() sets both variables. This also matters
  // when xcrun handles -sdk: a nested xcrun must inherit the new SDKROOT,
  // or it could prefer an older SDKROOT and undo the outer SDK selection.
  // Apple propagates its selection through SDKROOT; OSXCROSS_SDKROOT is
  // the additional variable needed by our compiler wrapper.
  char *sdkRoot = getenv("SDKROOT");

  if (!sdkRoot)
    return 0;

  // Environment updates may invalidate the pointer returned by getenv().
  const std::string sdkValue = sdkRoot;
  unsetenv("OSXCROSS_SDKROOT");
  char *sdkArgs[1] = { const_cast<char *>(sdkValue.c_str()) };
  return sdk(nullptr, sdkArgs);
}

int xcrun(int argc, char **argv, Target &target) {
  isStandaloneXcrun = !endsWith(argv[0], "-xcrun");

  if (getenv("xcrun_log"))
    showCommand = true;

  if (!getenv("OSXCROSS_XCRUN_NO_ENV_WARNING")) {
    constexpr const char *envVars[] = {
      "DEVELOPER_DIR", "TOOLCHAINS", "xcrun_verbose"
    };

    for (const char *evar : envVars) {
      if (getenv(evar)) {
        warn << "xcrun: ignoring environment variable "
            << "'" << evar << "'" << warn.endl();
      }
    }
  }

  const int sdkResult = applySDKRootFromEnvironment();
  if (sdkResult != 0)
    return sdkResult;

  auto dummy = [](Target *, char **) { return 0; };

  ArgParser<int (*)(Target *, char **), 21> argParser = {{
    {"h", help},
    {"help", help},
    {"version", version},
    {"v", dummy},
    {"verbose", dummy},
    {"k", dummy},
    {"kill-cache", dummy},
    {"n", dummy},
    {"no-cache", dummy},
    {"sdk", sdk, 1},
    {"toolchain", dummy, 1},
    {"l", log },
    {"log", log},
    {"f", find, 1},
    {"find", find, 1},
    {"r", run, 1},
    {"run", run, 1},
    {"show-sdk-path", showSDKPath},
    {"show-sdk-version", showSDKVersion},
    {"show-sdk-platform-version", showSDKVersion},
    {"show-sdk-platform-path", showPlatformPath}
  }};

  if (argc == 1)
    help();

  int retVal = 1;

  for (int i = 1; i < argc; ++i) {
    auto b = argParser.parseArg(argc, argv, i);

    if (!b) {
      if (argv[i][0] == '-') {
        err << "xcrun: unknown argument: '" << argv[i] << "'" << err.endl();
        retVal = 2;
        break;
      }

      run(&target, &argv[i]);
    }

    retVal = b->fun(&target, &argv[i + 1]);

    if (retVal != 0)
      break;

    i += b->numArgs;
  }

  return retVal;
}

} // namespace program
