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

int lipo(int argc, char **argv, target::Target &target) {
  if (!target.buildFlavor.IsLLVM()) {
    err << "lipo: This wrapper is only intended to be used "
        << "with the OSXCross build flavor LLVM."
        << err.endl();
    return 1;
  }

  (void)argc;
  std::string executable;

  if (getenv("OSXCROSS_FORCE_LLVM_LIPO") ||
      !target::findExecutableInPath("osxcross-cctools-lipo", executable))
    executable = "llvm-lipo";

  execvp(executable.c_str(), argv);
  err << "cannot execute '" << executable << "'" << err.endl();
  return 1;
}

} // namespace llvm
} // namespace program
