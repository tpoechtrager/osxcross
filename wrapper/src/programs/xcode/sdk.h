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

#ifndef OSXCROSS_WRAPPER_PROGRAMS_XCODE_SDK_H
#define OSXCROSS_WRAPPER_PROGRAMS_XCODE_SDK_H

#include <string>

#include "target.h"

namespace program {
namespace xcode {

// Determine the SDK for xcrun and xcodebuild with target.determineSDK(),
// from these sources, in this order:
//   1. -sdk, whose value is sdkOption; nullptr if the option was not given
//   2. SDKROOT
//   3. OSXCROSS_SDKROOT
// As with Apple's xcrun, "macosx", "macosx.internal" and "/" select the
// default SDK, and so does an empty -sdk. An empty SDKROOT or "/" counts as
// not set. Any other value is the SDK directory.
bool determineSDK(target::Target &target, const char *sdkOption,
                  bool quiet = false);

// Determine the SDK for xcodebuild, only from -sdk (sdkOption): like Apple's
// xcodebuild, SDKROOT and OSXCROSS_SDKROOT are not read. The values are those
// of -sdk above; in addition, "//" selects the default SDK as well.
bool determineSDKFromOption(target::Target &target, const char *sdkOption,
                            bool quiet = false);

} // namespace xcode
} // namespace program

#endif // OSXCROSS_WRAPPER_PROGRAMS_XCODE_SDK_H
