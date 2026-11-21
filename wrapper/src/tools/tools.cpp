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
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <libproc.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#include <sys/user.h>
#endif

#ifdef __FreeBSD__
#include <libutil.h>
#endif

#include "tools/tools.h"

namespace tools {

//
// Misc helper tools
//

void appendUTF8(std::string &out, unsigned code) {
  if (code < 0x80) {
    out += static_cast<char>(code);
  } else if (code < 0x800) {
    out += static_cast<char>(0xC0 | (code >> 6));
    out += static_cast<char>(0x80 | (code & 0x3F));
  } else if (code < 0x10000) {
    out += static_cast<char>(0xE0 | (code >> 12));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (code >> 18));
    out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
  }
}

//
// Debugging
//

int getDebugLevel() {
  // OCDEBUG does not change while the wrapper runs, so read it once.
  static const int DebugLevel = [] {
    const char *value = getenv("OCDEBUG");
    return value ? std::max(atoi(value), 0) : 0;
  }();
  return DebugLevel;
}

//
// Terminal text colors
//

bool isTerminal() {
  static const bool StderrIsTerminal = isatty(fileno(stderr));
  return StderrIsTerminal;
}

//
// Executable path
//

#ifdef __OpenBSD__
namespace {

// OpenBSD cannot report the path of the running executable, so resolve
// argv[0] like the shell that started us: a name with a '/' is a path, any
// other name is looked up in PATH. Symlinks are resolved, as on Linux and
// FreeBSD, where the kernel reports the resolved path: started through a
// symlink elsewhere, the directory is still the installation's bin/.
std::string resolveArgv0(const std::string &argv0) {
  if (argv0.find('/') == std::string::npos)
    return findExecutableInPath(argv0);

  char *resolved = realpath(argv0.c_str(), nullptr);

  if (!resolved)
    return std::string();

  const std::string path = resolved;
  free(resolved);
  return path;
}

} // unnamed namespace
#endif

std::string getExecutableDir() {
#ifdef __APPLE__
  std::vector<char> buf(PATH_MAX + 1);
  uint32_t size = buf.size();

  if (_NSGetExecutablePath(buf.data(), &size) != 0 || !buf[0])
    return std::string();

  // This is the path the program was started with, possibly a symlink.
  // Resolve it, as on Linux and FreeBSD, where the kernel reports the
  // resolved path.
  if (char *resolved = realpath(buf.data(), nullptr)) {
    const std::string dir = getDirName(resolved);
    free(resolved);
    return dir;
  }

  return getDirName(buf.data());
#elif defined(__FreeBSD__) || defined(__DragonFly__)
  int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
  std::vector<char> buf(PATH_MAX + 1);
  size_t size = buf.size();

  if (sysctl(mib, 4, buf.data(), &size, nullptr, 0) != 0 || !size)
    return std::string();

  buf.back() = '\0';

  if (!buf[0])
    return std::string();

  return getDirName(buf.data());
#elif defined(__OpenBSD__)
  int mib[4] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };
  size_t size = 0;

  if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0)
    return std::string();

  // The kernel stores the argv pointer array followed by the strings.
  std::vector<char *> argv(size / sizeof(char *) + 1);

  if (sysctl(mib, 4, argv.data(), &size, nullptr, 0) != 0 || !argv[0])
    return std::string();

  const std::string path = resolveArgv0(argv[0]);

  if (path.empty())
    return std::string();

  return getDirName(path);
#else
  std::vector<char> buf(PATH_MAX + 1);
  const ssize_t size = readlink("/proc/self/exe", buf.data(), buf.size());

  // A result that fills the whole buffer may have been truncated.
  if (size <= 0 || static_cast<size_t>(size) >= buf.size())
    return std::string();

  return getDirName(std::string(buf.data(), size));
#endif
}

std::string getParentProcessName() {
  const pid_t ppid = getppid();
#ifdef __APPLE__
  char path[PROC_PIDPATHINFO_MAXSIZE];

  if (proc_pidpath(ppid, path, sizeof(path)) > 0)
    return getFileName(path);
#elif defined(__FreeBSD__)
  if (struct kinfo_proc *proc = kinfo_getproc(ppid)) {
    const std::string name = getFileName(proc->ki_comm);
    free(proc);
    return name;
  }
#else
  const std::string procDir = "/proc/" + std::to_string(ppid);
  std::string name;

  if (getFileContent(procDir + "/comm", name)) {
    if (!name.empty() && name.back() == '\n')
      name.pop_back();

    return name;
  }

  std::vector<char> buf(PATH_MAX + 1);
  const ssize_t size =
      readlink((procDir + "/exe").c_str(), buf.data(), buf.size() - 1);

  if (size > 0)
    return getFileName(buf.data());
#endif
  return "unknown";
}

//
// Environment
//

void prependEnvVariable(const char *var, const std::string &val) {
  std::string newVal = val;

  if (const char *oldVal = getenv(var)) {
    newVal += ':';
    newVal += oldVal;
  }

  setenv(var, newVal.c_str(), 1);
}

std::string escapePath(const std::string &path) {
  std::string escaped;

  for (const char c : path) {
    switch (c) {
    case '"':
    case '\'':
    case '\\':
    case '$':
    case '(':
    case ')':
    case ' ':
    case ';':
    case ':':
      escaped += '\\';
    }
    escaped += c;
  }

  return escaped;
}

std::vector<std::string> splitPath(const std::string &path) {
  std::vector<std::string> dirs;
  size_t start = 0;

  while (start <= path.size()) {
    size_t end = path.find(':', start);

    if (end == std::string::npos)
      end = path.size();

    // Skip empty elements ("a::b", leading or trailing ':').
    if (end > start)
      dirs.push_back(path.substr(start, end - start));

    start = end + 1;
  }

  return dirs;
}

