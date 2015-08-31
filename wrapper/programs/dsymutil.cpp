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

int dsymutil(int argc, char **argv, Target &target) {
  (void)argc;

  std::string dsymutil;
  char LLVMDsymutilVersionOutput[1024];
  const char *LLVMDsymutilVersionStr;
  LLVMVersion LLVMDsymutilVersion;

  const std::string &ParentProcessName = getParentProcessName();

  if (!debug && ParentProcessName.find("clang") == std::string::npos &&
      ParentProcessName != "collect2" && ParentProcessName != "unknown")
    debug = 1;

  if (char *p = getenv("OSXCROSS_LLVM_DSYMUTIL")) {
    dsymutil = p;
    debug = 1;
  } else {
    if (!realPath("osxcross-llvm-dsymutil", dsymutil) &&
        !realPath("llvm-dsymutil", dsymutil)) {
      if (debug)
        dbg << "dsymutil: cannot find [osxcross-]llvm-dsymutil in PATH"
            << dbg.endl();
      return 0;
    }
  }

  std::string command = dsymutil + " -version";

  if (runcommand(command.c_str(), LLVMDsymutilVersionOutput,
                 sizeof(LLVMDsymutilVersionOutput)) == RUNCOMMAND_ERROR) {
    if (debug)
      dbg << "dsymutil: executing \"" << command << "\" failed"
          << dbg.endl();
    return 0;
  }

  LLVMDsymutilVersionStr = strstr(LLVMDsymutilVersionOutput, "LLVM version ");

  if (!LLVMDsymutilVersionStr) {
    if (debug)
      dbg << "dsymutil: unable to parse llvm-dsymutil version"
          << dbg.endl();
    return 0;
  }

  LLVMDsymutilVersionStr += 13; // strlen("LLVM version ");

  LLVMDsymutilVersion = parseLLVMVersion(LLVMDsymutilVersionStr);

  constexpr LLVMVersion RequiredLLVMDsymutilVersion(3, 8);

  if (LLVMDsymutilVersion < RequiredLLVMDsymutilVersion) {
    if (debug)
      dbg << "ignoring dsymutil invocation: '"
          << dsymutil << "' is too old ("
          << LLVMDsymutilVersion.Str() << " < "
          << RequiredLLVMDsymutilVersion.Str() << ")"
          << dbg.endl();
    return 0;
  }

  std::stringstream lipo;
  std::string triple;

  lipo << target.execpath << PATHDIV
       << target.getDefaultTriple(triple) << "-lipo";

  setenv("LIPO", lipo.str().c_str(), 1);

  if (execvp(dsymutil.c_str(), argv))
    err << "cannot execute '" << dsymutil << "'" << err.endl();

  return 1;
}

} // namespace program
