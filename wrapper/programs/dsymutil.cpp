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

namespace program {

int dsymutil(int argc, char **argv) {
  (void)argc;

  std::string dsymutil;
  char llvmveroutput[1024];
  const char *verstr;
  LLVMVersion llvmver;

  if (!realPath("llvm-dsymutil", dsymutil))
    return 0;

  std::string command = dsymutil + " -version";

  if (runcommand(command.c_str(), llvmveroutput, sizeof(llvmveroutput)) ==
      RUNCOMMAND_ERROR)
    return 0;

  verstr = strstr(llvmveroutput, "LLVM version ");

  if (!verstr)
    return 0;

  verstr += 13; // strlen("LLVM version ");

  llvmver = parseLLVMVersion(verstr);

  // LLVM <= 3.6 is too old
  if (llvmver <= LLVMVersion(3, 6))
    return 0;

  if (execvp(dsymutil.c_str(), argv))
    err << "cannot execute '" << dsymutil << "'" << err.endl();

  return 1;
}

} // namespace program