std::string joinPath(const std::vector<std::string> &path) {
  std::string joined;

  for (size_t i = 0; i < path.size(); ++i) {
    if (i > 0)
      joined += ':';

    joined += escapePath(path[i]);
  }

  return joined;
}

bool hasPath(const std::vector<std::string> &path, const std::string &dir) {
  return std::find(path.begin(), path.end(), dir) != path.end();
}

//
// Files and Directories
//

bool getFileContent(const std::string &file, std::string &content) {
  std::ifstream f(file.c_str());

  if (!f.is_open())
    return false;

  // operator<< catches read errors (for example EISDIR) and only sets
  // failbit, so a directory gives an empty string instead of an exception.
  std::ostringstream ss;
  ss << f.rdbuf();
  content = ss.str();
  return true;
}

bool getFileContent(int fd, std::string &content) {
  char buffer[4096];
  content.clear();

  for (;;) {
    const ssize_t n = read(fd, buffer, sizeof(buffer));

    if (n > 0)
      content.append(buffer, static_cast<size_t>(n));
    else if (n == 0)
      return true;
    else if (errno != EINTR)
      return false;
  }
}

bool writeFileContent(int fd, const std::string &content) {
  size_t written = 0;

  while (written < content.size()) {
    const ssize_t n =
        write(fd, content.data() + written, content.size() - written);

    if (n < 0 && errno == EINTR)
      continue;

    if (n < 0)
      return false;

    if (n == 0) {
      errno = EIO;
      return false;
    }

    written += static_cast<size_t>(n);
  }

  return true;
}

std::string getTempDir() {
  const char *dir = getenv("TMPDIR");

  if (dir && *dir && dirExists(dir) && !access(dir, W_OK | X_OK))
    return dir;

  return "/tmp";
}

bool fileExists(const std::string &file) {
  struct stat st;
  return !stat(file.c_str(), &st);
}

bool dirExists(const std::string &dir) {
  struct stat st;
  return !stat(dir.c_str(), &st) && S_ISDIR(st.st_mode);
}

bool isExecutableFile(const std::string &file) {
  struct stat st;

  // access() alone is not enough: POSIX lets it succeed for the superuser
  // even if no execute bit is set. Like a shell, skip such files.
  return !stat(file.c_str(), &st) && S_ISREG(st.st_mode) &&
         (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) &&
         !access(file.c_str(), X_OK);
}

std::vector<std::string> listFiles(const std::string &dir) {
  std::vector<std::string> files;

  if (DIR *d = opendir(dir.c_str())) {
    while (const dirent *de = readdir(d))
      files.push_back(de->d_name);

    closedir(d);
  }

  return files;
}

bool ignoreCCACHE(const std::string &path) {
  return strncmp(getFileName(path), "ccache", 6) != 0;
}

std::string findExecutableInPath(const std::string &file, bool resolve,
                                 bool (*accept)(const std::string &path)) {
  const char *envPath = getenv("PATH");

  // splitPath() skips empty elements, so the current directory is never
  // searched.
  const std::vector<std::string> dirs =
      envPath ? splitPath(envPath) : std::vector<std::string>();

  for (const std::string &dir : dirs) {
    std::string path = dir + PathDiv + file;

    if (!isExecutableFile(path))
      continue;

    if (resolve) {
      char *resolved = realpath(path.c_str(), nullptr);

      if (!resolved)
        continue;

      path = resolved;
      free(resolved);
    }

    if (!accept || accept(path))
      return path;
  }

  return std::string();
}

std::string getDirName(const std::string &file) {
  const size_t slash = file.rfind(PathDiv);

  if (slash == std::string::npos)
    return ".";

  if (slash == 0)
    return std::string(1, PathDiv);

  return file.substr(0, slash);
}

void stripFileName(std::string &path) {
  size_t lastPathDiv = path.find_last_of(PathDiv);
  if (lastPathDiv != 0 && lastPathDiv != std::string::npos)
    path.resize(lastPathDiv);
}

const char *getFileName(const char *file) {
  const char *p = strrchr(file, PathDiv);
  return p ? p + 1 : file;
}

//
// OSVersion
//

OSVersion parseOSVersion(const char *osVer, bool strict) {
  OSVersion osNum;
  int *const parts[] = {&osNum.major, &osNum.minor, &osNum.patch};
  const char *p = osVer;
  bool endsWithNumber = false;

  // Like atoi(), the lenient form skips leading whitespace and a '+', as in
  // MACOSX_DEPLOYMENT_TARGET=" 10.9".
  if (!strict) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      ++p;

    if (*p == '+')
      ++p;
  }

  // Up to three numbers of up to six digits, separated by dots.
  for (int i = 0; i < 3; ++i) {
    int digits = 0;

    for (; *p >= '0' && *p <= '9' && digits < 6; ++p, ++digits)
      *parts[i] = *parts[i] * 10 + (*p - '0');

    endsWithNumber = digits > 0;

    if (!endsWithNumber || *p != '.' || i == 2)
      break;

    ++p;
  }

  // Strict: nothing may follow, not even a dot.
  if (strict && (!endsWithNumber || *p))
    return OSVersion();

  return osNum;
}

bool OSVersion::matchesString(const char *versionStr) const {
  switch (std::count(versionStr, versionStr + strlen(versionStr), '.')) {
  case 0:
    return majorStr() == versionStr;
  case 1:
    return shortStr() == versionStr;
  case 2:
    return str() == versionStr;
  default:
    return false;
  }
}

} // namespace tools
