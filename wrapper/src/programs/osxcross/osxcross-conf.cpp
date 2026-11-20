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

#include <iostream>
#include <string>

#include "tools/tools.h"
#include "target.h"
#include "programs/programs.h"

using namespace tools;
using namespace target;

namespace program {
namespace osxcross {
namespace {

template<typename A>
void print(const char *var, const A &val) {
  std::cout << "export OSXCROSS_" << var << "=" << "\"" << val << "\""
            << std::endl;
}

} // unnamed namespace

int conf(int, char **, Target &target) {
  OSVersion osxVersionMin = getDefaultDeploymentTarget();
  const char *ltoPath = getLibLTOPath();
  std::string buildDir = getBuildDir();

  if (buildDir.empty()) {
    buildDir += target.execDir;
    buildDir += "/../../build";
  }

  if (!target.determineSDK(target.compilerSDKSources()))
    return 1;

  const std::string &sdkPath = target.sdk.path;

  if (!osxVersionMin.num())
    osxVersionMin = target.sdk.version;

  if (!ltoPath)
    ltoPath = "";

  print("VERSION", getOSXCrossVersion());
  print("BUILD_FLAVOR", getBuildFlavor());
  print("DEFAULT_DEPLOYMENT_TARGET", osxVersionMin.shortStr());
  // The old name of DEFAULT_DEPLOYMENT_TARGET. Scripts outside OSXCross may
  // still read it: keep it, do not remove it.
  print("OSX_VERSION_MIN", osxVersionMin.shortStr());
  print("TARGET", getDefaultTarget());
  print("BASE_DIR", buildDir + "/..");
  print("SDK", sdkPath);
  print("SDK_DIR", sdkPath + "/..");
  print("SDK_VERSION", target.sdk.version.shortStr());
  print("TARBALL_DIR", buildDir + "/../tarballs");
  print("PATCH_DIR", buildDir + "/../patches");
  print("TARGET_DIR", std::string(target.execDir) + "/..");
  print("DIR_SDK_TOOLS", sdkPath + "/../tools");
  print("BUILD_DIR", buildDir);
  print("CCTOOLS_PATH", target.execDir);
  print("LIBLTO_PATH", ltoPath);
  print("LINKER_VERSION", getLinkerVersion());
  print("SUPPORTED_ARCHS", getSupportedArchsString());
  print("GCC_TARGET_ARCHS", getSupportedArchsString(true));
  print("DEFAULT_ARCH", getArchName(getDefaultArch()));

  return 0;
}

} // namespace osxcross
} // namespace program
