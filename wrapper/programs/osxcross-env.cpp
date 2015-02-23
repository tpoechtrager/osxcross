/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014, 2015 by Thomas Poechtrager                     *
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
#include <map>

using namespace tools;
using namespace target;

namespace program {
namespace osxcross {

int env(int argc, char **argv) {
  char epath[PATH_MAX + 1];
  char *oldpath = getenv("PATH");

  assert(oldpath);

  if (!getExecutablePath(epath, sizeof(epath)))
    exit(EXIT_FAILURE);

  // TODO: escape?

  auto containsBadChars = [](const char * p, const char * desc)->bool {
    if (!p)
      return false;

    const char *pp = p;

    do {
      auto badChar = [&](const char *p) {
        std::cerr << desc << " should not contain '" << *p << "'" << std::endl;

        const char *start = p - std::min<size_t>(p - pp, 30);

        size_t len = std::min<size_t>(strlen(start), 60);
        std::cerr << std::string(start, len) << std::endl;

        while (start++ != p)
          std::cerr << " ";

        std::cerr << "^" << std::endl;
      };
      switch (*p) {
      case '"':
      case '\'':
      case '$':
      case ' ':
      case ';':
        badChar(p);
        return true;
      }
    } while (*p && *++p);
    return false;
  };

  if (argc <= 1) {
    const std::string &pname = getParentProcessName();

    if (pname == "csh" || pname == "tcsh") {
      std::cerr << std::endl << "you are invoking this program from a C shell, "
                << std::endl << "please use " << std::endl << std::endl
                << "setenv PATH `" << epath << "/osxcross-env -v=PATH`"
                << std::endl << std::endl << "instead." << std::endl
                << std::endl;
    }
  }

  auto hasPath = [](const char * ov, const char * v, const char * vs)->bool {
    // ov = old value
    // v = value
    // vs = value suffix

    if (!ov || !v)
      return false;

    bool hasPathSeparator = false;

    for (auto p = ov; *p; ++p) {
      if (*p == ':') {
        hasPathSeparator = true;
        break;
      }
    }

    static std::string tmp;

    auto check = [&](int t)->bool {
      tmp.clear();

      if (t == 0)
        tmp = ':';

      tmp += v;

      if (vs)
        tmp += vs;

      if (t == 1)
        tmp += ':';

      return strstr(ov, tmp.c_str()) != nullptr;
    };

    return ((hasPathSeparator && (check(0) || check(1))) || check(-1));
  };

  if (containsBadChars(oldpath, "PATH"))
    return 1;

  std::stringstream path;
  std::stringstream librarypath;
  std::map<std::string, std::string> vars;

  path << oldpath;

  if (!hasPath(oldpath, epath, nullptr))
    path << ":" << epath;

  vars["PATH"] = path.str();

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
    return static_cast<int>(printVariable(var));
  }

  return 0;
}

} // namespace osxcross
} // namespace program
