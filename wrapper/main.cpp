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

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "tools.h"
#include "target.h"
#include "progs.h"

using namespace tools;
using namespace target;

int unitTest = 0;
int debug = 0;

namespace {

void warnExtension(const char *extension) {
  static bool noExtWarnings = !!getenv("OSXCROSS_NO_EXTENSION_WARNINGS");
  if (noExtWarnings)
    return;
  warn << extension << " is an osxcross extension" << warn.endl();
  warnInfo << "you can silence this warning via "
           << "'OSXCROSS_NO_EXTENSION_WARNINGS=1' (env)" << warnInfo.endl();
}

//
// Command Line Options
//

namespace commandopts {

typedef bool (*OptFun)(Target &target, const char *opt, const char *val,
                      char **);

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

  static bool noIncCheck = !!getenv("OSXCROSS_NO_INCLUDE_PATH_WARNINGS");

  if (noIncCheck)
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

typedef OptParser<OptFun, 17> Parser;
typedef Parser::ValueMode ValueMode;
typedef Parser::Forwarding Forwarding;

const Parser Options = {{
  {"-mmacos-version-min", versionMin, ValueMode::JOINED_WITH_EQUALS},
  {"-mmacosx-version-min", versionMin, ValueMode::JOINED_WITH_EQUALS},
  {"-stdlib", stdlib, ValueMode::JOINED_WITH_EQUALS},
  {"-arch", arch, ValueMode::SEPARATE},
  {"-m16", arch},
  {"-m32", arch},
  {"-mx32", arch},
  {"-m64", arch},
  {"-x", language, ValueMode::JOINED_OR_SEPARATE, Forwarding::KEEP},
  {"-foc-use-gcc-libstdc++", useGCCLibstdcxx},
  // for internal use only
  {"-foc-run-prog", runProg, ValueMode::JOINED_WITH_EQUALS},
  {"-isystem", checkIncludePath, ValueMode::JOINED_OR_SEPARATE,
   Forwarding::KEEP},
  {"-icxx-isystem", checkIncludePath, ValueMode::JOINED_OR_SEPARATE,
   Forwarding::KEEP},
  {"-cxx-isystem", checkIncludePath, ValueMode::JOINED_OR_SEPARATE,
   Forwarding::KEEP},
  {"-I", checkIncludePath, ValueMode::JOINED_OR_SEPARATE, Forwarding::KEEP},

  // sets a custom path for the compiler
  {"-foc-compiler-path", compilerPath, ValueMode::JOINED_WITH_EQUALS},

  // specifies an additional directory to search when looking for clang's
  // intrinsic paths
  {"-foc-intrinsic-path", intrinsicPath, ValueMode::JOINED_WITH_EQUALS}
}, getDebugLevel() > 0};

bool parse(int argc, char **argv, Target &target) {
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

    const Parser::Option *opt = Options.parse(arg);

    if (!opt) {
      target.userArgs.push_back(arg);
      continue;
    }

    const bool pushArg = opt->forwarding == Forwarding::KEEP;
    const int firstArg = i;
    const char *val = nullptr;

    if (opt->valueMode != ValueMode::NONE) {
      val = arg + opt->nameLen;

      if (opt->valueMode == ValueMode::JOINED_WITH_EQUALS) {
        if (*val != '=') {
          err << "expected '" << opt->name << "=<val>' "
              << "instead of '" << arg << "'" << err.endl();
          return false;
        }

        ++val;
      }

      if (opt->valueMode != ValueMode::JOINED_WITH_EQUALS &&
          !*val && i < argc - 1)
        val = argv[++i];

      if (!*val) {
        err << "missing argument for '" << opt->name << "'" << err.endl();
        return false;
      }
    }

    Options.printDebug(*opt, arg, val);

    if (opt->fun && !opt->fun(target, opt->name, val, &argv[i + 1]))
      return false;

    if (pushArg) {
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
