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

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include <strings.h>
#include <unistd.h>

#include "tools/tools.h"
#include "tools/process.h"
#include "tools/json.h"
#include "tools/plist.h"
#include "target.h"

namespace target {

std::vector<Arch> getSupportedArchs(bool gcc) {
  std::vector<Arch> result;
  std::istringstream iss(getSupportedArchsString(gcc));
  std::string token;

  while (iss >> token)
    result.push_back(parseArch(token.c_str()));

  return result;
}

Arch getDefaultArch(bool gcc) {
  std::istringstream iss(getSupportedArchsString(gcc));
  std::string first;
  iss >> first;
  return parseArch(first.c_str());
}

// The macOS version in the name of an SDK directory, for SDKs without
// SDKSettings.json: 15.4 for "/path/to/MacOSX15.4.sdk"; 0.0 if the name
// does not start with "MacOSX".
static OSVersion parseSDKVersionFromDirName(std::string sdkPath) {
  // Ignore trailing slashes, as in "MacOSX15.4.sdk/".
  while (!sdkPath.empty() && sdkPath.back() == PathDiv)
    sdkPath.pop_back();

  // MacOSX.sdk may be a symlink to a versioned SDK directory.
  // Resolve it only to determine the version, preserving the selected SDK.
  if (!strcasecmp(getFileName(sdkPath), "MacOSX.sdk")) {
    char *resolved = realpath(sdkPath.c_str(), nullptr);
    if (resolved) {
      sdkPath = resolved;
      free(resolved);
    }
  }

  const char *sdkName = getFileName(sdkPath);

  if (strncasecmp(sdkName, "MacOSX", 6))
    return OSVersion();

  return parseOSVersion(sdkName + 6);
}

// The macOS version that belongs to a Darwin target, 15.4 for "darwin24.4".
static OSVersion getMacOSVersion(const std::string &darwinTarget) {
  // The Darwin version follows "darwin", for example "24.4" in "darwin24.4".
  constexpr size_t darwinLen = constexprStrLen("darwin");
  if (darwinTarget.size() <= darwinLen) return OSVersion();
  const char *darwinVersion = darwinTarget.c_str() + darwinLen;

  const OSVersion darwin = parseOSVersion(darwinVersion);
  const int major = darwin.major;
  const int minor = darwin.minor;

  // Darwin 27 and later correspond directly to macOS 27 and later.
  if (major >= 27)
    return OSVersion(major, minor);

  // Darwin 25 corresponds to macOS 26.
  // Darwin 26 was skipped.
  if (major >= 25)
    return OSVersion(major + 1, minor);

  // MacOS 11-15 (Darwin 20-24)
  if (major >= 20) {
    // Adjust for early versions where the minor version was offset by -1

    // Darwin 23.0 => macOS 14.0
    // Darwin 23.1 => macOS 14.1

    // Darwin 22.1 => macOS 13.0
    // Darwin 22.2 => macOS 13.1

    // ...

    // Darwin 22 and 22.0, as in a program name such as
    // x86_64-apple-darwin22-clang, are macOS 13.0 as well.
    return OSVersion(11 + (major - 20),
                     major < 23 && minor > 0 ? minor - 1 : minor);
  }

  // MacOS 10 (Darwin 4 => 10.0, ..., Darwin 19 => 10.15)
  return OSVersion(10, major - 4);
}

// Where a value of the SDK comes from, as text for the debug output.
static const char *getSDKValueSource(SDKInfo::From from) {
  switch (from) {
  case SDKInfo::From::SETTINGS_JSON:
    return "SDKSettings.json";
  case SDKInfo::From::SETTINGS_PLIST:
    return "SDKSettings.plist";
  case SDKInfo::From::DIRECTORY_NAME:
    return "SDK directory name";
  case SDKInfo::From::SDK_VERSION:
    return "SDK version";
  case SDKInfo::From::UNKNOWN_FROM:
    break;
  }

  return "unknown";
}

// path, made absolute with the current directory if it is relative.
// Symlinks are kept: for SDKs without SDKSettings.json the version comes
// from the directory name.
static std::string makeAbsolutePath(const std::string &path) {
  if (path.empty() || path[0] == PathDiv)
    return path;

  char cwd[PATH_MAX];

  if (!getcwd(cwd, sizeof(cwd)))
    return path;

  return std::string(cwd) + PathDiv + path;
}

Target::Target()
    : vendor(getDefaultVendor()), supportedArchs(getSupportedArchs()),
      targetOS(getDefaultTarget()), primaryArch(getDefaultArch()),
      compiler(getDefaultCompilerIdentifier()),
      compilerName(getDefaultCompilerName()), skipIntrinsicHeaders(),
      stdlib(Stdlib::UNSET), useGCCLibs(), language(), linking(true),
      useResponseFile() {
  execDir = getExecutableDir();

  if (execDir.empty()) {
    err << "cannot determine the directory of the osxcross executable"
        << err.endl();
    exit(EXIT_FAILURE);
  }

  // OSXCROSS_SDK_SEARCH_DIR was removed; tell those who still set it.
  const char *sdkSearchDir = getenv("OSXCROSS_SDK_SEARCH_DIR");

  if (sdkSearchDir && sdkSearchDir[0]) {
    err << "OSXCROSS_SDK_SEARCH_DIR is no longer supported; "
           "set OSXCROSS_SDKROOT to the SDK directory" << err.endl();
    exit(EXIT_FAILURE);
  }
}

std::vector<Target::SDKSource>
Target::compilerSDKSources(std::vector<SDKSource> extra) const {
  // The compiler's options. In the other tools, such as the ld shim or
  // osxcross-conf, they are always empty and therefore skipped.
  std::vector<SDKSource> sources = {{"-isysroot", commandLineIsysroot},
                                    {"--sysroot", commandLineSysroot}};

  sources.insert(sources.end(), extra.begin(), extra.end());

  // As with Apple's clang, SDKROOT is used only if it is an absolute path to
  // an existing directory other than "/"; anything else is ignored.
  auto checkSDKRoot = [](const std::string &value) {
    if (value[0] == PathDiv && value[1] && dirExists(value))
      return SDKSource::Use::PATH;
    return SDKSource::Use::SKIP;
  };

  sources.push_back({"SDKROOT", getenv("SDKROOT"), checkSDKRoot});
  sources.push_back({"OSXCROSS_SDKROOT", getenv("OSXCROSS_SDKROOT")});
  return sources;
}

SDKInfo Target::readSDKInfo(const std::string &path, const char *from) {
  using From = SDKInfo::From;

  SDKInfo sdkInfo;
  std::string settingsText;
  json::Value settings;

  sdkInfo.path = path;
  sdkInfo.from = from;

  if (getFileContent(path + "/SDKSettings.json", settingsText) &&
      json::parse(settingsText, settings)) {
    // "Version" is exact: MacOSX15.sdk can be 15.5. Versions are read
    // strictly; an invalid one such as "10.9beta" counts as not given.
    if (settings["Version"].isString())
      sdkInfo.version.set(
          parseOSVersion(settings["Version"].string.c_str(), /*strict=*/true),
          From::SETTINGS_JSON);

    // Minimum deployment target, e.g. "12.0" for SDK 27.3.
    const json::Value &minTarget =
        settings["SupportedTargets"]["macosx"]["MinimumDeploymentTarget"];

    if (minTarget.isString())
      sdkInfo.minDeploymentTarget.set(
          parseOSVersion(minTarget.string.c_str(), /*strict=*/true),
          From::SETTINGS_JSON);

    // Maximum deployment target, e.g. "27.3.99" for SDK 27.3.
    const json::Value &maxTarget = settings["MaximumDeploymentTarget"];

    if (maxTarget.isString())
      sdkInfo.maxDeploymentTarget.set(
          parseOSVersion(maxTarget.string.c_str(), /*strict=*/true),
          From::SETTINGS_JSON);

    if (settings["DisplayName"].isString())
      sdkInfo.displayName.set(settings["DisplayName"].string,
                              From::SETTINGS_JSON);

    if (settings["CanonicalName"].isString())
      sdkInfo.canonicalName.set(settings["CanonicalName"].string,
                                From::SETTINGS_JSON);
  }

  // SDKs before 11.0 have no SDKSettings.json, but their SDKSettings.plist
  // states the version. Its MaximumDeploymentTarget is not used: up to SDK
  // 10.12 it has no patch level ("10.6" instead of 10.6.99), so patch
  // releases such as 10.6.8 would be rejected.
  if (!sdkInfo.version.num()) {
    std::string plistData;
    plist::Value plistSettings;

    if (getFileContent(path + "/SDKSettings.plist", plistData) &&
        plist::parse(plistData, plistSettings) &&
        plistSettings["Version"].isString())
      sdkInfo.version.set(
          parseOSVersion(plistSettings["Version"].string.c_str(),
                         /*strict=*/true),
          From::SETTINGS_PLIST);
  }

  // If neither file provides a version, use the name of the SDK directory
  // as fallback.
  if (!sdkInfo.version.num())
    sdkInfo.version.set(parseSDKVersionFromDirName(path),
                        From::DIRECTORY_NAME);

  // SDKs since 11.0 state the maximum as their version with patch level 99
  // (27.3.99), so that any patch release is accepted. SDKs without a maximum
  // get the same.
  if (!sdkInfo.maxDeploymentTarget.num() && sdkInfo.version.num())
    sdkInfo.maxDeploymentTarget.set(
        OSVersion(sdkInfo.version.major, sdkInfo.version.minor, 99),
        From::SDK_VERSION);

  // Names as Apple's SDKs have them, for SDKs that do not state them.
  if (sdkInfo.displayName.empty())
    sdkInfo.displayName.set("macOS " + sdkInfo.version.shortStr(),
                            From::SDK_VERSION);

  if (sdkInfo.canonicalName.empty())
    sdkInfo.canonicalName.set("macosx" + sdkInfo.version.shortStr(),
                              From::SDK_VERSION);

  return sdkInfo;
}

bool Target::determineSDK(const std::vector<SDKSource> &sources, bool quiet) {
  //
  // SDK path
  //

  std::string path;
  const char *from = "OSXCross"; // unless a source gives the directory
  bool fromSource = false;       // a source gave the directory

  for (const SDKSource &source : sources) {
    if (!source.given)
      continue;

    SDKSource::Use use;

    if (source.check)
      use = source.check(source.value);
    else
      use = source.value.empty() ? SDKSource::Use::SKIP : SDKSource::Use::PATH;

    switch (use) {
    case SDKSource::Use::SKIP:
      if (getDebugLevel() >= 2 && !source.value.empty())
        dbg << source.option << "=" << source.value << " ignored"
            << dbg.endl();
      continue; // the next source
    case SDKSource::Use::DEFAULT_SDK:
      break;
    case SDKSource::Use::PATH:
      path = source.value;
      from = source.option;
      fromSource = true;
      break;
    }

    break; // this source decides
  }

  if (path.empty()) {
    // The SDK that's installed by build.sh. It is named after the macOS
    // version of the target; the 10.4 SDK is named MacOSX10.4u.sdk
    // (universal), the SDK for macOS 15.0 may also be named MacOSX15.sdk.
    const OSVersion targetVersion = getMacOSVersion(targetOS);
    const std::string prefix = execDir + "/../SDK/MacOSX";
    const char *suffix = targetVersion <= OSVersion(10, 4) ? "u.sdk" : ".sdk";

    path = prefix + targetVersion.shortStr() + suffix;

    if (!dirExists(path) && targetVersion.minor == 0)
      path = prefix + targetVersion.majorStr() + suffix;
  }

  //
  // SDK version, deployment targets and names (SDKSettings.json, with
  // fallbacks)
  //

  sdk = readSDKInfo(path, from);

  //
  // Debug output
  //

  if (getDebugLevel() >= 2) {
    dbg << (toolName.empty() ? "" : toolName + ": ") << "SDK " << sdk.path
        << " (from " << sdk.from << ")" << dbg.endl();

    // The SDK has the file, but the version does not come from it.
    if (fileExists(sdk.path + "/SDKSettings.json") &&
        sdk.version.from != SDKInfo::From::SETTINGS_JSON)
      dbg << "  SDKSettings.json is not readable, not valid JSON or has no "
          << "valid \"Version\"" << dbg.endl();

    // Name, value and source of each value, in aligned columns.
    struct Row {
      const char *name;
      std::string value;
      const char *source;
    };

    std::vector<Row> rows;

    if (sdk.version.num())
      rows.push_back({"version", sdk.version.shortStr(),
                      getSDKValueSource(sdk.version.from)});
    else
      rows.push_back({"version", "unknown",
                      "not in SDKSettings.json, SDKSettings.plist or the "
                      "directory name"});

    if (sdk.minDeploymentTarget.num())
      rows.push_back({"min deployment target",
                      sdk.minDeploymentTarget.displayStr(),
                      getSDKValueSource(sdk.minDeploymentTarget.from)});

    if (sdk.maxDeploymentTarget.num())
      rows.push_back({"max deployment target",
                      sdk.maxDeploymentTarget.displayStr(),
                      getSDKValueSource(sdk.maxDeploymentTarget.from)});

    rows.push_back({"display name", sdk.displayName,
                    getSDKValueSource(sdk.displayName.from)});
    rows.push_back({"canonical name", sdk.canonicalName,
                    getSDKValueSource(sdk.canonicalName.from)});

    size_t nameWidth = 0;
    size_t valueWidth = 0;

    for (const Row &row : rows) {
      nameWidth = std::max(nameWidth, strlen(row.name));
      valueWidth = std::max(valueWidth, row.value.size());
    }

    for (const Row &row : rows) {
      dbg << "  " << row.name
          << std::string(nameWidth - strlen(row.name) + 2, ' ') << row.value
          << std::string(valueWidth - row.value.size() + 2, ' ')
          << row.source << dbg.endl();
    }
  }

  //
  // Does the SDK actually exist?
  //

  if (!dirExists(sdk.path)) {
    if (!quiet) {
      err << "cannot find macOS SDK (expected in: " << sdk.path;
      if (fromSource)
        err << ", from " << sdk.from;
      err << ")" << err.endl();
    }
    return false;
  }

  return true;
}

void Target::passSDKToChildProcesses() const {
  setenv("SDKROOT", makeAbsolutePath(sdk.path).c_str(), 1);
  unsetenv("OSXCROSS_SDKROOT");
}

bool Target::getMacPortsDir(std::string &path) const {
  path = execDir;
  path += "/../macports";
  return dirExists(path);
}

bool Target::getMacPortsSysRootDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs";
  return dirExists(path);
}

bool Target::getMacPortsPkgConfigDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/lib/pkgconfig";
  return dirExists(path);
}

bool Target::getMacPortsIncludeDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/include";
  return dirExists(path);
}

bool Target::getMacPortsLibDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/lib";
  return dirExists(path);
}

bool Target::getMacPortsFrameworksDir(std::string &path) const {
  if (!getMacPortsDir(path))
    return false;

  path += "/pkgs/opt/local/Library/Frameworks";
  return dirExists(path);
}

bool Target::archSupported(const Arch arch) {
  return std::find(supportedArchs.begin(), supportedArchs.end(), arch) !=
         supportedArchs.end();
}

bool Target::checkArchs() {
  for (auto a : archs) {
    if (!archSupported(a)) {
      err << "architecture '" << getArchName(a) << "' is not supported!"
          << err.endl();
      return false;
    }
  }

  return true;
}

void Target::addArch(const Arch arch) {
  auto &v = archs;
  for (size_t i = 0; i < v.size(); ++i) {
    if (v[i] == arch) {
      v.erase(v.begin() + i);
      addArch(arch);
      return;
    }
  }
  v.push_back(arch);
}

bool Target::haveArch(const Arch arch) {
  for (auto a : archs) {
    if (arch == a)
      return true;
  }
  return false;
}

