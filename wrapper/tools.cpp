/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014, 2015 by Thomas Poechtrager                     *
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

#include <vector>
#include <string>
#include <sstream>
#include <istream>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cassert>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#else
#include <windows.h>
#include <tlhelp32.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <libproc.h>
#endif

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <libutil.h>
#endif

#include "tools.h"

namespace tools {

char *getExecutablePath(char *buf, size_t len) {
  char *p;
#ifdef __APPLE__
  unsigned int l = len;
  if (_NSGetExecutablePath(buf, &l) != 0)
    return nullptr;
#elif defined(__FreeBSD__)
  int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
  size_t l = len;
  if (sysctl(mib, 4, buf, &l, nullptr, 0) != 0)
    return nullptr;
#elif defined(_WIN32)
  size_t l = GetModuleFileName(nullptr, buf, (DWORD)len);
#else
  ssize_t l = readlink("/proc/self/exe", buf, len);
#endif
  if (l <= 0)
    return nullptr;
  buf[len - 1] = '\0';
  p = strrchr(buf, PATHDIV);
  if (p) {
    *p = '\0';
  }
  return buf;
}

const std::string &getParentProcessName() {
  static std::string name;
#ifdef _WIN32
  HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 pe;

  auto zerope = [&]() {
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(PROCESSENTRY32);
  };

  zerope();

  auto pid = GetCurrentProcessId();
  decltype(pid) ppid = -1;

  if (Process32First(h, &pe)) {
    do {
      if (pe.th32ProcessID == pid) {
        ppid = pe.th32ParentProcessID;
        break;
      }
    } while (Process32Next(h, &pe));
  }

  if (ppid != static_cast<decltype(ppid)>(-1)) {
    PROCESSENTRY32 *ppe = nullptr;
    zerope();

    if (Process32First(h, &pe)) {
      do {
        std::cout << pe.szExeFile << " " << pe.th32ProcessID << std::endl;
        if (pe.th32ProcessID == ppid) {
          ppe = &pe;
          break;
        }
      } while (Process32Next(h, &pe));
    }

    if (ppe) {
      char *p = strrchr(ppe->szExeFile, '\\');
      if (p) {
        name = p + 1;
      } else {
        name = ppe->szExeFile;
      }
    }
  }

  CloseHandle(h);

  if (!name.empty()) {
    return name;
  }
#else
  auto getName = [](const char * path)->const char * {
    if (const char *p = strrchr(path, '/')) {
      return p + 1;
    }
    return path;
  };
  auto ppid = getppid();
#ifdef __APPLE__
  char path[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(ppid, path, sizeof(path))) {
    name = getName(path);
    return name;
  }
#elif defined(__FreeBSD__)
  struct kinfo_proc *proc = kinfo_getproc(ppid);
  if (proc) {
    name = getName(proc->ki_comm);
    free(proc);
    return name;
  }
#else
  std::stringstream file;
  file << "/proc/" << ppid << "/comm";
  if (getFileContent(file.str(), name)) {
    if (!name.empty() && name.rbegin()[0] == '\n') {
      name.resize(name.size() - 1);
    }
    return name;
  } else {
    clear(file);
    file << "/proc/" << ppid << "/exe";
    char buf[PATH_MAX + 1];
    if (readlink(file.str().c_str(), buf, sizeof(buf)) > 0) {
      buf[PATH_MAX] = '\0';
      name = getName(buf);
      return name;
    }
  }
#endif
#endif
  name = "unknown";
  return name;
}

#ifdef _WIN32
std::string &fixPathDiv(std::string &path) {
  for (auto &c : path) {
    if (c == '/')
      c = '\\';
  }
  return path;
}
#endif

//
// Environment
//

void concatEnvVariable(const char *var, const std::string &val) {
  std::string nval = val;
  if (char *oldval = getenv(var)) {
    nval += ":";
    nval += oldval;
  }
  setenv(var, nval.c_str(), 1);
}

//
// Files and Directories
//

std::string *getFileContent(const std::string &file, std::string &content) {
  std::ifstream f(file.c_str());

  if (!f.is_open())
    return nullptr;

  f.seekg(0, std::ios::end);
  auto len = f.tellg();
  f.seekg(0, std::ios::beg);

  if (len != static_cast<decltype(len)>(-1))
    content.reserve(static_cast<size_t>(f.tellg()));

  content.assign(std::istreambuf_iterator<char>(f),
                 std::istreambuf_iterator<char>());

  return &content;
}

bool writeFileContent(const std::string &file, const std::string &content) {
  std::ofstream f(file.c_str());

  if (!f.is_open())
    return false;

  f << content;
  return f.good();
}

bool fileExists(const std::string &dir) {
  struct stat st;
  return !stat(dir.c_str(), &st);
}

bool dirExists(const std::string &dir) {
  struct stat st;
  return !stat(dir.c_str(), &st) && S_ISDIR(st.st_mode);
}

typedef bool (*listfilescallback)(const char *file);

bool isDirectory(const char *file, const char *prefix) {
  struct stat st;
  if (prefix) {
    std::string tmp = prefix;
    tmp += "/";
    tmp += file;
    return !stat(tmp.c_str(), &st) && S_ISDIR(st.st_mode);
  } else {
    return !stat(file, &st) && S_ISDIR(st.st_mode);
  }
}

bool listFiles(const char *dir, std::vector<std::string> *files,
               listfilescallback cmp) {
#ifndef _WIN32
  DIR *d = opendir(dir);
  dirent *de;

  if (!d)
    return false;

  while ((de = readdir(d))) {
    if ((!cmp || cmp(de->d_name)) && files) {
      files->push_back(de->d_name);
    }
  }

  closedir(d);
  return true;
#else
  WIN32_FIND_DATA fdata;
  HANDLE handle;

  handle = FindFirstFile(dir, &fdata);

  if (handle == INVALID_HANDLE_VALUE)
    return false;

  do {
    if ((!cmp || cmp(fdata.cFileName)) && files) {
      files->push_back(fdata.cFileName);
    }
  } while (FindNextFile(handle, &fdata));

  FindClose(handle);

  return true;
#endif
}

typedef bool (*realpathcmp)(const char *file, const struct stat &st);

bool isExecutable(const char *f, const struct stat &) {
  return !access(f, F_OK | X_OK);
}

std::string &realPath(const char *file, std::string &result, realpathcmp cmp) {
  char *PATH = getenv("PATH");
  const char *p = PATH ? PATH : "";
  std::string sfile;
  struct stat st;

  do {
    if (*p == ':')
      ++p;

    while (*p && *p != ':')
      sfile += *p++;

    sfile += "/";
    sfile += file;

    if (!stat(sfile.c_str(), &st) && (!cmp || cmp(sfile.c_str(), st)))
      break;

    sfile.clear();
  } while (*p);

#ifndef _WIN32
  if (!sfile.empty()) {
    char buf[PATH_MAX + 1];
    ssize_t len;

    if ((len = readlink(sfile.c_str(), buf, PATH_MAX)) != -1)
      result.assign(buf, len);
  }
#endif

  result.swap(sfile);
  return result;
}

std::string &getPathOfCommand(const char *command, std::string &result) {
  realPath(command, result, isExecutable);

  const size_t len = strlen(command) + 1;

  if (result.size() < len) {
    result.clear();
    return result;
  }

  result.resize(result.size() - len);
  return result;
}

const char *getFileName(const char *file) {
  const char *p = strrchr(file, PATHDIV);

  if (!p)
    p = file;
  else
    ++p;

  return p;
}

const char *getFileExtension(const char *file) {
  const char *p = strrchr(file, '.');

  if (!p)
    p = "";

  return p;
}

//
// Time
//

time_type getNanoSeconds() {
#ifdef __APPLE__
  union {
    AbsoluteTime at;
    time_type ull;
  } tmp;
  tmp.ull = mach_absolute_time();
  Nanoseconds ns = AbsoluteToNanoseconds(tmp.at);
  tmp.ull = UnsignedWideToUInt64(ns);
  return tmp.ull;
#elif defined(__linux__)
  struct timespec tp;
  if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
    return static_cast<time_type>((tp.tv_sec * 1000000000LL) + tp.tv_nsec);
#endif
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) == 0)
    return static_cast<time_type>((tv.tv_sec * 1000000000LL) +
                                  (tv.tv_usec * 1000));
  abort();
}

//
// OSVersion
//

OSVersion parseOSVersion(const char *OSVer) {
  const char *p = OSVer;
  OSVersion OSNum;

  OSNum.major = atoi(p);

  while (*p && *p++ != '.')
    ;
  if (!*p)
    return OSNum;

  OSNum.minor = atoi(p);

  while (*p && *p++ != '.')
    ;
  if (!*p)
    return OSNum;

  OSNum.patch = atoi(p);
  return OSNum;
}

//
// OS Compat
//

#ifdef _WIN32
int setenv(const char *name, const char *value, int overwrite) {
  std::string buf;
  (void)overwrite; // TODO

  buf = name;
  buf += '=';
  buf += value;

  return putenv(buf.c_str());
}

int unsetenv(const char *name) { return setenv(name, "", 1); }

int execvp(const char *file, char *const argv[]) {
  (void)file;
  (void)argv;

  return 1;
}
#endif

} // namespace tools
