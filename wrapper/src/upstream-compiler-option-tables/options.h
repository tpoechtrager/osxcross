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

#include "commandopts.h"

namespace commandopts {

// The options that take a value in the following argument(s): of both
// compilers, of Clang only and of GCC only. The wrapper does nothing with
// them; it only needs them to skip their values while parsing (see
// options.cpp).
extern const std::vector<Parser::Option> PassthroughCompilerOptions;
extern const std::vector<Parser::Option> PassthroughClangOptions;
extern const std::vector<Parser::Option> PassthroughGCCOptions;

} // namespace commandopts

#endif // OSXCROSS_WRAPPER_UPSTREAM_COMPILER_OPTION_TABLES_OPTIONS_H
