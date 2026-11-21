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

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

#include "tools/tools.h"
#include "tools/process.h"
#include "tools/response-file.h"
#include "target.h"
#include "programs/programs.h"

namespace program {
namespace llvm {

using tools::err;
using tools::process::execProgram;
using tools::process::printExecProgramArgs;

// This linker wrapper is specific to the LLVM build flavor. It translates
// Apple ld64 arguments for ld64.lld and must not be used by the cctools-based
// stable or latest flavors.
int ld(int argc, char **argv, target::Target &target) {
  if (!target.buildFlavor.isLLVM()) {
    err << "ld: This wrapper is only intended to be used "
        << "with the OSXCross build flavor LLVM."
        << err.endl();
    return 1;
  }

  // Options in response files (@<file>) are translated as well. ld64.lld
  // would read the files itself, but would not understand ld64 options
  // such as -macos_version_min or -ld_classic in them.
  tools::responsefile::ExpandedArgs expanded;

  if (!tools::responsefile::expand(argc, argv,
                                   tools::responsefile::Syntax::LLD, expanded))
    return 1;

  // Some build systems expect Apple's ld64 version format when probing the
  // linker. Imitate that format for compatibility. The ld64 project version
  // below is synthetic; use --version to print the actual LLD version.
  const char *versionLine = "@(#)PROGRAM:ld  PROJECT:ld64-9999.9 "
                            "(lld - use --version to see the LLVM version)";

  if (expanded.argc == 2 && !strcmp(expanded.argv[1], "-v")) {
    std::cout << versionLine << std::endl;
    return 0;
  }

  const bool debug = tools::getDebugLevel() > 0;
  const char *minimumVersion = nullptr; // -macos_version_min
  const char *sdkVersionArg = nullptr;  // -sdk_version
  const char *sysRoot = nullptr;        // the last -syslibroot
  const char *archName = nullptr;       // the last -arch
  bool platformVersionSeen = false;
  std::vector<std::string> args;

  args.push_back("ld64.lld");

  for (int i = 1; i < expanded.argc; ++i) {
    const char *arg = expanded.argv[i];

    // Like Apple's ld64, print the version and link. ld64.lld would print its
    // own version and the search paths instead.
    if (!strcmp(arg, "-v")) {
      std::cout << versionLine << std::endl;
      continue;
    }

    // GCC and older Darwin tooling use the legacy ld64 option. LLD expects the
    // minimum version as part of -platform_version instead, so consume it here.
    if ((!strcmp(arg, "-macos_version_min") ||
         !strcmp(arg, "-macosx_version_min")) &&
        i + 1 < expanded.argc) {
      minimumVersion = expanded.argv[++i];
      continue;
    }

    // The SDK version for the legacy option; ld64.lld ignores -sdk_version
    // with a warning, so it goes into -platform_version as well.
    if (!strcmp(arg, "-sdk_version") && i + 1 < expanded.argc) {
      sdkVersionArg = expanded.argv[++i];
      continue;
    }

    // GCC emits this Apple ld64 option for Darwin targets. ld64.lld does not
    // implement it and emits a warning, so deliberately do not forward it.
    if (!strcmp(arg, "-no_compact_unwind"))
      continue;

    // Select the classic or the new linker of Apple's ld64 (Xcode 15).
    // ld64.lld would read them as -l<library>.
    if (!strcmp(arg, "-ld_classic") || !strcmp(arg, "-ld_new"))
      continue;

    // The caller's SDK and architecture, for -platform_version below. Like
    // ld64, the last -arch wins; the last -syslibroot gives the SDK version.
    if ((!strcmp(arg, "-syslibroot") || !strcmp(arg, "-arch")) &&
        i + 1 < expanded.argc) {
      if (!strcmp(arg, "-syslibroot"))
        sysRoot = expanded.argv[i + 1];
      else
        archName = expanded.argv[i + 1];

      args.push_back(arg);
      args.push_back(expanded.argv[++i]);
      continue;
    }

    // Preserve a complete caller-provided platform tuple instead of adding a
    // second, potentially conflicting -platform_version option.
    if (!strcmp(arg, "-platform_version"))
      platformVersionSeen = true;

    args.push_back(arg);
  }

  if (sysRoot) {
    // The caller's SDK takes the place of -isysroot; determineSDK() gives
    // its version. A missing directory is left to ld64.lld.
    target.determineSDK(target.compilerSDKSources({{"-syslibroot", sysRoot}}),
                        true);
  } else {
    // Without -syslibroot, ld64.lld searches the libraries of the host
    // (/usr/lib, /usr/local/lib). Link against the selected SDK instead.
    if (!target.determineSDK(target.compilerSDKSources()))
      return 1;

    args.insert(args.begin() + 1, {"-syslibroot", target.sdk.path});
  }

  // ld64.lld needs both the deployment target and SDK version. Synthesize the
  // tuple when the caller only supplied the legacy option (or no version).
  if (!platformVersionSeen) {
    const tools::OSVersion sdkVersion =
        sdkVersionArg ? tools::parseOSVersion(sdkVersionArg)
                      : target.sdk.version;

    // The deployment target: -macos_version_min or MACOSX_DEPLOYMENT_TARGET,
    // passed on as given; as in the compiler wrapper, an empty or invalid
    // MACOSX_DEPLOYMENT_TARGET counts as not set.
    const char *envMinimum = getenv("MACOSX_DEPLOYMENT_TARGET");
    std::string minimum;

    if (minimumVersion) {
      minimum = minimumVersion;
    } else if (envMinimum && tools::parseOSVersion(envMinimum).num()) {
      minimum = envMinimum;
    } else {
      // The default, as in Target::setup(): the build default (the SDK
      // version if there is none), raised to what the architecture needs.
      tools::OSVersion defaultMinimum = target::getDefaultDeploymentTarget();

      if (!defaultMinimum.num())
        defaultMinimum = sdkVersion;

      const tools::Arch arch =
          archName ? tools::parseArch(archName) : target.primaryArch;

      defaultMinimum =
          std::max(defaultMinimum, target::getMinDeploymentTargetForArch(arch));
      minimum = defaultMinimum.shortStr();
    }

    // As with clang, an SDK without a known version gets the deployment
    // target as its version.
    std::string sdk = minimum;

    if (sdkVersionArg)
      sdk = sdkVersionArg;
    else if (sdkVersion.num())
      sdk = sdkVersion.shortStr();

    args.insert(args.begin() + 1,
                {"-platform_version", "macos", minimum, sdk});
  }

  if (debug)
    printExecProgramArgs(argc, argv, args);

  if (expanded.fileExpanded)
    return tools::responsefile::execProgram(args[0], args);

  return execProgram(args[0], args);
}

} // namespace llvm
} // namespace program