bool Target::hasLibCXX() const { return sdk.version >= OSVersion(10, 7); }

bool Target::libCXXIsDefaultCXXLib() const {
  OSVersion deploymentTarget = this->deploymentTarget;

  if (!deploymentTarget.num())
    deploymentTarget = sdk.version;

  return stdlib != Stdlib::LIBSTDCXX && hasLibCXX() && !isGCC() &&
         (deploymentTarget >= OSVersion(10, 9) ||
          sdk.version >= OSVersion(10, 14));
}

bool Target::isCXX() {
  if (isKnownCompiler())
    return (compiler == Compiler::CLANGXX || compiler == Compiler::GXX);

  return endsWith(compilerName, "++");
}

bool Target::isGCH() {
  if (!language)
    return false;

  return !strcmp(language, "c-header") ||
         !strcmp(language, "c++-header") ||
         !strcmp(language, "objective-c-header") ||
         !strcmp(language, "objective-c++-header");
}


bool Target::isClang() const { return tools::isClang(compiler); }

bool Target::isGCC() const { return tools::isGCC(compiler); }

bool Target::isKnownCompiler() const {
  return tools::isKnownCompiler(compiler);
}


const std::string &Target::buildDefaultTriple(std::string &triple,
  bool gcc, bool useAarch64InsteadOfArm64) const {
  triple = makeTriple(getDefaultArch(gcc), getDefaultVendor(),
                      getDefaultTarget(), useAarch64InsteadOfArm64);
  return triple;
}

std::string Target::buildToolPath(const std::string &tool) const {
  return buildToolPath(triple, tool);
}

std::string Target::buildToolPath(const std::string &toolTriple,
                                  const std::string &tool) const {
  std::string path = execDir;
  path += PathDiv;
  path += toolTriple;
  path += "-";
  path += tool;
  return path;
}

