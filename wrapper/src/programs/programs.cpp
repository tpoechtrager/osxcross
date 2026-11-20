/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2025-2026 by Thomas Poechtrager                      *
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

#include <string>

#include "programs/programs.h"

namespace program {

namespace {

int dummy(int, char **, Target &) { return 0; }

constexpr Prog Programs[] = {
  // Built-in tools
  { "sw_vers",          macos::sw_vers },
  { "xcrun",            xcode::xcrun },
  { "xcodebuild",       xcode::xcodebuild },

  // LLVM tools

  // These are LLVM tools where we must modify the passed arguments to it.
  { "ld",               llvm::ld },
  { "lipo",             llvm::lipo },
  { "as",               llvm::clang::as },

  // Used 'as-is', besides making them believe
  // they are invoked as "llvm-<tool>" instead of "<tool>".
  { "otool",            "llvm-otool" },

  { "nm",               "llvm-nm" },
  { "ar",               "llvm-ar" },
  { "libtool",          "llvm-libtool-darwin" },
  { "install_name_tool","llvm-install-name-tool" },
  { "ranlib",           "llvm-ranlib" },
  { "readtapi",         "llvm-readtapi" },
  { "objdump",          "llvm-objdump" },
  { "strip",            "llvm-strip" },
  { "strings",          "llvm-strings" },
  { "size",             "llvm-size" },
  { "symbolizer",       "llvm-symbolizer" },
  { "cov",              "llvm-cov" },
  { "profdata",         "llvm-profdata" },
  { "readobj",          "llvm-readobj" },
  { "readelf",          "llvm-readelf" },
  { "dwarfdump",        "llvm-dwarfdump" },
  { "cxxfilt",          "llvm-cxxfilt" },
  { "objcopy",          "llvm-objcopy" },
  { "config",           "llvm-config" },
  { "dis",              "llvm-dis" },
  { "link",             "llvm-link" },
  { "lto",              "llvm-lto" },
  { "lto2",             "llvm-lto2" },
  { "bcanalyzer",       "llvm-bcanalyzer" },
  { "bitcode_strip",    "llvm-bitcode-strip" },

  // OSXCross tools
  { "osxcross",         osxcross::version },
  { "osxcross-env",     osxcross::env },
  { "osxcross-conf",    osxcross::conf },

  // Tools where we must modify the passed arguments to it.
  { "osxcross-man",     host::man },
  { "pkg-config",       host::pkg_config },

  // Dummy tool. No-op.
  { "wrapper",          dummy }
};

} // unnamed namespace

const Prog *getProg(const std::string &name) {
  for (const Prog &p : Programs) {
    if (name == p.name)
      return &p;
  }

  return nullptr;
}

} // namespace program
