/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2016 by Thomas Poechtrager                      *
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
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
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

#include "tools.h"

namespace tools {

//
// Debugging
//

int getDebugLevel() {
  const char *value = getenv("OCDEBUG");
  return value ? std::max(atoi(value), 0) : 0;
}

//
// Terminal text colors
//

bool isTerminal() {
  static const bool val = isatty(fileno(stderr));
  return val;
}

//
// Executable path
//

#ifdef __OpenBSD__
namespace {

// OpenBSD cannot report the path of the running executable, so resolve
// argv[0] like the shell that started us: a path starting with '/' or '.'
// is resolved with realpath(), any other name is looked up in PATH.
std::string resolveArgv0(const std::string &argv0) {
  if (argv0[0] == '/' || argv0[0] == '.') {
    char *resolved = realpath(argv0.c_str(), nullptr);

    if (!resolved)
      return std::string();

    std::string path = resolved;
    free(resolved);
    return path;
  }

  const char *envpath = getenv("PATH");

  if (!envpath)
    return std::string();

  for (const std::string &dir : splitPath(envpath)) {
    const std::string candidate = dir + PATHDIV + argv0;
    struct stat st;

    if (!stat(candidate.c_str(), &st) && (st.st_mode & S_IXUSR))
      return candidate;
  }

  return std::string();
}

} // unnamed namespace
#endif

std::string getExecutableDir() {
#ifdef __APPLE__
  std::vector<char> buf(PATH_MAX + 1);
  uint32_t size = buf.size();

  if (_NSGetExecutablePath(buf.data(), &size) != 0 || !buf[0])
    return std::string();

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
  const std::string procdir = "/proc/" + std::to_string(ppid);
  std::string name;

  if (getFileContent(procdir + "/comm", name)) {
    if (!name.empty() && name.back() == '\n')
      name.pop_back();

    return name;
  }

  std::vector<char> buf(PATH_MAX + 1);
  const ssize_t size =
      readlink((procdir + "/exe").c_str(), buf.data(), buf.size() - 1);

  if (size > 0)
    return getFileName(buf.data());
#endif
  return "unknown";
}

//
// Environment
//

void prependEnvVariable(const char *var, const std::string &val) {
  std::string newval = val;

  if (const char *oldval = getenv(var)) {
    newval += ':';
    newval += oldval;
  }

  setenv(var, newval.c_str(), 1);
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

bool fileExists(const std::string &file) {
  struct stat st;
  return !stat(file.c_str(), &st);
}

bool dirExists(const std::string &dir) {
  struct stat st;
  return !stat(dir.c_str(), &st) && S_ISDIR(st.st_mode);
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
  const char *envpath = getenv("PATH");

  // splitPath() skips empty elements, so the current directory is never
  // searched.
  const std::vector<std::string> dirs =
      envpath ? splitPath(envpath) : std::vector<std::string>();

  for (const std::string &dir : dirs) {
    std::string path = dir + PATHDIV + file;
    struct stat st;

    if (stat(path.c_str(), &st) != 0)
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
  const size_t slash = file.rfind(PATHDIV);

  if (slash == std::string::npos)
    return ".";

  if (slash == 0)
    return std::string(1, PATHDIV);

  return file.substr(0, slash);
}

bool getProgramOutput(const std::vector<std::string> &args,
                      std::string &output) {
  output.clear();

  if (args.empty())
    return false;

  int fds[2];

  if (pipe(fds) != 0)
    return false;

  // An inherited SIG_IGN would discard the child status before waitpid().
  // Temporarily use the default action, also clearing SA_NOCLDWAIT.
  struct sigaction savedAction, action = {};
  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);

  if (sigaction(SIGCHLD, &action, &savedAction) != 0) {
    close(fds[0]);
    close(fds[1]);
    return false;
  }

  const pid_t pid = fork();

  if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    sigaction(SIGCHLD, &savedAction, nullptr);
    return false;
  }

  if (pid == 0) {
    close(fds[0]);

    // Install stdout first: the pipe may occupy a standard descriptor
    // when the caller started us with stdin or stderr closed.
    if (dup2(fds[1], STDOUT_FILENO) < 0)
      _exit(127);
    if (fds[1] != STDOUT_FILENO)
      close(fds[1]);

    const int devnull = open("/dev/null", O_RDWR);

    if (devnull < 0 || dup2(devnull, STDIN_FILENO) < 0 ||
        dup2(devnull, STDERR_FILENO) < 0)
      _exit(127);
    if (devnull > STDERR_FILENO)
      close(devnull);

    std::vector<char *> argv;
    argv.reserve(args.size() + 1);

    for (const std::string &arg : args)
      argv.push_back(const_cast<char *>(arg.c_str()));

    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(fds[1]);

  char buf[4096];

  for (;;) {
    const ssize_t n = read(fds[0], buf, sizeof(buf));

    if (n > 0)
      output.append(buf, static_cast<size_t>(n));
    else if (n == 0 || errno != EINTR)
      break;
  }

  close(fds[0]);

  int status;
  pid_t waited;

  do {
    waited = waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);

  const bool success =
      waited == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  sigaction(SIGCHLD, &savedAction, nullptr);
  return success;
}

void stripFileName(std::string &path) {
  size_t lastpathdiv = path.find_last_of(PATHDIV);
  if (lastpathdiv != 0 && lastpathdiv != std::string::npos)
    path.resize(lastpathdiv);
}

const char *getFileName(const char *file) {
  const char *p = strrchr(file, PATHDIV);
  return p ? p + 1 : file;
}

//
// OSVersion
//

OSVersion parseOSVersion(const char *OSVer) {
  OSVersion OSNum;
  const char *p = OSVer;

  for (int *part : {&OSNum.major, &OSNum.minor, &OSNum.patch}) {
    *part = atoi(p);
    if (!(p = strchr(p, '.')))
      break;
    ++p;
  }

  return OSNum;
}

bool OSVersion::matchesString(const char *str) const {
  switch (std::count(str, str + strlen(str), '.')) {
  case 0:
    return majorStr() == str;
  case 1:
    return shortStr() == str;
  case 2:
    return Str() == str;
  default:
    return false;
  }
}

} // namespace tools
