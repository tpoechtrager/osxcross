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

#include <cstdlib>
#include <string>
#include <vector>

#include "target.h"
#include "programs/xcode/sdk.h"

using target::Target;

namespace program {
namespace xcode {
namespace {

// -sdk: these values select the default SDK, any other value is the SDK
// directory, even if it is relative or does not exist.
Target::SDKSource::Use checkSDK(const std::string &value) {
  if (value == "macosx" || value == "macosx.internal" || value == "/" ||
      value.empty())
    return Target::SDKSource::Use::DEFAULT_SDK;
  return Target::SDKSource::Use::PATH;
}

std::vector<Target::SDKSource> xcodeSDKSources(const char *sdkOption) {
  // SDKROOT: an empty value and "/" count as not set; otherwise as -sdk.
  auto checkSDKRoot = [](const std::string &value) {
    if (value.empty() || value == "/")
      return Target::SDKSource::Use::SKIP;
    if (value == "macosx" || value == "macosx.internal")
      return Target::SDKSource::Use::DEFAULT_SDK;
    return Target::SDKSource::Use::PATH;
  };

  return {{"-sdk", sdkOption, checkSDK},
          {"SDKROOT", getenv("SDKROOT"), checkSDKRoot},
          {"OSXCROSS_SDKROOT", getenv("OSXCROSS_SDKROOT")}};
}

} // unnamed namespace

bool determineSDK(Target &target, const char *sdkOption, bool quiet) {
  return target.determineSDK(xcodeSDKSources(sdkOption), quiet);
}

bool determineSDKFromOption(Target &target, const char *sdkOption,
                            bool quiet) {
  // xcodebuild: as -sdk for xcrun, but "//" selects the default SDK, too.
  auto checkXcodebuildSDK = [](const std::string &value) {
    if (value == "//")
      return Target::SDKSource::Use::DEFAULT_SDK;
    return checkSDK(value);
  };

  return target.determineSDK({{"-sdk", sdkOption, checkXcodebuildSDK}},
                             quiet);
}

} // namespace xcode
} // namespace program
