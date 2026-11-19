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
#include <map>
#include <string>
#include <vector>

#include "tools/tools.h"
#include "target.h"
#include "programs/programs.h"

using namespace tools;
using namespace target;

namespace program {
namespace osxcross {

int env(int argc, char **argv, Target &) {
  const char *oldPath = getenv("PATH");

  if (!oldPath)
    abort();

  const std::string execDir = getExecutableDir();

  if (execDir.empty())
    exit(EXIT_FAILURE);

  if (argc <= 1) {
    const std::string parentName = getParentProcessName();

    if (parentName == "csh" || parentName == "tcsh") {
      std::cerr << std::endl << "you are invoking this program from a C shell, "
                << std::endl << "please use " << std::endl << std::endl
                << "setenv PATH `" << execDir << "/osxcross-env -v=PATH`"
                << std::endl << std::endl << "instead." << std::endl
                << std::endl;
    }
  }

  std::vector<std::string> path = splitPath(oldPath);
  std::map<std::string, std::string> vars;

  if (!hasPath(path, execDir))
    path.push_back(execDir);

  vars["PATH"] = joinPath(path);

  auto printVariable = [&](const std::string & var)->bool {
    auto it = vars.find(var);
    if (it == vars.end()) {
      std::cerr << "unknown variable '" << var << "'" << std::endl;
      return false;
    }
    std::cout << it->second << std::endl;
    return true;
  };

  if (argc <= 1) {
    std::cout << std::endl;
    for (auto &v : vars) {
      std::cout << "export " << v.first << "=";
      if (!printVariable(v.first))
        return 1;
      std::cout << std::endl;
    }
  } else {
    if (strncmp(argv[1], "-v=", 3))
      return 1;

    const char *var = argv[1] + 3;
    return printVariable(var) ? 0 : 1;
  }

  return 0;
}

} // namespace osxcross
} // namespace program
