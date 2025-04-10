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
 
 using namespace tools;
 
 namespace program {
 
int lipo(int argc, char **argv) {
  (void)argc;

  std::string lipo;
 
  if (getenv("OSXCROSS_FORCE_LLVM_LIPO") || !realPath("osxcross-replacement-lipo", lipo)) {
    lipo = "llvm-lipo";
  }
 
  if (execvp(lipo.c_str(), argv))
    err << "cannot execute '" << lipo << "'" << err.endl();
 
   return 1;
 }
 
 } // namespace program