void Target::setCompilerPath() {
  if (isGCC()) {
    compilerPath = buildToolPath("base-" + compilerName);
    compilerArgv0 = getTriple() + "-" + compilerName;
  } else {
    if (!compilerPath.empty()) {
      compilerPath += "/";
      compilerPath += compilerName;
      compilerArgv0 = compilerName;
    } else {
      compilerPath = findExecutableInPath(compilerName, true, ignoreCCACHE);

      if (compilerPath.empty())
        compilerPath = compilerName;

      compilerArgv0 += compilerName;
    }
  }
}

// Returns the clang version from the name of a resource directory, e.g. 18
// for lib/clang/18 or 15.0.7 for lib/clang/15.0.7. Other names such as
// 64bit give 0.0 and are ignored; parseClangVersion() alone would read
// 64bit as clang 64.
static ClangVersion parseClangVersionFromDirName(const char *name) {
  if (*name < '0' || *name > '9' ||
      strspn(name, "0123456789.") != strlen(name))
    return ClangVersion();

  return parseClangVersion(name);
}

bool Target::findClangIntrinsicHeadersAndVersion(std::string &path) {
  assert(isClang());

  auto warnHeadersNotFound = [](const std::string &reason) {
    warn << "cannot find clang intrinsic headers or determine the clang "
            "version: " << reason << warn.endl();
    return false;
  };

  auto warnVersionNotFound = [](const std::string &reason) {
    warn << "cannot determine the clang version: " << reason << warn.endl();
  };

  if (compilerPath.empty())
    return warnHeadersNotFound("compiler path is empty");

  static constexpr const char *IntrinHeader = "xmmintrin.h";

  std::string clangBinDir = compilerPath;
  stripFileName(clangBinDir);

  // Every successful lookup ends here.
  auto found = [&](const std::string &intrinDir, const ClangVersion &cv) {
    path = intrinDir;
    clangVersion = cv;
    return true;
  };

  // Returns <dir>/include or <dir>, whichever contains xmmintrin.h, or an
  // empty string.
  auto findHeaderDir = [](const std::string &dir) -> std::string {
    for (const std::string &d : { dir + "/include", dir }) {
      if (fileExists(d + "/" + IntrinHeader))
        return d;
    }

    return std::string();
  };

  // Looks for <dir>/<version>/include/xmmintrin.h or
  // <dir>/<version>/xmmintrin.h; the highest version wins.
  auto tryDir = [&](const std::string &dir) {
    ClangVersion bestVersion;
    std::string bestPath;

    for (const std::string &file : listFiles(dir)) {
      if (file[0] == '.' || !dirExists(dir + "/" + file))
        continue;

      const ClangVersion cv = parseClangVersionFromDirName(file.c_str());

      if (cv == ClangVersion())
        continue;

      const std::string intrinDir = findHeaderDir(dir + "/" + file);

      if (!intrinDir.empty() && cv > bestVersion) {
        bestVersion = cv;
        bestPath = intrinDir;
      }
    }

    if (bestVersion == ClangVersion())
      return false;

    return found(bestPath, bestVersion);
  };

  std::vector<std::string> dirs;

  // -foc-intrinsic-path is an explicit choice, so it is checked first. It
  // has always been appended to the clang bin directory as it is; that
  // stays, and an absolute path or a relative path without a leading '/'
  // is tried as well.
  if (!intrinsicPathOption.empty()) {
    dirs.push_back(clangBinDir + intrinsicPathOption);

    if (intrinsicPathOption[0] == '/')
      dirs.push_back(intrinsicPathOption);
    else
      dirs.push_back(clangBinDir + "/" + intrinsicPathOption);
  }

  dirs.push_back(clangBinDir + "/../lib/clang");

#ifdef __linux__
#ifdef __x86_64__
  // opensuse uses lib64 instead of lib on x86_64
  dirs.push_back(clangBinDir + "/../lib64/clang");
  // Fedora installs clang into /usr/lib64/llvm*
  dirs.push_back(clangBinDir + "/../../../lib/clang/");
  // Gentoo installs clang into /usr/lib/llvm/version
  dirs.push_back(clangBinDir + "/../../../clang/");
#elif __i386__
  dirs.push_back(clangBinDir + "/../lib32/clang");
#endif
#endif

  dirs.push_back(clangBinDir + "/../include/clang");
  dirs.push_back(clangBinDir + "/usr/include/clang");

  // OSXCROSS_FIND_INTRINSICS_VIA_PRINT_RESOURCE_DIR skips the guesses and
  // goes straight to the last resort below (for tests).
  const bool forceResourceDir =
      getenv("OSXCROSS_FIND_INTRINSICS_VIA_PRINT_RESOURCE_DIR") != nullptr;

  if (!forceResourceDir) {
    for (const std::string &dir : dirs) {
      if (tryDir(dir))
        return true;
    }
  }

  // Last resort: ask the compiler for its resource directory. This covers
  // layouts the guesses above do not know, e.g. NixOS, where bin/clang is a
  // wrapper script in a different store path than the headers. It is only
  // the last resort because it starts clang as a separate process (twice if
  // the version has to come from --version), which is much slower than
  // looking at a few directories, and this runs on every wrapper call.
  if (getDebugLevel() > 0) {
    dbg << (forceResourceDir
                ? "OSXCROSS_FIND_INTRINSICS_VIA_PRINT_RESOURCE_DIR is set"
                : "clang intrinsic headers not found in the known directories; "
                  "clang version could not be determined from them")
        << "; asking '" << compilerPath << " -print-resource-dir'"
        << dbg.endl();
  }

  std::string resourceDir;

  if (!process::getProgramOutput({compilerPath, "-print-resource-dir"},
                                 resourceDir))
    return warnHeadersNotFound("'" + compilerPath +
                               " -print-resource-dir' failed");

  while (!resourceDir.empty() &&
         (resourceDir.back() == '\n' || resourceDir.back() == '\r'))
    resourceDir.pop_back();

  if (resourceDir.empty())
    return warnHeadersNotFound("'" + compilerPath +
                               " -print-resource-dir' returned an empty "
                               "resource directory");

  const std::string intrinDir = findHeaderDir(resourceDir);

  if (intrinDir.empty())
    return warnHeadersNotFound("resource directory '" + resourceDir +
                               "' returned by '" + compilerPath +
                               " -print-resource-dir' contains no "
                               "intrinsic headers");

  // The resource directory is usually <prefix>/lib/clang/<version>.
  ClangVersion cv = parseClangVersionFromDirName(getFileName(resourceDir));

  if (cv == ClangVersion()) {
    // No version in the path (NixOS: .../resource-root), so parse
    // "... clang version X.Y.Z ..." or "Apple LLVM version X.Y.Z" from
    // --version. -dumpversion is not used because Clang < 14 prints 4.2.1.
    static const char *const Needles[] = {
      "clang version ", "Apple LLVM version "
    };
    std::string version;

    if (!process::getProgramOutput({compilerPath, "--version"}, version)) {
      warnVersionNotFound("'" + compilerPath + " --version' failed");
    } else {
      for (const char *needle : Needles) {
        const size_t pos = version.find(needle);

        if (pos != std::string::npos) {
          cv = parseClangVersion(version.c_str() + pos + strlen(needle));
          if (cv != ClangVersion())
            break;
        }
      }

      if (cv == ClangVersion())
        warnVersionNotFound("unrecognized output from '" + compilerPath +
                            " --version'");
    }
  }

  return found(intrinDir, cv);
}

