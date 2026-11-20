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
#include <cstring>
#include <string>
#include <vector>

#include "tools/tools.h"
#include "tools/process.h"
#include "target.h"
#include "programs/programs.h"

extern char **environ;
using namespace tools;

namespace program {
namespace host {
namespace {

struct EnvVar {
  std::string name;
  std::string value;
  EnvVar() {}
  EnvVar(std::string name, std::string value) : name(name), value(value) {}
};

EnvVar &var(const char *p, EnvVar &envVar, const bool skipVal = false) {
  if (!p)
    return envVar;

  const char *separator = strchr(p, '=');
  if (!separator)
    return envVar;

  envVar.name.assign(p, separator - p);

  if (!skipVal)
    envVar.value.assign(separator + 1);

  return envVar;
}

} // unnamed namespace

int pkg_config(int argc, char **argv, Target &target) {
  std::vector<EnvVar> envVars;
  std::vector<EnvVar> unset;
  EnvVar envVar;
  bool useNativeVariables = getenv("OSXCROSS_PKG_CONFIG_USE_NATIVE_VARIABLES");

  if (!getenv("OSXCROSS_PKG_CONFIG_NO_MP_INC")) {
    std::string macPortsSysRoot;

    if (target.getMacPortsSysRootDir(macPortsSysRoot)) {
      const char *sysRootVar = useNativeVariables
                                   ? "PKG_CONFIG_SYSROOT_DIR"
                                   : "OSXCROSS_PKG_CONFIG_SYSROOT_DIR";
      const char *userSysRoot = getenv(sysRootVar);

      // The sysroot is a single directory, not a search path. As with plain
      // pkg-config, one set by the user is kept. The MacPorts packages only
      // work with their own sysroot, so they are left out then.
      if (!userSysRoot || !*userSysRoot) {
        setenv(sysRootVar, macPortsSysRoot.c_str(), 1);

        std::string macPortsPkgConfigPath;

        if (target.getMacPortsPkgConfigDir(macPortsPkgConfigPath))
          prependEnvVariable(useNativeVariables ? "PKG_CONFIG_PATH"
                                                : "OSXCROSS_PKG_CONFIG_PATH",
                             macPortsPkgConfigPath);
      }
    }
  } else {
    unsetenv("OSXCROSS_PKG_CONFIG_NO_MP_INC");
  }

  if (!useNativeVariables) {
    // Map OSXCROSS_PKG_* to PKG_*.
    for (char **env = environ; *env; ++env) {
      char *p = *env;

      if (!strncmp(p, "OSXCROSS_PKG_CONFIG", 19)) {
        p += 9; // skip OSXCROSS_
        envVars.push_back(var(p, envVar));
      } else if (!strncmp(p, "PKG_CONFIG", 10)) {
        // Unset native pkg-config vars.
        unset.push_back(var(p, envVar, true));
      }
    }
  }

  if (useNativeVariables || !envVars.empty()) {
    if (!useNativeVariables) {
      for (const EnvVar &envVar : unset) {
        warn << argv[0] << ": ignoring environment variable '" << envVar.name
             << "' - please see README.PKG-CONFIG.md for more" << warn.endl();

        unsetenv(envVar.name.c_str());
      }

      for (const EnvVar &envVar : envVars)
        setenv(envVar.name.c_str(), envVar.value.c_str(), 1);
    }

    // Prevent pkg-config from looking for *.pc files
    // in pre-defined search paths, such as /usr.
    if (!getenv("PKG_CONFIG_LIBDIR"))
      setenv("PKG_CONFIG_LIBDIR", "", 1);

    return process::execProgram("pkg-config",
                                std::vector<std::string>(argv, argv + argc));
  }

  warn << argv[0] << " is a no-op - please see README.PKG-CONFIG.md for more"
       << warn.endl();

  return 1;
}

} // namespace host
} // namespace program
