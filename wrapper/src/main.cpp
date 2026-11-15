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

/*
 * Debug messages can be enabled by setting 'OCDEBUG' (ENV) to >= 1.
 */

#include "compat.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "tools/tools.h"
#include "target.h"
#include "progs.h"
#include "passthrough-compiler-options/options.h"

using namespace tools;
using namespace target;

int unitTest = 0;
int debug = 0;

namespace {

void warnExtension(const char *extension) {
  static const bool NoExtensionWarnings =
      !!getenv("OSXCROSS_NO_EXTENSION_WARNINGS");
  if (NoExtensionWarnings)
    return;
  warn << extension << " is an osxcross extension" << warn.endl();
  warnInfo << "you can silence this warning via "
           << "'OSXCROSS_NO_EXTENSION_WARNINGS=1' (env)" << warnInfo.endl();
}

//
// Command Line Options
//

namespace commandopts {

// Declared in passthrough-compiler-options/options.h
using ::commandopts::OptFun;
using ::commandopts::Parser;
using ::commandopts::PassthroughCompilerOptions;

bool versionMin(Target &target, const char *, const char *val, char **) {
  target.deploymentTarget = parseOSVersion(val);

  if (!target.deploymentTarget.matchesString(val))
    warn << "'-mmacosx-version-min=' (" << target.deploymentTarget.str()
         << " != " << val << ")" << warn.endl();

  return true;
}

bool arch(Target &target, const char *opt, const char *val, char **) {
  Arch arch;

  if (!strcmp(opt, "-arch")) {
    arch = parseArch(val);

    if (arch == Arch::UNKNOWN_ARCH)
      warn << "'-arch': unknown architecture '" << val << "'"
           << warn.endl();

    const char *name = getArchName(arch);

    if (strcmp(val, name))
      warn << "'-arch': '" << val << "' != '" << name << "'" << warn.endl();
  } else {
    if (!strcmp(opt, "-m16") || !strcmp(opt, "-mx32")) {
      err << "'" << opt << "' is not supported" << err.endl();
      return false;
    } else if (!strcmp(opt, "-m32")) {
      arch = Arch::I386;
    } else if (!strcmp(opt, "-m64")) {
      arch = Arch::X86_64;
    } else {
      __builtin_unreachable();
    }
  }

  if (target.isClang())
    target.addArch(arch);
  else
    target.primaryArch = arch;

  return true;
}

bool stdlib(Target &target, const char *, const char *val, char **) {
  if (target.isGCC())
    warnExtension("'-stdlib='");

  size_t i = 0;

  for (auto stdlibName : StdlibNames) {
    if (!strcmp(val, stdlibName)) {
      target.stdlib = static_cast<Stdlib>(i);
      break;
    }
    ++i;
  }

  if (i == (sizeof(StdlibNames) / sizeof(StdlibNames[0]))) {
    err << "value of '-stdlib=' must be ";

    for (size_t j = 0; j < i; ++j) {
      err << "'" << StdlibNames[j] << "'";

      if (j == i - 2)
        err << " or ";
      else if (j < i - 2)
        err << ", ";
    }

    err << err.endl();
    return false;
  }

  return true;
}

bool language(Target &target, const char *, const char *val, char **) {
  target.language = val;
  return true;
}

bool useGCCLibstdcxx(Target &target, const char *, const char *, char **) {
  target.stdlib = Stdlib::LIBSTDCXX;
  target.useGCCLibs = true;
  return true;
}

bool compilerPath(Target &target, const char *, const char *path, char **) {
  target.compilerPath = path;
  return true;
}

bool intrinsicPath(Target &target, const char *, const char *path, char **) {
  target.intrinsicPathOption = path;
  return true;
}

bool isysroot(Target &target, const char *, const char *path, char **) {
  target.commandLineIsysroot = path;
  return true;
}

bool sysroot(Target &target, const char *, const char *path, char **) {
  target.commandLineSysroot = path;
  return true;
}

bool skipIntrinsicHeaders(Target &target, const char *, const char *,
                          char **) {
  target.skipIntrinsicHeaders = true;
  return true;
}

bool noLinking(Target &target, const char *, const char *, char **) {
  target.linking = false;
  return true;
}

// -Xarch_<arch> <arg> changes <arg> for <arch> only. The wrapper applies the
// SDK, the C++ library and its header setup to all architectures, so the
// options that change them cannot be given per architecture.
bool xarch(Target &, const char *opt, const char *archName, char **args) {
  static const char *const Prefixes[] = {
    "-stdlib=", "-isysroot", "--sysroot", "-foc-compiler-path=",
    "-foc-intrinsic-path=", "-foc-run-prog="
  };
  static const char *const Names[] = {
    "-nostdinc", "--no-standard-includes", "-nobuiltininc",
    "-foc-use-gcc-libstdc++"
  };

  const char *arg = *args; // nullptr if -Xarch_<arch> is the last argument
  const char *rejected = nullptr;

  if (!arg)
    return true;

  for (const char *prefix : Prefixes) {
    if (!strncmp(arg, prefix, strlen(prefix)))
      rejected = prefix;
  }

  for (const char *name : Names) {
    if (!strcmp(arg, name))
      rejected = name;
  }

  if (!rejected)
    return true;

  err << "'" << opt << archName << " " << arg << "' is not supported"
      << err.endl();
  note << "osxcross applies '" << std::string(rejected, strcspn(rejected, "="))
       << "' to all architectures (a known limitation)" << note.endl();
  return false;
}

bool runProg(Target &target, const char *, const char *progName,
             char **progArgs) {
  auto *prog = program::getProg(progName);

  if (!prog)
    exit(EXIT_FAILURE);

  std::vector<char *> args;
  args.push_back(const_cast<char *>(progName));

  while (*progArgs)
    args.push_back(*progArgs++);
  args.push_back(nullptr);

  (*prog)(args.size() - 1, args.data(), target);
}

bool checkIncludePath(Target &, const char *opt, const char *path, char **) {
#ifndef __APPLE__
  constexpr const char *dangerousIncludePaths[] = { "/usr/include",
                                                    "/usr/local/include" };

  static const bool NoIncludePathWarnings =
      !!getenv("OSXCROSS_NO_INCLUDE_PATH_WARNINGS");

  if (NoIncludePathWarnings)
    return true;

  char *resolved = realpath(path, nullptr);
  const char *resolvedPath = resolved ? resolved : path;

  for (const char *dangerousPath : dangerousIncludePaths) {
    if (!strncmp(resolvedPath, dangerousPath, strlen(dangerousPath))) {
      warn << "possibly dangerous include path specified: '" << opt << " "
           << path << "'";

      if (strcmp(path, resolvedPath))
        warn << " (" << resolvedPath << ")";

      warn << warn.endl();

      warnInfo << "you can silence this warning via "
               << "'OSXCROSS_NO_INCLUDE_PATH_WARNINGS=1' (env)"
               << warnInfo.endl();
    }
  }

  free(resolved);
#else
  (void)opt;
  (void)path;
#endif

  return true;
}

// The wrapper's own options. parse() adds the options of the compiler that
// take a value (passthrough-compiler-options/options.cpp), so that their
// values are not taken as options.
Parser options = {{
  {"-mmacos-version-min", versionMin, ValueMode::EQUALS},
  {"-mmacosx-version-min", versionMin, ValueMode::EQUALS},
  {"-stdlib", stdlib, ValueMode::EQUALS},
  {"--stdlib", stdlib, ValueMode::SEPARATE | ValueMode::EQUALS},
  {"-arch", arch, ValueMode::SEPARATE},
  {"-m16", arch},
  {"-m32", arch},
  {"-mx32", arch},
  {"-m64", arch},
  {"-x", language, ValueMode::JOINED | ValueMode::SEPARATE, ParseMode::FORWARD},
  {"--language", language, ValueMode::SEPARATE | ValueMode::EQUALS,
   ParseMode::FORWARD},
  {"-foc-use-gcc-libstdc++", useGCCLibstdcxx},
  // for internal use only
  {"-foc-run-prog", runProg, ValueMode::EQUALS},
  {"-isystem", checkIncludePath, ValueMode::JOINED | ValueMode::SEPARATE,
   ParseMode::FORWARD},
  {"-icxx-isystem", checkIncludePath, ValueMode::JOINED | ValueMode::SEPARATE,
   ParseMode::FORWARD},
  {"-cxx-isystem", checkIncludePath, ValueMode::JOINED | ValueMode::SEPARATE,
   ParseMode::FORWARD},
  {"-I", checkIncludePath, ValueMode::JOINED | ValueMode::SEPARATE,
   ParseMode::FORWARD},
  {"--include-directory", checkIncludePath,
   ValueMode::SEPARATE | ValueMode::EQUALS, ParseMode::FORWARD},

  // The SDK (see Target::determineSDK()). Consumed: setup() passes the SDK
  // in use as -isysroot.
  {"-isysroot", isysroot, ValueMode::JOINED | ValueMode::SEPARATE},
  {"--sysroot", sysroot, ValueMode::SEPARATE | ValueMode::EQUALS},

  // clang's builtin headers chosen or turned off by the user (see
  // Target::skipIntrinsicHeaders)
  {"-resource-dir", skipIntrinsicHeaders,
   ValueMode::SEPARATE | ValueMode::EQUALS, ParseMode::FORWARD},
  {"-nostdinc", skipIntrinsicHeaders, ValueMode::NONE, ParseMode::FORWARD},
  {"--no-standard-includes", skipIntrinsicHeaders, ValueMode::NONE,
   ParseMode::FORWARD},
  {"-nobuiltininc", skipIntrinsicHeaders, ValueMode::NONE,
   ParseMode::FORWARD},

  // Options after which nothing is linked (see Target::linking)
  {"-c", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"--compile", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"-E", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"--preprocess", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"-S", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"--assemble", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"-M", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"--dependencies", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"-MM", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"--user-dependencies", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"-fsyntax-only", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"--analyze", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"--precompile", noLinking, ValueMode::NONE, ParseMode::FORWARD},
  {"-emit-ast", noLinking, ValueMode::NONE, ParseMode::FORWARD},

  // -Xarch_<arch> <arg>: see xarch()
  {"-Xarch_", xarch, ValueMode::JOINED_AND_SEPARATE, ParseMode::FORWARD},

  // sets a custom path for the compiler
  {"-foc-compiler-path", compilerPath, ValueMode::EQUALS},

  // specifies an additional directory to search when looking for clang's
  // intrinsic paths
  {"-foc-intrinsic-path", intrinsicPath, ValueMode::EQUALS},
}, getDebugLevel() > 0};

bool parse(int argc, char **argv, Target &target) {
  // The compiler's options with a value, so that "-c" in "-o -c" is
  // taken as the file name (passthrough-compiler-options/options.cpp)
  options.add(PassthroughCompilerOptions);

  target.userArgs.reserve(argc);

  if (char *p = getenv("MACOSX_DEPLOYMENT_TARGET")) {
    target.deploymentTarget = parseOSVersion(p);
    unsetenv("MACOSX_DEPLOYMENT_TARGET");
  }

  for (int i = 1; i < argc; ++i) {
    char *arg = argv[i];

    if (*arg != '-') {
      target.userArgs.push_back(arg);
      continue;
    }

    const char *suffix = nullptr;
    const Parser::Option *opt = options.parse(arg, &suffix);

    if (!opt) {
      target.userArgs.push_back(arg);
      continue;
    }

    const ValueMode mode = opt->valueMode;
    const int firstArg = i;
    const char *val = nullptr;
    int numArgs = 0; // following arguments that belong to the option

    if (hasFlag(mode, ValueMode::REMAINING_ARGS)) {
      numArgs = argc;
    } else if (hasFlag(mode, ValueMode::JOINED_AND_SEPARATE)) {
      val = suffix;
      numArgs = 1;
    } else if (*suffix == '=' && hasFlag(mode, ValueMode::EQUALS)) {
      val = suffix + 1;
    } else if (*suffix) {
      val = suffix;
    } else if (hasFlag(mode, ValueMode::SEPARATE)) {
      numArgs = opt->numArgs;
      val = argv[i + 1]; // nullptr after the last argument
    } else if (hasFlag(mode, ValueMode::EQUALS)) {
      err << "expected '" << opt->name << "=<val>' "
          << "instead of '" << arg << "'" << err.endl();
      return false;
    } else if (hasFlag(mode, ValueMode::JOINED)) {
      val = suffix;
    }

    // Only for the wrapper's own options; the compiler reports the others.
    if (opt->fun && (!val || !*val) && mode != ValueMode::NONE &&
        !hasFlag(mode, ValueMode::JOINED_AND_SEPARATE)) {
      err << "missing argument for '" << opt->name << "'" << err.endl();
      return false;
    }

    i = std::min(i + numArgs, argc - 1);

    options.printDebug(*opt, arg, val);

    if (opt->fun && !opt->fun(target, opt->name, val, &argv[firstArg + 1]))
      return false;

    if (hasFlag(opt->parseMode, ParseMode::FORWARD)) {
      for (int j = firstArg; j <= i; ++j)
        target.userArgs.push_back(argv[j]);
    }
  }

  return true;
}

} // namespace commandopts

void detectCXXLib(Target &target) {
  if (target.compilerName.size() <= 7)
    return;

  Stdlib prevStdlib = target.stdlib;

  if (endsWith(target.compilerName, "-stdc++")) {
    target.stdlib = Stdlib::LIBSTDCXX;
    target.compilerName.resize(target.compilerName.size() - 7);
  } else if (endsWith(target.compilerName, "-gstdc++")) {
    target.stdlib = Stdlib::LIBSTDCXX;
    target.useGCCLibs = true;
    target.compilerName.resize(target.compilerName.size() - 8);
  } else if (endsWith(target.compilerName, "-libc++")) {
    target.stdlib = Stdlib::LIBCXX;
    target.compilerName.resize(target.compilerName.size() - 7);
  }

  if (prevStdlib != Stdlib::UNSET && prevStdlib != target.stdlib)
    warn << "ignoring '-stdlib=" << getStdlibString(prevStdlib) << "'"
         << warn.endl();
}

//
// detectTarget():
//  detect target and setup invocation command
//

bool detectTarget(int argc, char **argv, Target &target) {
  const char *cmd = argv[0];
  const char *p = strrchr(cmd, '/');
  char archName[16];
  size_t len;

  if (p)
    cmd = &p[1];

  if (auto *prog = program::getProg(cmd))
    (*prog)(argc, argv, target);

  // -> x86_64 <- -apple-darwin13
  p = strchr(cmd, '-');
  len = (p ? p : cmd) - cmd;

  if (len >= sizeof(archName))
    return false;

  memcpy(archName, cmd, len);
  archName[len] = '\0';

  target.primaryArch = parseArch(archName);

  if (target.primaryArch != Arch::UNKNOWN_ARCH) {
    cmd += len;

    if (*cmd++ != '-')
      return false;

    if (strncmp(cmd, "apple-", 6))
      return false;

    cmd += 6;

    if (strncmp(cmd, "darwin", 6))
      return false;

    if (!(p = strchr(cmd, '-')))
      return false;

    target.targetOS = std::string(cmd, p - cmd);
    target.compiler = getCompilerIdentifier(&p[1]);
    target.compilerName = &p[1];

    if (target.compilerName == "cc") {
      target.compiler = getDefaultCompilerIdentifier();
      target.compilerName = getDefaultCompilerName();
    } else if (target.compilerName == "c++") {
      target.compiler = getDefaultCXXCompilerIdentifier();
      target.compilerName = getDefaultCXXCompilerName();
    } else if (auto *prog = program::getProg(target.compilerName)) {
      (*prog)(argc, argv, target);
    }

    if (target.targetOS != getDefaultTarget())
      warn << "this wrapper was built for target "
            << "'" << getDefaultTarget() << "'" << warn.endl();

    if (!commandopts::parse(argc, argv, target))
      return false;

    detectCXXLib(target);
    return target.setup();
  }

  if (!strncmp(cmd, "o32", 3))
    target.primaryArch = Arch::I386;
  else if (!strncmp(cmd, "o64h", 4))
    target.primaryArch = Arch::X86_64H;
  else if (!strncmp(cmd, "o64", 3))
    target.primaryArch = Arch::X86_64;
  else if (!strncmp(cmd, "oa64e", 5))
    target.primaryArch = Arch::ARM64E;
  else if (!strncmp(cmd, "oa64", 4))
    target.primaryArch = Arch::ARM64;
  else
    return false;

  if (const char *p = strchr(cmd, '-')) {
    const char *compilerName = &cmd[p - cmd + 1];
    target.compiler = getCompilerIdentifier(compilerName);
    target.compilerName = compilerName;
  }

  if (!commandopts::parse(argc, argv, target))
    return false;

  detectCXXLib(target);
  return target.setup();
}

//
// Time
//

// Measures the elapsed time, excluding the time between halt() and resume().
class Benchmark {
  typedef std::chrono::steady_clock Clock;

public:
  Benchmark() : start(Clock::now()) {}

