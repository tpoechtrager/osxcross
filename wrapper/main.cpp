/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014 by Thomas Poechtrager                           *
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

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <cassert>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "tools.h"
#include "target.h"
#include "progs.h"

using namespace tools;
using namespace target;

namespace {

//
// detectTarget():
//  detect target and setup invocation command
//

#define PABREAK                                                                \
  else target.args.push_back(arg);                                             \
  break

bool detectTarget(int argc, char **argv, Target &target) {
  const char *cmd = argv[0];
  const char *p = strrchr(cmd, '/');
  size_t len;
  size_t i = 0;

  if (p)
    cmd = &p[1];

  target.args.reserve(static_cast<size_t>(argc));

  auto warnExtension = [](const char *extension) {
    std::cerr << "warning: '" << extension << "' is an OSXCross extension"
              << std::endl;
  };

  auto parseArgs = [&]()->bool {
    typedef bool (*delayedfun)(Target &);
    std::vector<delayedfun> delayedfuncs;

    auto getVal = [&](char * arg, const char * flag, int & i)->const char * {
      const char *val = arg + strlen(flag);

      if (!*val) {
        val = argv[++i];

        if (i >= argc) {
          std::cerr << "missing argument for '" << flag << "'" << std::endl;
          return nullptr;
        }
      }

      return val;
    };

    if (char *p = getenv("MACOSX_DEPLOYMENT_TARGET")) {
      target.OSNum = parseOSVersion(p);
      unsetenv("MACOSX_DEPLOYMENT_TARGET");
    }

    for (int i = 1; i < argc; ++i) {
      char *arg = argv[i];

      if (arg[0] == '-') {
        switch (arg[1]) {
        case 'a': {
          // -a

          if (!strncmp(arg, "-arch", 5)) {
            const char *val = getVal(arg, "-arch", i);

            if (!val)
              return false;

            Arch arch = parseArch(val);

            if (arch == Arch::unknown) {
              std::cerr << "warning: '-arch': unknown architecture '" << val
                        << "'" << std::endl;
            }

            const char *name = getArchName(arch);

            if (strcmp(val, name)) {
              std::cerr << "warning: '-arch': " << val << " != " << name
                        << std::endl;
            }

            target.addArch(arch);
          }

          PABREAK;
        }
        case 'E': {
          // -E

          if (!strcmp(arg, "-E")) {
            target.nocodegen = true;

            delayedfuncs.push_back([](Target &t) {
              if (t.targetarch.size() > 1) {
                std::cerr << "cannot use '-E' with multiple -arch options"
                          << std::endl;
                return false;
              }
              return true;
            });

            target.args.push_back(arg);
          }

          PABREAK;
        }
        case 'f': {
          // -f

          if (!strcmp(arg, "-flto") || !strncmp(arg, "-flto=", 5)) {
            target.args.push_back(arg);

            if (target.isClang())
              continue;

            delayedfuncs.push_back([](Target &t) {
              if (t.targetarch.size() > 1) {
                std::cerr << "gcc does not support '-flto' with multiple "
                          << "-arch flags" << std::endl;
                return false;
              }
              return true;
            });
          }

          PABREAK;
        }
        case 'm': {
          // -m

          if (!strncmp(arg, "-mmacosx-version-min=", 21)) {
            const char *val = arg + 21;
            target.OSNum = parseOSVersion(val);

            if (target.OSNum != val) {
              std::cerr << "warning: '-mmacosx-version-min=' ("
                        << target.OSNum.Str() << " != " << val << ")"
                        << std::endl;
            }
          } else if (!strcmp(arg, "-m16") || !strcmp(arg, "-mx32")) {
            std::cerr << "'" << arg << "' not supported" << std::endl;
            return false;
          } else if (!strcmp(arg, "-m32")) {
            target.addArch(Arch::i386);
          } else if (!strcmp(arg, "-m64")) {
            target.addArch(Arch::x86_64);
          }

          PABREAK;
        }
        case 'o': {
          // -o

          if (!strcmp(arg, "-oc-use-gcc-libs")) {
            if (target.isGCC()) {
              std::cerr << "warning: '" << arg << "' has no effect"
                        << std::endl;
              break;
            }
            target.stdlib = StdLib::libstdcxx;
            target.usegcclibs = true;
          } else if (!strncmp(arg, "-o", 2)) {
            target.outputname = getVal(arg, "-o", i);
          }

          PABREAK;
        }
        case 's': {
          // -s

          if (!strncmp(arg, "-stdlib=", 8)) {
            const char *val = arg + 8;
            size_t i = 0;

            if (target.isGCC())
              warnExtension("-stdlib=");

            for (auto stdlibname : StdLibNames) {
              if (!strcmp(val, stdlibname)) {
                target.stdlib = static_cast<StdLib>(i);
                break;
              }
              ++i;
            }

            if (i == (sizeof(StdLibNames) / sizeof(StdLibNames[0]))) {
              std::cerr << "value of '-stdlib=' must be ";

              for (size_t j = 0; j < i; ++j) {
                std::cerr << "'" << StdLibNames[j] << "'";
                if (j == i - 2) {
                  std::cerr << " or ";
                } else if (j < i - 2) {
                  std::cerr << ", ";
                }
              }

              std::cerr << std::endl;
              return false;
            }

          } else if (!strncmp(arg, "-std=", 5)) {
            const char *val = arg + 5;
            target.langstd = val;
          }

          PABREAK;
        }
        case 'x': {
          if (!strncmp(arg, "-x", 2)) {
            target.lang = getVal(arg, "-x", i);
          }

          PABREAK;
        }
        default:
          target.args.push_back(arg);
        }

        continue;
      }

      // Detect source file
      target.args.push_back(arg);

      const char *prevarg = "";

      if (i > 1) {
        prevarg = argv[i - 1];

        if (prevarg[0] == '-' && strlen(prevarg) > 2 &&
            strcmp(prevarg, "-MT") && strcmp(prevarg, "-MF"))
          prevarg = "";
      }

      if (prevarg[0] != '-' || !strcmp(prevarg, "-c")) {
        constexpr const char *badexts[] = { ".o", ".a" };
        const char *ext = getFileExtension(arg);
        bool b = false;

        for (auto &badext : badexts) {
          if (!strcmp(ext, badext)) {
            b = true;
            break;
          }
        }

        if (!b)
          target.sourcefile = arg;
      }
    }

    for (auto fun : delayedfuncs) {
      if (!fun(target))
        return false;
    }

    return true;
  };

  auto checkCXXLib = [&]() {
    if (target.compiler.size() <= 7)
      return;

    if (target.compiler.rfind("-libc++") == (target.compiler.size() - 7)) {
      if (target.stdlib != StdLib::unset && target.stdlib != StdLib::libcxx) {
        std::cerr << "warning: '-stdlib=" << getStdLibString(target.stdlib)
                  << "' will be ignored" << std::endl;
      }

      target.compiler.resize(target.compiler.size() - 7);
      target.stdlib = StdLib::libcxx;
    }
  };

  if (auto *prog = program::getprog(cmd))
    (*prog)(argc, argv, target);

  // -> x86_64 <- -apple-darwin13
  p = strchr(cmd, '-');
  len = (p ? p : cmd) - cmd;

  for (auto arch : ArchNames) {
    ++i;

    if (!strncmp(cmd, arch, len)) {
      target.arch = static_cast<Arch>(i - 1);
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

      target.target = std::string(cmd, p - cmd);
      target.compiler = &p[1];

      if (target.compiler == "cc")
        target.compiler = getDefaultCompiler();
      else if (target.compiler == "c++")
        target.compiler = getDefaultCXXCompiler();
      else if (auto *prog = program::getprog(target.compiler))
        (*prog)(argc, argv, target);

      if (target.target != getDefaultTarget()) {
        std::cerr << "warning: target mismatch (" << target.target
                  << " != " << getDefaultTarget() << ")" << std::endl;
      }

      if (!parseArgs())
        return false;

      checkCXXLib();
      return target.setup();
    }
  }

  if (!strncmp(cmd, "o32", 3))
    target.arch = Arch::i386;
  else if (!strncmp(cmd, "o64h", 4))
    target.arch = Arch::x86_64h;
  else if (!strncmp(cmd, "o64", 3))
    target.arch = Arch::x86_64;
  else
    return false;

  if (const char *p = strchr(cmd, '-'))
    target.compiler = &cmd[p - cmd + 1];

  if (!parseArgs())
    return false;

  checkCXXLib();
  return target.setup();
}

//
// generateMultiArchObjectFile():
//  support multiple -arch flags with gcc
//  and clang + -oc-use-gcc-libs
//

void generateMultiArchObjectFile(int &rc, int argc, char **argv,
                                 Target &target, int debug) {
#ifndef _WIN32
  std::string stdintmpfile;
  string_vector objs;
  std::stringstream obj;
  bool compile = false;
  size_t num = 0;

  if (!strcmp(argv[argc - 1], "-")) {
    //
    // fork() + reading from stdin isn't a good idea
    //

    std::stringstream file;
    std::string stdinsrc;
    std::string line;

    while (std::getline(std::cin, line)) {
      stdinsrc += line;
      stdinsrc += '\n';
    }

    file << "/tmp/" << getNanoSeconds() << "_stdin";

    if (target.isC())
      file << ".c";
    else if (target.isCXX())
      file << ".cpp";
    else if (target.isObjC())
      file << ".m";

    stdintmpfile = file.str();
    writeFileContent(stdintmpfile, stdinsrc);
    target.args[target.args.size() - 1] = stdintmpfile;
  }

  auto cleanup = [&]() {
    if (!stdintmpfile.empty())
      remove(stdintmpfile.c_str());
    for (auto &obj : objs)
      remove(obj.c_str());
  };

  if (!target.outputname) {
    bool f = false;

    for (auto &arg : target.args) {
      if (arg == "-c") {
        f = true;
        break;
      }
    }

    if (f && target.haveSourceFile()) {
      static std::string outputname;
      const char *ext = getFileExtension(target.sourcefile);
      size_t pos;

      if (*ext)
        outputname = std::string(target.sourcefile, ext - target.sourcefile);
      else
        outputname = target.sourcefile;

      outputname += ".o";

      if ((pos = outputname.find_last_of('/')) == std::string::npos)
        pos = 0;
      else
        ++pos;

      target.outputname = outputname.c_str() + pos;
    } else {
      if (f) {
        std::cerr << "source filename detection failed" << std::endl;
        std::cerr << "using a.out" << std::endl;
      }
      target.outputname = "a.out";
    }
  }

  const char *outputname = strrchr(target.outputname, '/');

  if (!outputname)
    outputname = target.outputname;
  else
    ++outputname;

  for (auto &arch : target.targetarch) {
    const char *archname = getArchName(arch);
    pid_t pid;
    ++num;

    clear(obj);
    obj << "/tmp/" << getNanoSeconds() << "_" << outputname << "_" << archname;

    objs.push_back(obj.str());
    pid = fork();

    if (pid > 0) {
      int status = 1;

      if (wait(&status) == -1) {
        std::cerr << "wait() failed" << std::endl;
        cleanup();
        rc = 1;
        break;
      }

      if (WIFEXITED(status)) {
        status = WEXITSTATUS(status);

        if (status) {
          rc = status;
          break;
        }
      } else {
        rc = 1;
        break;
      }
    } else if (pid == 0) {

      if (target.isGCC()) {
        // GCC
        bool is32bit = false;

        switch (arch) {
        case Arch::i386:
        case Arch::i486:
        case Arch::i586:
        case Arch::i686:
          is32bit = true;
        case Arch::x86_64:
          break;
        default:
          assert(false && "unsupported arch");
        }

        target.fargs.push_back(is32bit ? "-m32" : "-m64");
      } else if (target.isClang()) {
        // Clang
        target.fargs.push_back("-arch");
        target.fargs.push_back(getArchName(arch));
      } else {
        assert(false && "unsupported compiler");
      }

      target.fargs.push_back("-o");
      target.fargs.push_back(obj.str());

      if (target.usegcclibs) {
        target.setupGCCLibs(arch);

        if (target.langGiven()) {
          // -x must be added *after* the static libstdc++ *.a
          // otherwise clang thinks they are source files
          target.fargs.push_back("-x");
          target.fargs.push_back(target.lang);
        }
      }

      if (debug) {
        std::cerr << "[d] "
                  << "[" << num << "/" << target.targetarch.size()
                  << "] [compiling] " << archname << std::endl;
      }

      compile = true;
      break;
    } else {
      std::cerr << "fork() failed" << std::endl;
      rc = 1;
      break;
    }
  }

  if (!compile && !target.nocodegen && rc == -1) {
    std::string cmd;
    std::string lipo;
    std::string path;

    lipo = "x86_64-apple-";
    lipo += getDefaultTarget();
    lipo += "-lipo";

    if (getPathOfCommand(lipo.c_str(), path).empty()) {
      lipo = "lipo";

      if (getPathOfCommand(lipo.c_str(), path).empty()) {
        std::cerr << "cannot find lipo binary" << std::endl;
        rc = 1;
      }
    }

    if (rc == -1) {
      cmd.swap(path);
      cmd += "/";
      cmd += lipo;
      cmd += " -create ";

      for (auto &obj : objs) {
        cmd += obj;
        cmd += " ";
      }

      cmd += "-output ";
      cmd += target.outputname;

      if (debug) {
        std::cerr << "[d] [lipo] <-- " << cmd << std::endl;
      }

      rc = system(cmd.c_str());
      rc = WEXITSTATUS(rc);
    }
  }

  if (!compile)
    cleanup();
#else
  (void)rc;
  (void)argc;
  (void)argv;
  (void)target;
  (void)debug;
  std::cerr << __func__ << " not supported" << std::endl;
  rc = 1;
#endif
}

} // unnamed namespace

