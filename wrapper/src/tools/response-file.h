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

// How a program splits a response file into arguments. Both split at
// whitespace and accept single quotes, double quotes and backslash escapes.
// LLVM tools such as clang and ld64.lld take the character after a
// backslash literally, also in quotes. GCC 15 changed how it reads some
// backslashes; GCC stands for the rules that all GCC versions share, and a
// file that depends on the version is left to GCC.
enum class Syntax { GCC, LLVM };

// Replaces each argument @<file> in argv[1] to argv[argc - 1] with the
// arguments in <file>, recursively, as the program that reads the
// arguments does. A nested @<file> is relative to the current directory.
// @<file> stays as it is if the program would not read <file> either, for
// example because it does not exist or is a directory; the same holds for
// "-Wl,@<file>" and other arguments that do not start with '@'. It also
// stays, with a warning, if the wrapper cannot read <file> the way the
// program does: a GCC response file whose meaning depends on the GCC
// version, or a FIFO or UTF-16 file for clang.
//
// If a file was replaced with its arguments, argc and argv are set to the
// expanded arguments, which stay valid until the program exits and end with
// a null pointer, and expanded is set to true. Otherwise, argc, argv and
// expanded are not changed. Returns false after printing an error if a
// response file includes itself, the files are nested too deeply, or clang
// would read them with Windows rules.
bool expand(int &argc, char **&argv, Syntax syntax, bool &expanded);

// Like process::execProgram(), but passes args[1..] in a temporary
// response file, so that a long command line stays within the system's
// limits for arguments. The file is deleted after the program ended. Only
// returns if the program could not be started, and then returns 1.
int execProgram(const std::string &path,
                const std::vector<std::string> &args);

} // namespace responsefile
} // namespace tools

#endif // OSXCROSS_WRAPPER_TOOLS_RESPONSE_FILE_H
