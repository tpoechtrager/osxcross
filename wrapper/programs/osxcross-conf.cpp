/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2016 by Thomas Poechtrager                      *
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
using namespace target;

namespace program {
namespace osxcross {

template<typename A>

void print(const char *var, const A &val) {
  std::cout << "export OSXCROSS_" << var << "=" << val << std::endl;
};

int conf(Target &target) {
  std::string SDKPath;
  OSVersion OSXVersionMin = getDefaultMinTarget();
  const char *ltopath = getLibLTOPath();
  std::string BuildDir = getBuildDir();

  if (BuildDir.empty()) {
    BuildDir += target.execpath;
    BuildDir += "/../../build";
  }

  if (!target.getSDKPath(SDKPath))
    return 1;

  if (!OSXVersionMin.Num())
    OSXVersionMin = target.getSDKOSNum();

  if (!ltopath)
    ltopath = "";

  print("VERSION", getOSXCrossVersion());
  print("OSX_VERSION_MIN", OSXVersionMin.shortStr());
  print("TARGET", getDefaultTarget());
  print("BASE_DIR", BuildDir + "/..");
  print("SDK", SDKPath);
  print("SDK_DIR", SDKPath + "/..");
  print("SDK_VERSION", target.getSDKOSNum().shortStr());
  print("TARBALL_DIR", BuildDir + "/../tarballs");
  print("PATCH_DIR", BuildDir + "/../patches");
  print("TARGET_DIR", std::string(target.execpath) + "/..");
  print("DIR_SDK_TOOLS", SDKPath + "/../tools");
  print("BUILD_DIR", BuildDir);
  print("CCTOOLS_PATH", target.execpath);
  print("LIBLTO_PATH", ltopath);
  print("LINKER_VERSION", getLinkerVersion());

  return 0;
}

} // namespace osxcross
} // namespace program
