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
namespace clang {

using tools::err;

// This assembler wrapper is specific to the LLVM build flavor. It translates
// Darwin as(1) arguments for Clang's integrated assembler and must not be used
// by the cctools-based stable or latest flavors.
int as(int argc, char **argv, target::Target &target) {
  if (!target.buildFlavor.IsLLVM()) {
    err << "as: This wrapper is only intended to be used "
        << "with the OSXCross build flavor LLVM."
        << err.endl();
    return 1;
  }

  const bool debug = getenv("OCDEBUG") != nullptr;
  bool someInputFiles = false;
  bool outputSpecified = false;
  std::vector<char *> givenArgs;

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];

    // Darwin's as driver accepts -Q/-q, but they have no useful equivalent
    // for Clang's integrated assembler. Forwarding them would make Clang fail.
    if (!strcmp(arg, "-Q") || !strcmp(arg, "-q"))
      continue;

    // Do not mistake a separate option argument for an input file. Darwin
    // as(1) accepts these options as two arguments, for example "-o out.o".
    if ((!strcmp(arg, "-o") || !strcmp(arg, "-I") ||
         !strcmp(arg, "-arch")) &&
        i + 1 < argc) {
      if (!strcmp(arg, "-o"))
        outputSpecified = true;
      givenArgs.push_back(argv[i]);
      givenArgs.push_back(argv[++i]);
      continue;
    }

    // Like Darwin as(1), assemble stdin when no input file, "-" or "--" was
    // supplied. Clang uses "-" for stdin while Darwin as also accepts "--".
    if (!strcmp(arg, "--") || !strcmp(arg, "-") || arg[0] != '-')
      someInputFiles = true;

    givenArgs.push_back(argv[i]);
  }

  std::vector<char *> args;
  args.push_back(const_cast<char *>("xcrun"));
  args.push_back(const_cast<char *>("clang"));

  // Force assembler input even for stdin or files without a .s suffix. These
  // options must precede a synthesized "-" or Clang rejects stdin input.
  args.push_back(const_cast<char *>("-x"));
  args.push_back(const_cast<char *>("assembler"));

  if (!someInputFiles)
    args.push_back(const_cast<char *>("-"));

  for (std::vector<char *>::const_iterator it = givenArgs.begin();
       it != givenArgs.end(); ++it) {
    // Translate Darwin as(1)'s alternate stdin spelling for Clang.
    if (!strcmp(*it, "--"))
      args.push_back(const_cast<char *>("-"));
    // Darwin as(1) accepts -V; Clang does not.
    else if (strcmp(*it, "-V"))
      args.push_back(*it);
  }

  if (!outputSpecified) {
    args.push_back(const_cast<char *>("-o"));
    args.push_back(const_cast<char *>("a.out"));
  }

  // Avoid invoking an external as wrapper recursively, and stop Clang after
  // assembly so it cannot invoke the linker.
  args.push_back(const_cast<char *>("-integrated-as"));
  args.push_back(const_cast<char *>("-c"));

  // Darwin as(1) accepts driver options that may be unused after translation.
  args.push_back(const_cast<char *>("-Wno-unused-command-line-argument"));

  // The GCC wrapper communicates its selected deployment target through the environment;
  // the standalone assembler invocation cannot otherwise recover that value.
  if (char *version = getenv("OSXCROSS_AS_TARGET_VERSION")) {
    std::string minimumVersion = "-mmacos-version-min=";
    minimumVersion += version;
    args.push_back(strdup(minimumVersion.c_str()));
  }

  if (debug)
    printExternalToolArgs(argc, argv, args);

  args.push_back(nullptr);
  execvp(args[0], args.data());

  err << "Couldn't execute " << args[0] << err.endl();
  return 1;
}

} // namespace clang
} // namespace llvm
} // namespace program
