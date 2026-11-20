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

#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "tools/tools.h"
#include "tools/plist.h"
#include "target.h"
#include "programs/programs.h"
#include "programs/argument-parser.h"
#include "programs/xcode/sdk.h"

using namespace tools;
using namespace target;

namespace program {
namespace xcode {
namespace {

// Like Apple's xcodebuild, errors in the options exit with 64.
constexpr int UsageError = 64;

// Like Apple's xcodebuild, the options are read first, then the action runs;
// -version comes before -help and -license. Arguments that are not options
// are keys for -version.
const char *sdkOption = nullptr; // nullptr: -sdk not given
std::vector<const char *> keys;  // arguments that are not options

void printXcodeVersion() {
  std::cout << "Xcode 26.6" << std::endl;
  std::cout << "Build version 17F113" << std::endl;
}

int help(Target &, char **) {
  std::cerr << "Only '-version [-sdk <sdk>] [<key>...]' and '-license check' "
               "are supported by this stub tool" << std::endl;
  return 0;
}

// Without an action, print the help and fail.
int usage(Target &target, char **args) {
  // As with Apple's xcodebuild, -sdk without a value needs -version.
  if (sdkOption && !*sdkOption) {
    err << "xcodebuild: -sdk must be supplied with an argument unless used "
           "with -version" << err.endl();
    return UsageError;
  }

  help(target, args);
  return 1;
}

// The string values of the SDK's System/Library/CoreServices/
// SystemVersion.plist, such as ProductBuildVersion, sorted by key. If a key
// repeats, its last value counts, as with Apple's CoreFoundation.
std::map<std::string, std::string>
readSystemVersion(const std::string &sdkPath) {
  std::map<std::string, std::string> values;
  std::string data;
  plist::Value root;

  if (!getFileContent(sdkPath + "/System/Library/CoreServices/"
                                "SystemVersion.plist", data) ||
      !plist::parse(data, root) || !root.isDict())
    return values;

  for (size_t i = 0; i < root.keys.size(); ++i) {
    if (root.values[i].isString())
      values[root.keys[i]] = root.values[i].string;
    else
      values.erase(root.keys[i]);
  }

  return values;
}

// The first line of the block that "-version -sdk <sdk>" prints, as Apple's:
// "MacOSX15.5.sdk - macOS 15.5 (macosx15.5)". The names come from
// Target::readSDKInfo().
std::string sdkHeading(const SDKInfo &sdk) {
  std::string dirName = sdk.path;
  while (dirName.size() > 1 && dirName[dirName.size() - 1] == PathDiv)
    dirName.erase(dirName.size() - 1);

  return std::string(getFileName(dirName)) + " - " + sdk.displayName + " (" +
         sdk.canonicalName + ")";
}

// "-version -sdk <sdk> [<key>...]": print the value of each key, or all of
// them without a key. As with Apple's xcodebuild, an unknown key prints
// nothing.
int sdkInfo(Target &target) {
  if (!determineSDKFromOption(target, sdkOption))
    return UsageError;

  const std::string sdkVersion = target.sdk.version.shortStr();

  // The keys in the order of Apple's block, then those of
  // SystemVersion.plist, sorted.
  std::vector<std::pair<std::string, std::string>> values = {
    {"SDKVersion", sdkVersion},
    {"Path", target.sdk.path},
    {"PlatformVersion", sdkVersion},
    {"PlatformPath", target.execDir + "/.."}
  };

  for (const auto &value : readSystemVersion(target.sdk.path))
    values.push_back(value);

  if (keys.empty()) {
    std::cout << sdkHeading(target.sdk) << std::endl;
    for (const auto &value : values)
      std::cout << value.first << ": " << value.second << std::endl;
    std::cout << std::endl;
    return 0;
  }

  for (const char *key : keys) {
    for (const auto &value : values) {
      if (value.first == key) {
        std::cout << value.second << std::endl;
        break;
      }
    }
  }

  return 0;
}

int version(Target &target, char **) {
  // Without -sdk, -version prints the Xcode version and ignores the keys.
  if (!sdkOption) {
    printXcodeVersion();
    return 0;
  }

  return sdkInfo(target);
}

// args is nullptr for "-license" without a value. Apple's xcodebuild then
// shows the license; here, it is an error.
int license(Target &, char **args) {
  if (!args) {
    err << "xcodebuild: only '-license check' is supported" << err.endl();
    return UsageError;
  }

  if (strcmp(args[0], "check")) {
    err << "xcodebuild: unsupported license action: '" << args[0] << "'"
        << err.endl();
    return UsageError;
  }

  // Succeeds silently, as Apple's xcodebuild does once the license has been
  // accepted.
  return 0;
}

// args is nullptr for "-sdk" without a value. Apple's xcodebuild then prints
// all SDKs; here, it selects the default SDK. As with Apple's xcodebuild,
// only a second -sdk with a value is an error.
int sdk(Target &, char **args) {
  if (!args) {
    if (!sdkOption)
      sdkOption = "";
    return 0;
  }

  if (sdkOption && *sdkOption) {
    err << "xcodebuild: option '-sdk' may only be provided once" << err.endl();
    return UsageError;
  }

  sdkOption = args[0];
  return 0;
}

int addKey(Target &, char **args) {
  keys.push_back(args[0]);
  return 0;
}

} // unnamed namespace

int xcodebuild(int argc, char **argv, Target &target) {
  using Kind = ArgumentParser::Kind;
  using Value = ArgumentParser::Value;

  const ArgumentParser parser = {
    "xcodebuild", UsageError,
    true, // one '-' only, as with Apple's xcodebuild
    {
      {"version", version, Kind::ACTION},
      {"sdk", sdk, Kind::SETTING, Value::OPTIONAL},
      {"license", license, Kind::FALLBACK_ACTION, Value::OPTIONAL},
      {"help", help, Kind::FALLBACK_ACTION},
      {"usage", help, Kind::FALLBACK_ACTION}
    },
    {nullptr, addKey},
    nullptr, usage
  };

  return parser.execute(argc, argv, target);
}

} // namespace xcode
} // namespace program
