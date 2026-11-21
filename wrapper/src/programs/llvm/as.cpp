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
#include <string>
#include <vector>

#include "tools/tools.h"
#include "tools/process.h"
#include "tools/response-file.h"
#include "target.h"
#include "programs/programs.h"
#include "programs/xcode/sdk.h"

namespace program {
namespace llvm {
namespace clang {

using tools::err;
using tools::process::execProgram;
using tools::process::printExecProgramArgs;

// This assembler wrapper is specific to the LLVM build flavor. It translates
// Darwin as(1) arguments for Clang's integrated assembler and must not be used
// by the cctools-based stable or latest flavors.
int as(int argc, char **argv, target::Target &target) {
  if (!target.buildFlavor.isLLVM()) {
    err << "as: This wrapper is only intended to be used "
        << "with the OSXCross build flavor LLVM."
        << err.endl();
    return 1;
  }

  // Options in response files (@<file>) apply as well, as with Apple's as,
  // which reads them as clang does. The clang wrapper below reads the files
  // again if they are passed on and prints the warnings for files left
  // unread.
  tools::responsefile::ExpandedArgs expanded;

  if (!tools::responsefile::expand(argc, argv,
                                   tools::responsefile::Syntax::CLANG, expanded,
                                   /*printWarnings=*/false))
    return 1;

  // as is an Xcode tool: read SDKROOT as xcrun does, so that a relative
  // path or a name such as "macosx" works, and pass the SDK on to the clang
  // wrapper below.
  const char *sdkRoot = getenv("SDKROOT");

  if (sdkRoot && sdkRoot[0]) {
    if (!xcode::determineSDK(target, nullptr))
      return 1;
    target.passSDKToChildProcesses();
  }

  const bool debug = tools::getDebugLevel() > 0;
  bool someInputFiles = false;
  bool outputSpecified = false;
  std::vector<const char *> givenArgs;

  for (int i = 1; i < expanded.argc; ++i) {
    const char *arg = expanded.argv[i];

    // Darwin's as driver accepts -Q/-q, but they have no useful equivalent
    // for Clang's integrated assembler. Forwarding them would make Clang fail.
    if (!strcmp(arg, "-Q") || !strcmp(arg, "-q"))
      continue;

    // Do not mistake a separate option argument for an input file. Darwin
    // as(1) accepts these options as two arguments, for example "-o out.o".
    if ((!strcmp(arg, "-o") || !strcmp(arg, "-I") ||
         !strcmp(arg, "-arch")) &&
        i + 1 < expanded.argc) {
      if (!strcmp(arg, "-o"))
        outputSpecified = true;
      givenArgs.push_back(expanded.argv[i]);
      givenArgs.push_back(expanded.argv[++i]);
      continue;
    }

    // Like Darwin as(1), assemble stdin when no input file, "-" or "--" was
    // supplied. Clang uses "-" for stdin while Darwin as also accepts "--".
    if (!strcmp(arg, "--") || !strcmp(arg, "-") || arg[0] != '-')
      someInputFiles = true;

    givenArgs.push_back(expanded.argv[i]);
  }

  // Run the clang wrapper for the same target, for example
  // x86_64-apple-darwinXX-as -> x86_64-apple-darwinXX-clang.
  target.setTriple();
  std::string clang = target.buildToolPath("clang");

  std::vector<std::string> args;
  args.push_back(clang);

  // Force assembler input even for stdin or files without a .s suffix. These
  // options must precede a synthesized "-" or Clang rejects stdin input.
  args.push_back("-x");
  args.push_back("assembler");

  if (!someInputFiles)
    args.push_back("-");

  for (const char *arg : givenArgs) {
    // Translate Darwin as(1)'s alternate stdin spelling for Clang.
    if (!strcmp(arg, "--"))
      args.push_back("-");
    // Darwin as(1) accepts -V; Clang does not.
    else if (strcmp(arg, "-V"))
      args.push_back(arg);
  }

  if (!outputSpecified) {
    args.push_back("-o");
    args.push_back("a.out");
  }

  // Avoid invoking an external as wrapper recursively, and stop Clang after
  // assembly so it cannot invoke the linker.
  args.push_back("-integrated-as");
  args.push_back("-c");

  // Darwin as(1) accepts driver options that may be unused after translation.
  args.push_back("-Wno-unused-command-line-argument");

  // The GCC wrapper communicates its selected deployment target through the
  // environment; the standalone assembler invocation cannot otherwise recover
  // that value.
  if (const char *version = getenv("OSXCROSS_AS_TARGET_VERSION"))
    args.push_back(std::string("-mmacos-version-min=") + version);

  if (debug)
    printExecProgramArgs(argc, argv, args);

  if (expanded.fileExpanded)
    return tools::responsefile::execProgram(clang, args);

  return execProgram(clang, args);
}

} // namespace clang
} // namespace llvm
} // namespace program
