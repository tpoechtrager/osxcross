/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2026 by Thomas Poechtrager                           *
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

#ifndef OSXCROSS_WRAPPER_TOOLS_RESPONSE_FILE_H
#define OSXCROSS_WRAPPER_TOOLS_RESPONSE_FILE_H

#include <string>
#include <vector>

// Response files: arguments of the form @<file>, which stand for the
// arguments written in <file>.
namespace tools {
namespace responsefile {

// The program that reads the response files, which decides how they are
// split into arguments. All split at whitespace and accept single quotes,
// double quotes and backslash escapes. Clang and ld64.lld use the rules of
// LLVM: they take the character after a backslash literally, also in
// quotes. GCC 15 changed how it reads some backslashes; GCC stands for the
// rules that all GCC versions share, and a file that depends on the version
// is left to GCC.
enum class Syntax { GCC, CLANG, LLD };

// Arguments after expand(), in the form main() gets them: argv[0] to
// argv[argc - 1], followed by a null pointer.
struct ExpandedArgs {
  int argc;
  char **argv;
  bool fileExpanded; // a response file was replaced with its arguments
};

// Replaces each argument @<file> in argv[1] to argv[argc - 1] with the
// arguments in <file>, recursively, as the program that reads the
// arguments does. A nested @<file> is relative to the current directory.
// @<file> stays as it is if the program would not read <file> either, for
// example because it does not exist or is a directory; the same holds for
// "-Wl,@<file>" and other arguments that do not start with '@'. It also
// stays, with a warning, if the wrapper cannot read <file> the way the
// program does: a GCC response file whose meaning depends on the GCC
// version, or a FIFO or UTF-16 file for clang and ld64.lld. With
// printWarnings set to false, these warnings are left to the program that
// gets the arguments, for a wrapper that passes them on to another osxcross
// wrapper, which reads them again.
//
// Stores the result in expanded; argc and argv are not changed. If no file
// was replaced with its arguments, expanded holds argc and argv. Otherwise,
// it points to new arguments, which stay valid until the program exits.
// Returns false after printing an error if a response file includes
// itself, the files are nested too deeply, or clang would read them with
// Windows rules.
bool expand(int argc, char **argv, Syntax syntax, ExpandedArgs &expanded,
            bool printWarnings = true);

// Like process::execProgram(), but passes args[1..] in a temporary
// response file, so that a long command line stays within the system's
// limits for arguments. The file is deleted after the program ended. Only
// returns if the program could not be started, and then returns 1.
int execProgram(const std::string &path,
                const std::vector<std::string> &args);

} // namespace responsefile
} // namespace tools

#endif // OSXCROSS_WRAPPER_TOOLS_RESPONSE_FILE_H