// The GCC runtime is linked statically, unless the user asks for the shared
// one.
static bool useStaticGCCRuntime() {
  // The variable does not change while the wrapper runs, so read it once.
  static const bool UseStatic = !getenv("OSXCROSS_GCC_NO_STATIC_RUNTIME");
  return UseStatic;
}

// The OSXCross target directory, where ./build_gcc.sh installs GCC.
std::string Target::gccInstallDir() const {
  return execDir + "/..";
}

// Links the libstdc++ and libgcc installed by ./build_gcc.sh (clang++-gstdc++)
// instead of the default libraries:
//
//   <gccInstallDir>/<gccTriple>/lib[/i386]                libstdc++, libsupc++
//   <gccInstallDir>/lib/gcc/<gccTriple>/<version>[/i386]  libgcc, libemutls_w
//
// The i386 libraries are a multilib of the x86_64 GCC installation.
void Target::addGCCRuntimeLibs(Arch arch) {
  assert(stdlib == Stdlib::LIBSTDCXX && useGCCLibs);
  assert(!gccTriple.empty());

  const std::string multilib = arch == Arch::I386 ? "/i386" : "";
  const std::string libgccDir = gccInstallDir() + "/lib/gcc/" + gccTriple +
                                "/" + gccVersion.str() + multilib;
  const std::string libstdcxxDir = gccInstallDir() + "/" + gccTriple + "/lib" +
                                   multilib;
  const bool dynamic = !useStaticGCCRuntime();

  wrapperArgs.push_back("-nodefaultlibs");

  if (dynamic) {
    wrapperArgs.push_back("-L");
    wrapperArgs.push_back(libgccDir);
    wrapperArgs.push_back("-L");
    wrapperArgs.push_back(libstdcxxDir);
  }

  auto addLib = [&](const std::string &dir, const char *lib) {
    if (dynamic) {
      wrapperArgs.push_back("-l");
      wrapperArgs.push_back(lib);
    } else {
      wrapperArgs.push_back(dir + "/lib" + lib + ".a");
    }
  };

  wrapperArgs.push_back("-Qunused-arguments");

  addLib(libstdcxxDir, "stdc++");
  addLib(libstdcxxDir, "supc++");
  addLib(libgccDir, "gcc");
  // Fix #484: use the system unwinder with modern Darwin GCC runtimes.
  // https://github.com/tpoechtrager/osxcross/issues/484
  // Older GCC installations do not provide the weak emulated-TLS archive.
  const bool useSystemUnwinder = fileExists(libgccDir + "/libemutls_w.a");
  if (useSystemUnwinder) {
    addLib(libgccDir, "emutls_w");
    // Heap trampoline support is not present in every GCC version.
    if (fileExists(libgccDir + "/libheapt_w.a"))
      addLib(libgccDir, "heapt_w");
  } else {
    addLib(libgccDir, "gcc_eh");
  }

  wrapperArgs.push_back("-lc");

  // Fix #484: Clang can emit compact-unwind-only frames.
  // Preserve them for the system unwinder; the legacy GCC unwinder needs
  // DWARF instead. ld64.lld does not implement -no_compact_unwind.
  if (!useSystemUnwinder && !buildFlavor.isLLVM())
    wrapperArgs.push_back("-Wl,-no_compact_unwind");
}

std::string makeTriple(Arch arch, const char *vendor, const std::string &target,
                       bool useAarch64InsteadOfArm64) {
  if (arch == Arch::ARM64 && useAarch64InsteadOfArm64)
    arch = Arch::AARCH64;

  std::string triple = getArchName(arch);
  triple += "-";
  triple += vendor;
  triple += "-";
  triple += target;
  return triple;
}

void Target::setTriple(bool useAarch64InsteadOfArm64) {
  triple = makeTriple(primaryArch, vendor, targetOS, useAarch64InsteadOfArm64);
}