//
// Main routine
//

int main(int argc, char **argv) {
  char bbuf[sizeof(benchmark)];
  auto b = new (bbuf) benchmark;
  Target target;
  char **cargs = nullptr;
  int debug = 0;
  int rc = -1;

  if (!detectTarget(argc, argv, target)) {
    std::cerr << "cannot detect target" << std::endl;
    return 1;
  }

  if (char *p = getenv("OCDEBUG")) {
    debug = ((*p >= '1' && *p <= '9') ? *p - '0' + 0 : 0);
  }

  if (debug) {
    b->halt();

    if (debug >= 2) {
      std::cerr << "[d] detected target triple: " << target.getTriple()
                << std::endl;
      std::cerr << "[d] detected compiler: " << target.compiler << std::endl;

      std::cerr << "[d] detected stdlib: " << getStdLibString(target.stdlib)
                << std::endl;

      if (debug >= 3) {
        std::cerr << "[d] detected source file: "
                  << (target.sourcefile ? target.sourcefile : "-") << std::endl;

        std::cerr << "[d] detected language: " << target.getLangName()
                  << std::endl;
      }

      b->resume();
    }
  }

  concatEnvVariable("COMPILER_PATH", target.execpath);

  if (target.targetarch.size() > 1 && (target.usegcclibs || target.isGCC()))
    generateMultiArchObjectFile(rc, argc, argv, target, debug);

  auto printCommand = [&]() {
    std::string in;
    std::string out;

    for (int i = 0; i < argc; ++i) {
      in += argv[i];
      in += " ";
    }

    for (auto &arg : target.fargs) {
      out += arg;
      out += " ";
    }

    for (auto &arg : target.args) {
      out += arg;
      out += " ";
    }

    std::cerr << "[d] --> " << in << std::endl;
    std::cerr << "[d] <-- " << out << std::endl;
  };

  if (rc == -1) {
    cargs = new char *[target.fargs.size() + target.args.size() + 1];
    size_t i = 0;

    for (auto &arg : target.fargs)
      cargs[i++] = const_cast<char *>(arg.c_str());

    for (auto &arg : target.args)
      cargs[i++] = const_cast<char *>(arg.c_str());

    cargs[i] = nullptr;
  }

  if (debug) {
    time_type diff = b->getDiff();

    if (rc == -1)
      printCommand();

    std::cerr << "[d] === time spent in wrapper: " << diff / 1000000.0 << " ms"
              << std::endl;
  }

  if (rc == -1 && execvp(cargs[0], cargs)) {
    std::cerr << "invoking compiler failed" << std::endl;

    if (!debug)
      printCommand();

    return 1;
  }

  return rc;
}
