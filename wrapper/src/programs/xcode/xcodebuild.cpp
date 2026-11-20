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
#include "target.h"
#include "programs/programs.h"
#include "programs/xcode/sdk.h"

using namespace tools;
using namespace target;

namespace program {
namespace xcode {
namespace {

// Like Apple's xcodebuild, errors in the options exit with 64.
constexpr int UsageError = 64;

// Like Apple's xcodebuild, the options are read first, then the last action
// runs. Arguments that are not options are keys for -version.
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
  help(target, args);
  return 1;
}

// Replace the XML entities in a plist string.
std::string decodeXMLEntities(const std::string &text) {
  constexpr const char *entities[][2] = {
    {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"},
    {"&amp;", "&"}
  };

  std::string result;

  for (size_t i = 0; i < text.size(); ++i) {
    bool replaced = false;

    if (text[i] == '&') {
      for (const auto &entity : entities) {
        const size_t len = strlen(entity[0]);
        if (!text.compare(i, len, entity[0])) {
          result += entity[1];
          i += len - 1;
          replaced = true;
          break;
        }
      }
    }

    if (!replaced)
      result += text[i];
  }

  return result;
}

// The string values of the SDK's System/Library/CoreServices/
// SystemVersion.plist, such as ProductBuildVersion. The file is a flat
// dictionary of strings, so looking for <key> followed by <string> is enough.
std::map<std::string, std::string>
readSystemVersion(const std::string &sdkPath) {
  std::map<std::string, std::string> values;
  std::string text;

  if (!getFileContent(sdkPath + "/System/Library/CoreServices/"
                                "SystemVersion.plist", text))
    return values;

  size_t pos = 0;

  while ((pos = text.find("<key>", pos)) != std::string::npos) {
    pos += strlen("<key>");
    const size_t keyEnd = text.find("</key>", pos);
    if (keyEnd == std::string::npos)
      break;

    const std::string key = text.substr(pos, keyEnd - pos);
    pos = keyEnd + strlen("</key>");

    // Only a <string> right after the key is a value.
    const size_t valueStart = text.find_first_not_of(" \t\r\n", pos);
    if (valueStart == std::string::npos ||
        text.compare(valueStart, strlen("<string>"), "<string>"))
      continue;

    pos = valueStart + strlen("<string>");
    const size_t valueEnd = text.find("</string>", pos);
    if (valueEnd == std::string::npos)
      break;

    values[decodeXMLEntities(key)] =
        decodeXMLEntities(text.substr(pos, valueEnd - pos));
    pos = valueEnd + strlen("</string>");
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

int license(Target &, char **args) {
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
// all SDKs; here, it selects the default SDK.
int sdk(Target &, char **args) {
  if (sdkOption) {
    err << "xcodebuild: option '-sdk' may only be provided once" << err.endl();
    return UsageError;
  }

  sdkOption = args ? args[0] : "";
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
      {"license", license, Kind::ACTION, Value::ONE},
      {"help", help, Kind::ACTION},
      {"usage", help, Kind::ACTION}
    },
    {nullptr, addKey},
    nullptr, usage
  };

  return parser.execute(argc, argv, target);
}

} // namespace xcode
} // namespace program