bool Target::setup() {
  //
  // Architectures, SDK, triple and compiler
  //

  if (archs.empty())
    addArch(primaryArch);

  if (!checkArchs())
    return false;

  if (!isKnownCompiler())
    warn << "unknown compiler '" << compilerName << "'" << warn.endl();

  if (!determineSDK(compilerSDKSources()))
    return false;

  // The checks below compare against the SDK version; with an unknown one
  // (0.0) they would fail with misleading messages.
  if (!sdk.version.num()) {
    err << "cannot determine the version of the macOS SDK '" << sdk.path
        << "' (neither SDKSettings.json, SDKSettings.plist nor the "
        << "directory name has one)"
        << err.endl();
    return false;
  }

  setTriple();
  setCompilerPath();

  // SDK versions the architectures need: i386 requires the 10.13 SDK or an
  // earlier one (orEarlier), the others a minimum SDK version.
  constexpr struct {
    Arch arch;
    OSVersion sdkVer;
    bool orEarlier;
  } requiredSDKVersion[] = {
    { Arch::I386,    {10, 13}, true },
    { Arch::X86_64H, {10, 8} },
    { Arch::ARM64,   {11, 0} },
    { Arch::ARM64E,  {11, 0} },
  };

  for (auto &requiredSDK : requiredSDKVersion) {
    if (!haveArch(requiredSDK.arch))
      continue;

    const bool sdkSupported = requiredSDK.orEarlier
                                  ? sdk.version <= requiredSDK.sdkVer
                                  : sdk.version >= requiredSDK.sdkVer;

    if (!sdkSupported) {
      err << "Architecture '" << getArchName(requiredSDK.arch) << "' requires "
          << "macOS " << requiredSDK.sdkVer.shortStr() << " SDK "
          << (requiredSDK.orEarlier ? "(or earlier)" : "(or later)")
          << err.endl();
      return false;
    }

  }

  //
  // Deployment target (-mmacosx-version-min)
  //

  if (!deploymentTarget.num()) {
    const OSVersion defaultTarget = getDefaultDeploymentTarget();

    if (defaultTarget != OSVersion()) {
      // Default version is given via -DOSXCROSS_DEFAULT_DEPLOYMENT_TARGET=XX,
      // raised to what the architectures and the C++ library need.
      deploymentTarget = defaultTarget;

      // Default to >= 11.0 for arm64 and >= 10.8 for x86_64h
      for (Arch arch : archs)
        deploymentTarget = std::max(deploymentTarget,
                                    getMinDeploymentTargetForArch(arch));

      if (stdlib == Stdlib::LIBCXX) {
        // Default to >= 10.7 for libc++
        deploymentTarget = std::max(deploymentTarget, OSVersion(10, 7));
      }
    } else {
      // No -DOSXCROSS_DEFAULT_DEPLOYMENT_TARGET=XX: default to the SDK version
      deploymentTarget = sdk.version;
    }
  }

  if (deploymentTarget > sdk.maxDeploymentTarget) {
    err << "deployment target " << deploymentTarget.displayStr()
        << " is above the SDK maximum " << sdk.maxDeploymentTarget.displayStr()
        << err.endl();
    return false;
  }

  if (getenv("OSXCROSS_CHECK_MIN_DEPLOYMENT_TARGET")) {
    if (!sdk.minDeploymentTarget.num()) {
      warn << "OSXCROSS_CHECK_MIN_DEPLOYMENT_TARGET is set, but the SDK does "
           << "not state a minimum deployment target" << warn.endl();
    } else if (deploymentTarget < sdk.minDeploymentTarget) {
      err << "deployment target " << deploymentTarget.displayStr()
          << " is below the SDK minimum "
          << sdk.minDeploymentTarget.displayStr() << err.endl();
      return false;
    }
  }

  //
  // C++ standard library
  //

  if (stdlib == Stdlib::UNSET) {
    stdlib = libCXXIsDefaultCXXLib() ? Stdlib::LIBCXX : Stdlib::LIBSTDCXX;
  } else if (stdlib == Stdlib::LIBCXX) {
    if (!hasLibCXX()) {
      err << "libc++ requires macOS SDK 10.7 (or later)" << err.endl();
      return false;
    }

    if (deploymentTarget.num() && deploymentTarget < OSVersion(10, 7)) {
      err << "libc++ requires '-mmacosx-version-min=10.7' (or later)"
          << err.endl();
      return false;
    }
  }

  if (sdk.version >= OSVersion(10, 14) && stdlib == Stdlib::LIBSTDCXX &&
      !isGCC() && !useGCCLibs) {
    err << "macOS SDK '>= 10.14' does not support libstdc++ anymore"
        << err.endl();
    return false;
  }

  // The libstdc++ and libgcc installed by ./build_gcc.sh
  // (clang++-gstdc++, -foc-use-gcc-libstdc++).
  const bool useGCCLibstdcxx = stdlib == Stdlib::LIBSTDCXX && useGCCLibs;

  //
  // C++ standard library headers
  //
  // They are added as -isystem <cxxHeaderPath> and, for each entry of
  // cxxHeaderSubDirs, as -isystem <cxxHeaderPath>/<subdir>. cxxHeaderPath
  // stays empty if the compiler finds them itself (GCC with libstdc++).
  //

  std::string cxxHeaderPath;
  StringVector cxxHeaderSubDirs;

  switch (stdlib) {
  case Stdlib::LIBCXX: {
    cxxHeaderPath = sdk.path + "/usr/include/c++/v1";
    if (!dirExists(cxxHeaderPath)) {
      err << "cannot find " << getStdlibString(stdlib) << " headers"
          << err.endl();
      return false;
    }
    break;
  }
  case Stdlib::LIBSTDCXX: {
    // GCC brings its own libstdc++ headers.
    if (isGCC())
      break;

    if (useGCCLibs) {
      // Use libs from './build_gcc.sh' installation

      cxxHeaderPath = gccInstallDir() + "/";

      // Build the target triple name we BUILD for - not what triple has been
      // invoked.
      // i.e. x86_64-apple-darwin27-clang++-gstdc++ -arch arm64
      //      -> results in aarch64-apple-darwin27

      Arch cxxBuildArch;

      if (archs.size() > 1) { // TODO: should probably check earlier for this
        err << "clang++-gstdc++ does not support multiple architectures in "
              "one invocation"
            << err.endl();
        return false;
      }

      switch (archs[0]) {
      case Arch::ARM64:
        cxxBuildArch = Arch::AARCH64;
        break;
      case Arch::I386:
      case Arch::X86_64:
        // i386 is a multilib of the x86_64 GCC installation;
        cxxBuildArch = Arch::X86_64;
        break;
      default:
        err << "clang++-gstdc++ does not support architecture '"
            << getArchName(archs[0]) << "'" << err.endl();
        return false;
      }
      const std::string cxxBuildTriple =
          makeTriple(cxxBuildArch, vendor, targetOS);

      cxxHeaderPath += cxxBuildTriple;
      cxxHeaderPath += "/include/c++";

      // The highest installed GCC version wins.
      std::vector<GCCVersion> v;

      for (const std::string &entry : listFiles(cxxHeaderPath)) {
        if (entry[0] != '.')
          v.push_back(parseGCCVersion(entry.c_str()));
      }

      if (v.empty()) {
        err << "'-foc-use-gcc-libstdc++' requires gcc to be installed "
               "(./build_gcc.sh)" << err.endl();
        return false;
      }

      std::sort(v.begin(), v.end());
      gccVersion = v.back();
      gccTriple = cxxBuildTriple;

      cxxHeaderPath += "/";
      cxxHeaderPath += gccVersion.str();

      cxxHeaderSubDirs.push_back(cxxBuildTriple);
    } else {
      // Use SDK libs
      if (sdk.version <= OSVersion(10, 5))
        cxxHeaderPath = sdk.path + "/usr/include/c++/4.0.0";
      else
        cxxHeaderPath = sdk.path + "/usr/include/c++/4.2.1";

      cxxHeaderSubDirs.push_back(makeTriple(primaryArch, vendor, targetOS));
    }

    cxxHeaderSubDirs.push_back("backward");

    if (!dirExists(cxxHeaderPath)) {
      err << "cannot find " << getStdlibString(stdlib) << " headers"
          << err.endl();
      return false;
    }

    break;
  }
  case Stdlib::UNSET:
    abort();
  }

  //
  // Compiler arguments
  //
  // wrapperArgs come first, the user's arguments (userArgs) follow them, so
  // the user can override most of what is set here.
  //

  wrapperArgs.push_back(compilerArgv0);

  std::string clangIntrinsicPath;

  if (isClang()) {
    wrapperArgs.push_back("-target");
    wrapperArgs.push_back(getTriple());

    const char *linkerVersion = getLinkerVersion();

    // -mlinker-version= is only relevant for ld64.
    if (!buildFlavor.isLLVM() && linkerVersion && linkerVersion[0])
      wrapperArgs.push_back(std::string("-mlinker-version=") + linkerVersion);

    if (findClangIntrinsicHeadersAndVersion(clangIntrinsicPath)) {
      // The resource-directory lookup warns when the version is unknown.
      if (haveArch(Arch::X86_64H) && clangVersion != ClangVersion() &&
          clangVersion < ClangVersion(3, 5)) {
        err << "'" << getArchName(Arch::X86_64H) << "' requires clang 3.5 "
            << "(or later)" << err.endl();
        return false;
      }
    }

    wrapperArgs.push_back("-isysroot");
    wrapperArgs.push_back(sdk.path);

    if (isCXX()) {
      wrapperArgs.push_back(std::string("-stdlib=") + getStdlibString(stdlib));

      if (stdlib == Stdlib::LIBCXX || useGCCLibstdcxx) {
        // The C++ headers are added with -isystem below.
        // -Qunused-arguments: -nostdinc++ is unused when linking.
        wrapperArgs.push_back("-nostdinc++");
        wrapperArgs.push_back("-Qunused-arguments");
      }

      if (useGCCLibstdcxx) {
        // Match the TLS ABI of the libstdc++ built by build_gcc.sh.
        // User arguments follow wrapperArgs, so -fno-emulated-tls can
        // override this.
        wrapperArgs.push_back("-femulated-tls");

        // Use libs from './build_gcc.sh' installation
        if (!isGCH())
          addGCCRuntimeLibs(archs[0]);
      }
    }
  } else if (isGCC()) {
    // The SDK determined above, as for clang; without it, GCC would use its
    // built-in sysroot, the installed SDK.
    wrapperArgs.push_back("-isysroot");
    wrapperArgs.push_back(sdk.path);

    if (isCXX() && stdlib == Stdlib::LIBCXX) {
      wrapperArgs.push_back("-nostdinc++");
      wrapperArgs.push_back("-nodefaultlibs");

      if (!isGCH()) {
        wrapperArgs.push_back("-lc");
        wrapperArgs.push_back("-lc++");
        if (sdk.version <= OSVersion(10, 13)) {
          // SDK 10.14 does not have -lgcc_s anymore
          wrapperArgs.push_back("-lgcc_s.10.5");
        }
      }
    } else if (stdlib != Stdlib::LIBCXX && !isGCH() &&
               useStaticGCCRuntime()) {
      wrapperArgs.push_back("-static-libgcc");
      wrapperArgs.push_back("-static-libstdc++");
    }

    if (!buildFlavor.isLLVM() && !isGCH())
      wrapperArgs.push_back("-Wl,-no_compact_unwind");
  }

  auto addCXXHeaderPath = [&](const std::string &path) {
    wrapperArgs.push_back("-isystem");
    wrapperArgs.push_back(path);
  };

  if (!cxxHeaderPath.empty()) {
    addCXXHeaderPath(cxxHeaderPath);

    for (auto &subDir : cxxHeaderSubDirs)
      addCXXHeaderPath(cxxHeaderPath + "/" + subDir);
  }

  if (getenv("OSXCROSS_MP_INC")) {
    std::string macPortsIncludeDir;
    std::string macPortsLibraryDir;
    std::string macPortsFrameworksDir;

    // Add them to userArgs (instead of wrapperArgs),
    // so the user's -I / -L / -F is prefered.

    if (getMacPortsIncludeDir(macPortsIncludeDir)) {
      userArgs.push_back("-isystem");
      userArgs.push_back(macPortsIncludeDir);

      if (getMacPortsLibDir(macPortsLibraryDir)) {
        if (isClang())
          userArgs.push_back("-Qunused-arguments");

        userArgs.push_back("-L");
        userArgs.push_back(macPortsLibraryDir);
      }

      if (getMacPortsFrameworksDir(macPortsFrameworksDir)) {
        userArgs.push_back("-iframework");
        userArgs.push_back(macPortsFrameworksDir);
      }
    }
  }

  // Add clang's intrinsic headers, unless -resource-dir, -nostdinc or
  // -nobuiltininc is given (see skipIntrinsicHeaders).
  if (isClang() && !clangIntrinsicPath.empty()) {
    if (skipIntrinsicHeaders) {
      if (getDebugLevel() > 0)
        dbg << "clang's intrinsic headers not added (-resource-dir, "
            << "-nostdinc or -nobuiltininc given)" << dbg.endl();
    } else {
      wrapperArgs.push_back("-isystem");
      wrapperArgs.push_back(clangIntrinsicPath);
    }
  }

  if (deploymentTarget.num()) {
    std::string versionMin = deploymentTarget.str();

    if (isClang() && clangVersion < ClangVersion(11, 0) &&
        deploymentTarget >= OSVersion(11, 0)) {
      // Clang <= 10 can't parse -mmacosx-version-min=11.x
      warn << "your clang installation is outdated and can't parse "
              "'-mmacosx-version-min=" << deploymentTarget.shortStr() << "'. "
           << "setting it to 10.16." << warn.endl();
      versionMin = "10.16";
    }

    wrapperArgs.push_back("-mmacosx-version-min=" + versionMin);
  }

  for (auto arch : archs) {
    switch (arch) {
    case Arch::I386:
    case Arch::I486:
    case Arch::I586:
    case Arch::I686:
    case Arch::ARM64:
    case Arch::ARM64E:
    case Arch::X86_64:
    case Arch::X86_64H:
      break;
    default:
      err << "unsupported architecture: '" << getArchName(arch) << "'"
          << err.endl();
      return false;
    }

    if (isGCC()) {
      if (arch != Arch::X86_64 && arch != Arch::I386 &&
          arch != Arch::ARM64) {
        err << "gcc does not support architecture '" << getArchName(arch)
            << "'" << err.endl();
        return false;
      }

      if (archs.size() > 1)
        continue;

      // arm64 needs neither -m32 nor -m64.
      if (arch != Arch::ARM64)
        wrapperArgs.push_back(arch == Arch::I386 ? "-m32" : "-m64");
    } else if (isClang()) {
      // -foc-use-gcc-libstdc++ does not support multiple architectures.
      if (useGCCLibs && archs.size() > 1)
        continue;

      wrapperArgs.push_back("-arch");
      wrapperArgs.push_back(getArchName(arch));
    }
  }

#ifdef __ANDROID__
  // Workaround for Termux
  std::string ldSysRoot = "-Wl,-syslibroot,";
  ldSysRoot += sdk.path;
  wrapperArgs.push_back(ldSysRoot);
#endif

  if (isClang()) {
    // Link with ld64.lld. Not if nothing is linked: clang would warn about
    // the unused -fuse-ld=lld, which is an error with -Werror.
    if (buildFlavor.isLLVM() && linking)
      wrapperArgs.push_back("-fuse-ld=lld");

    if (getenv("OSXCROSS_PRETEND_TO_BE_APPLE_CLANG")) {
      wrapperArgs.push_back("-D__apple_build_version__=1");
    }

    if (sdk.version >= OSVersion(14, 0) && clangVersion < ClangVersion(17, 0)) {
      // MacOS 14 SDK uses __ENVIRONMENT_OS_VERSION_MIN_REQUIRED__ in
      // AvailabilityInternal.h
      wrapperArgs.push_back("-D__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__=" +
                            deploymentTarget.numStr());
    }
    if (clangVersion >= ClangVersion(3, 8)) {
      //
      // Silence:
      // warning: libLTO.dylib relative to clang installed dir not found;
      //          using 'ld' default search path instead
      //
      // '-flto' will of course work nevertheless, it's just a buggy
      // cross-compilation warning.
      //
      // User arguments follow wrapperArgs, so -Wliblto can override this
      // default.
      wrapperArgs.push_back("-Wno-liblto");
    }
  } else if (isGCC()) {
    if (userArgs.empty() || (userArgs.size() == 1 && userArgs[0] == "-v")) {
      //
      // HACK:
      // Discard all arguments besides the first one
      // (which is <arch>-apple-darwinXX-gcc) to fix the issue
      // described in #135.
      //

      wrapperArgs.resize(1);
    }
  }

  //
  // Legacy deployment targets and the environment of the invoked tools
  //

  if (deploymentTarget < OSVersion(10, 6) &&
      !getenv("OSXCROSS_NO_10_5_DEPRECATION_WARNING")) {
    warn << "deployment targets below macOS 10.6 are unsupported; "
            "the resulting binaries may not work"
         << warn.endl();

    if (!buildFlavor.isLLVM()) {
      note << "the linker may abort with an assertion for these targets"
           << note.endl();
    }

    info << "use the 'osxcross-1.1' branch for older targets" << info.endl();
  }

  // GCC or clang++-gstdc++: GCC's libstdc++ is used.
  const bool isGCCLibstdcxx = isGCC() || (isClang() && useGCCLibstdcxx);

  if (buildFlavor.isLLVM() && isGCC()) {
    // The LLVM as wrapper needs GCC's selected deployment target and SDK.
    setenv("OSXCROSS_AS_TARGET_VERSION",
           deploymentTarget.str().c_str(), 1);
    passSDKToChildProcesses();
  } else if (isGCCLibstdcxx) {
    // Silence 'operator new[]' warning in ld64.
    setenv("OSXCROSS_GCC_LIBSTDCXX", "1", 1);
  }

  return true;
}
} // namespace target
