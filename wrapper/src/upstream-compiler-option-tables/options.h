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

#ifndef OSXCROSS_WRAPPER_UPSTREAM_COMPILER_OPTION_TABLES_OPTIONS_H
#define OSXCROSS_WRAPPER_UPSTREAM_COMPILER_OPTION_TABLES_OPTIONS_H

#include <vector>

#include "tools/tools.h"

namespace target {
struct Target;
} // namespace target

namespace commandopts {

// Called for an option of the wrapper (see main.cpp) with its name, its
// value and the arguments after it.
typedef bool (*OptFun)(target::Target &, const char *opt, const char *val,
                       char **args);

typedef tools::OptParser<OptFun> Parser;

// The options of Clang and GCC that take a value in the following
// argument(s). The wrapper does nothing with them; it only needs them to
// skip their values while parsing (see options.cpp).
extern const std::vector<Parser::Option> PassthroughCompilerOptions;

} // namespace commandopts

#endif // OSXCROSS_WRAPPER_UPSTREAM_COMPILER_OPTION_TABLES_OPTIONS_H
