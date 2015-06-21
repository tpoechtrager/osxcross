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

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace tools;
using namespace target;

namespace program {
namespace osxcross {

int man(int argc, char **argv, Target &target) {
  std::string SDKPath;
  target.getSDKPath(SDKPath);
  std::string manpath = SDKPath + "/usr/share/man";

  if (!dirExists(manpath)) {
    err << "directory '" << manpath << "' does not exist" << err.endl();
    return 1;
  }

  std::vector<char *> args;

  args.push_back(const_cast<char *>("man"));
  args.push_back(const_cast<char *>("--manpath"));
  args.push_back(const_cast<char *>(manpath.c_str()));

  for (int i = 1; i < argc; ++i)
    args.push_back(argv[i]);

  args.push_back(nullptr);

  execvp(args[0], args.data());
  err << "cannot execute '" << args[0] << "'" << err.endl();
  return 1;
}

} // namespace osxcross
} // namespace program
