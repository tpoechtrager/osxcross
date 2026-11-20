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

#ifndef OSXCROSS_WRAPPER_TOOLS_PROCESS_H
#define OSXCROSS_WRAPPER_TOOLS_PROCESS_H

#include <string>
#include <vector>

namespace tools {
namespace process {

// Execute path (looked up in PATH if it contains no slash) with args;
// args[0] is the name the program sees as argv[0]. Only returns on failure,
// after printing an error message, and then returns 1.
int execProgram(const std::string &path,
                const std::vector<std::string> &args);

// Same, with argv[1] to argv[argc - 1] as arguments; the program sees path
// as argv[0].
int execProgram(const std::string &path, int argc, char **argv);

// Debug output: the original command line (argv) and the one passed to
// execProgram() (args).
void printExecProgramArgs(int argc, char **argv,
                          const std::vector<std::string> &args);

// Runs args[0] (looked up in PATH unless it contains a '/') with args, without
// a shell, and stores its standard output in output. stdin and stderr are
// redirected to /dev/null. Returns true if the program exited with status 0.
bool getProgramOutput(const std::vector<std::string> &args,
                      std::string &output);

} // namespace process
} // namespace tools

#endif // OSXCROSS_WRAPPER_TOOLS_PROCESS_H