  double getMilliseconds() const {
    return std::chrono::duration<double, std::milli>(Clock::now() - start)
        .count();
  }

  void halt() { haltTime = Clock::now(); }
  void resume() { start += Clock::now() - haltTime; }

private:
  Clock::time_point start;
  Clock::time_point haltTime;
};

} // unnamed namespace

//
// Main routine
//

int main(int argc, char **argv) {
  Benchmark b;
  Target target;
  int rc = -1;

  debug = getDebugLevel();

  if (char *p = getenv("OSXCROSS_UNIT_TEST")) {
    unitTest = atoi(p);

    if ((p = getenv("OSXCROSS_PROG_NAME")))
      argv[0] = p;
  }

  if (!detectTarget(argc, argv, target)) {
    err << "while detecting target" << err.endl();
    return 1;
  }

  if (debug) {
    b.halt();

    if (debug >= 2) {
      dbg << "detected target triple: " << target.getTriple() << dbg.endl();
      dbg << "detected compiler: " << target.compilerName << dbg.endl();

      dbg << "detected stdlib: " << getStdlibString(target.stdlib)
          << dbg.endl();

      b.resume();
    }
  }

#ifdef __DragonFly__
  // Escape DragonFlyBSD's weird PFS paths.
  prependEnvVariable("COMPILER_PATH", escapePath(target.execDir));
#else
  prependEnvVariable("COMPILER_PATH", target.execDir);
#endif

  auto printCommand = [&]() {
    std::string in;
    std::string out;

    for (int i = 0; i < argc; ++i) {
      in += argv[i];
      in += " ";
    }

    out += target.compilerPath;

    if (target.compilerPath != target.wrapperArgs[0]) {
      out += " (";
      out += target.wrapperArgs[0];
      out += ") ";
    } else {
      out += " ";
    }

    for (size_t i = 1; i < target.wrapperArgs.size(); ++i) {
      out += target.wrapperArgs[i];
      out += " ";
    }

    for (auto &arg : target.userArgs) {
      out += arg;
      out += " ";
    }

    if (!unitTest)
      dbg << "--> " << in << dbg.endl();

    dbg << "<-- " << out << dbg.endl();
  };

  if (debug) {
    const double ms = b.getMilliseconds();

    if (rc == -1)
      printCommand();

    dbg << "=== time spent in wrapper: " << ms << " ms" << dbg.endl();
  }

  if (unitTest == 2)
    return 0;

  if (rc == -1) {
    StringVector compilerArgs = target.wrapperArgs;
    compilerArgs.insert(compilerArgs.end(), target.userArgs.begin(),
                        target.userArgs.end());

    program::execProgram(target.compilerPath, compilerArgs);

    if (!debug)
      printCommand();

    return 1;
  }

  return rc;
}
