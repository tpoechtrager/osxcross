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

int pkg_config(int argc, char **argv) {
  (void)argc;

  bool execute = false;
  std::string varname;
  const char *val;

  // Map OSXCROSS_PKG_* to PKG_*
  for (char **env = environ; *env; ++env) {
     char *p = *env;

     if (!strncmp(p, "OSXCROSS_PKG", 12)) {
       execute = true;
       p += 9; // skip OSXCROSS_
       val = strchr(p, '=') + 1; // find value offset
       varname.assign(p, val - p - 1);
       setenv(varname.c_str(), val, 1);
     }
  }

  if (execute && execvp("pkg-config", argv))
    std::cerr << "cannot find or execute pkg-config" << std::endl;

  return 1;
}

} // namespace osxcross
} // namespace program
