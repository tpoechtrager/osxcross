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

#include "proginc.h"

using namespace tools;
using namespace target;

namespace program {
namespace {

int version(Target*, char**) {
  std::cout << "Xcode 11.0.0" << std::endl;
  std::cout << "Build version 0CFFFF" << std::endl;
  return 0;
}

int help(Target* = nullptr, char** = nullptr) {
  std::cerr << "Only '-version' is supported by this stub tool" << std::endl;
  return 0;
}

} // anonymous namespace

int xcodebuild(int argc, char **argv, Target &target) {
  auto dummy = [](Target*, char**) { return 0; };

  ArgParser<int (*)(Target*, char**), 3> argParser = {{
    {"version", version},
    {"sdk", dummy},
    {"help", help}
  }};

  if (argc == 1)
    help();

  int retVal = 1;

  for (int i = 1; i < argc; ++i) {
    auto b = argParser.parseArg(argc, argv, i);

    if (!b) {
      if (argv[i][0] == '-') {
        err << "xcodebuild: unknown argument: '" << argv[i] << "'" << err.endl();
        retVal = 2;
        break;
      }

      continue;
    }

    retVal = b->fun(&target, &argv[i + 1]);

    if (retVal != 0)
      break;

    i += b->numArgs;
  }

  return retVal;
}

} // namespace program
