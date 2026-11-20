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
#include <string>
#include <vector>

#include "tools/tools.h"
#include "tools/process.h"
#include "target.h"
#include "programs/programs.h"

using namespace tools;
using namespace target;

namespace program {
namespace host {

int man(int argc, char **argv, Target &target) {
  bool isStandaloneOSXCrossMan = !endsWith(argv[0], "-osxcross-man");

  if (!target.determineSDK(target.compilerSDKSources()))
    return 1;

  std::vector<std::string> manPaths;

  manPaths.push_back(target.sdk.path + "/usr/share/man");
  manPaths.push_back(std::string(target.execDir) + "/../share/man");

  unsetenv("MANPATH");

  for (const std::string &manPath : manPaths)
    if (dirExists(manPath))
      prependEnvVariable("MANPATH", manPath);

  if (!getenv("MANPATH")) {
    err << "cannot find man pages!" << err.endl();
    return 1;
  }

  std::vector<std::string> args;
  args.push_back("man");

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    // Rewrite gcc to <triple>-gcc for compatibility
    // with other man pages

    constexpr const char *gccManPages[] = { "gcc", "g++", "cpp", "gcov" };

    for (const char *mp : gccManPages) {
      if (arg == mp) {
        std::string triple;
        if (isStandaloneOSXCrossMan) {
          // osxcross-man -> use default GCC architecture for triple
          target.buildDefaultTriple(triple, /* GCC */ true,
                                    /* useAarch64InsteadOfArm64 */ true);
        } else {
          // <triple>-osxcross-man -> use the triple architecture for triple
          target.setTriple(/* useAarch64InsteadOfArm64 */ true);
          triple = target.getTriple();
        }
        arg = triple + "-" + arg;
        break;
      }
    }

    args.push_back(arg);
  }

  return process::execProgram(args[0], args);
}

} // namespace host
} // namespace program
