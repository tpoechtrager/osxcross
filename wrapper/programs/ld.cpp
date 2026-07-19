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

#include "proginc.h"

namespace program {
namespace llvm {

using tools::err;

// This linker wrapper is specific to the LLVM build flavor. It translates
// Apple ld64 arguments for ld64.lld and must not be used by the cctools-based
// stable or latest flavors.
int ld(int argc, char **argv, target::Target &target) {
  if (!target.buildFlavor.IsLLVM()) {
    err << "ld: This wrapper is only intended to be used "
        << "with the OSXCross build flavor LLVM."
        << err.endl();
    return 1;
  }

  // Some build systems expect Apple's ld64 version format when probing the
  // linker. Imitate that format for compatibility. The ld64 project version
  // below is synthetic; use --version to print the actual LLD version.
  if (argc >= 2 && !strcmp(argv[1], "-v")) {
    std::cout << "@(#)PROGRAM:ld  PROJECT:ld64-9999.9 "
                 "(lld - use --version to see the LLVM version)"
              << std::endl;
    return 0;
  }

  const bool debug = getenv("OCDEBUG") != nullptr;
  const char *minimumVersion = getenv("MACOSX_DEPLOYMENT_TARGET");
  bool platformVersionSeen = false;
  std::vector<char *> args;

  args.push_back(const_cast<char *>("ld64.lld"));

  for (int i = 1; i < argc; ++i) {
    // GCC and older Darwin tooling use the legacy ld64 option. LLD expects the
    // minimum version as part of -platform_version instead, so consume it here.
    if ((!strcmp(argv[i], "-macos_version_min") ||
         !strcmp(argv[i], "-macosx_version_min")) &&
        i + 1 < argc) {
      minimumVersion = argv[++i];
      continue;
    }

    // GCC emits this Apple ld64 option for Darwin targets. ld64.lld does not
    // implement it and emits a warning, so deliberately do not forward it.
    if (!strcmp(argv[i], "-no_compact_unwind"))
      continue;

    // Preserve a complete caller-provided platform tuple instead of adding a
    // second, potentially conflicting -platform_version option.
    if (!strcmp(argv[i], "-platform_version"))
      platformVersionSeen = true;

    args.push_back(argv[i]);
  }

  // ld64.lld needs both the deployment target and SDK version. Synthesize the
  // tuple when the caller only supplied the legacy option (or no version).
  if (!platformVersionSeen) {
    if (!minimumVersion)
      minimumVersion = strdup(target::getDefaultMinTarget().shortStr().c_str());

    args.insert(args.begin() + 1,
                {const_cast<char *>("-platform_version"),
                 const_cast<char *>("macos"),
                 const_cast<char *>(minimumVersion),
                 strdup(target.getSDKOSNum().shortStr().c_str())});
  }

  if (debug)
    printExternalToolArgs(argc, argv, args);

  args.push_back(nullptr);
  execvp(args[0], args.data());

  err << "Couldn't execute " << args[0] << err.endl();
  return 1;
}

} // namespace llvm
} // namespace program
