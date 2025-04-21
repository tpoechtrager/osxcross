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

using namespace tools;

namespace program {
namespace llvm {

int ld(int argc, char **argv, target::Target &target) {
  // Print version info if requested
  if (argc >= 2 && strcmp(argv[1], "-v") == 0) {
    std::cout << "@(#)PROGRAM:ld  PROJECT:ld64-9999.9 (lld - use --version to see the LLVM version)" << std::endl;
    return 0;
  }

  bool debug = !!getenv("OCDEBUG");

  std::vector<char*> args;
  args.push_back(const_cast<char*>("ld64.lld"));

  // Add -platform_version if missing

  const char *minVersion = nullptr;
  bool seenPlatformVersion = false;

  if (char *p = getenv("MACOSX_DEPLOYMENT_TARGET"))
    minVersion = p;

  for (int i = 1; i < argc; ++i) {
    // Extract macOS version
    if (strcmp(argv[i], "-macos_version_min") == 0 && i + 1 < argc) {
      minVersion = argv[++i];
      continue;
    }

    // Already contains -platform_version?
    if (strcmp(argv[i], "-platform_version") == 0)
      seenPlatformVersion = true;

    args.push_back(argv[i]);
  }

  // Insert -platform_version if missing
  if (!seenPlatformVersion) {
    if (!minVersion)
      minVersion = strdup(target::getDefaultMinTarget().shortStr().c_str());

    args.insert(args.begin() + 1, {
      const_cast<char*>("-platform_version"),
      const_cast<char*>("macos"),
      const_cast<char*>(minVersion),
      strdup(target.getSDKOSNum().shortStr().c_str())
    });
  }

  if (debug) printArgs(argc, argv, args);

  args.push_back(nullptr);

  execvp(args[0], args.data());
  std::cerr << "Couldn't execute " << args[0] << std::endl;
  return 1;
}

} // namespace llvm
} // namespace program
