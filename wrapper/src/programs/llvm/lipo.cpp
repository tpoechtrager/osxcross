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

#include "tools/tools.h"
#include "tools/process.h"
#include "target.h"
#include "programs/programs.h"

namespace program {
namespace llvm {

using tools::err;
using tools::process::execProgram;

int lipo(int argc, char **argv, target::Target &target) {
  if (!target.buildFlavor.isLLVM()) {
    err << "lipo: This wrapper is only intended to be used "
        << "with the OSXCross build flavor LLVM."
        << err.endl();
    return 1;
  }

  std::string executable;

  if (!getenv("OSXCROSS_FORCE_LLVM_LIPO"))
    executable = target::findExecutableInPath("osxcross-cctools-lipo");

  if (executable.empty())
    executable = "llvm-lipo";

  return execProgram(executable, argc, argv);
}

} // namespace llvm
} // namespace program
