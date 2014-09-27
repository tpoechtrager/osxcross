/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014 by Thomas Poechtrager                           *
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

#ifndef _WIN32
#include <unistd.h>
#endif

extern char **environ;

namespace program {
namespace osxcross {

struct envvar {
  std::string name;
  std::string value;
  envvar(std::string name, std::string value) : name(name), value(value) {}
};

int pkg_config(int argc, char **argv) {
  (void)argc;

  std::vector<envvar> envvars;

  // Map OSXCROSS_PKG_* to PKG_*
  for (char **env = environ; *env; ++env) {
     char *p = *env;

     if (!strncmp(p, "OSXCROSS_PKG", 12)) {
       p += 9; // skip OSXCROSS_
       const char *val = strchr(p, '=') + 1; // find value offset
       envvars.push_back(envvar(std::string(p, val - p - 1), val));
     }
  }

  for (const envvar &evar : envvars)
    setenv(evar.name.c_str(), evar.value.c_str(), 1);

  if (!envvars.empty() && execvp("pkg-config", argv))
    std::cerr << "cannot find or execute pkg-config" << std::endl;

  return 1;
}

} // namespace osxcross
} // namespace program
