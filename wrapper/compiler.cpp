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
 * Important:
 *  - Avoid the use of C++11 headers
 *  - Avoid std:: for C functions
 *
 * Any other C++11 features can be used as long they are supported
 * by Clang 3.2.
 *
 * Debug messages can be enabled by setting 'OCDEBUG' (ENV) to 1.
 *
 * TODO:
 *  - handle MACOSX_DEPLOYMENT_TARGET (env)
 *
 */

#include "compat.h"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <cassert>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
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

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

#include "oscompat.h"

#undef check
#undef major
#undef minor
#undef patch

namespace {

//
// Misc helper tools
//

typedef std::vector<std::string> string_vector;

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
  p = strrchr(buf, '/');
  if (*p) {
    *p = '\0';
  }
  return buf;
}

__attribute__((unused)) std::string &fixPathDiv(std::string &path) {
#ifdef _WIN32
  for (auto &c : path) {
    if (c == '/')
      c = '\\';
  }
#else
// let's assume the compiler is smart enough
// to optimize this function call away
#endif
  return path;
}

__attribute__((unused)) std::string *getFileContent(const std::string &file,
                                                    std::string &content) {
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
    file.str(std::string());
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

void concatEnvVariable(const char *var, const std::string val) {
  std::string nval = val;
  if (char *oldval = getenv(var)) {
    nval += ":";
    nval += oldval;
  }
  setenv(var, nval.c_str(), 1);
}

bool dirExists(const std::string &dir) {
  struct stat st;
  return !stat(dir.c_str(), &st) && S_ISDIR(st.st_mode);
}

typedef bool (*listfilescallback)(const char *file);

bool isDirectory(const char *file, const char *prefix = nullptr) {
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
               listfilescallback cmp = nullptr) {
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

  return files->size();
#endif
}

typedef bool (*realpathcmp)(const char *file, const struct stat &st);

bool isExecutable(const char *f, const struct stat &) {
  return !access(f, F_OK | X_OK);
}

std::string &realPath(const char *file, std::string &result,
                      realpathcmp cmp = nullptr) {
  char *PATH = getenv("PATH");
  const char *p = PATH;
  std::string sfile;
  struct stat st;

  assert(PATH);

  do {
    if (*p == ':')
      ++p;

    while (*p && *p != ':')
      sfile += *p++;

    sfile += "/";
    sfile += file;

    if (!stat(sfile.c_str(), &st) && (!cmp || cmp(sfile.c_str(), st))) {
      break;
    }

    sfile.clear();
  } while (*p);

#ifndef _WIN32
  if (!sfile.empty()) {
    char buf[PATH_MAX + 1];
    ssize_t len;

    if ((len = readlink(sfile.c_str(), buf, PATH_MAX)) != -1) {
      result.assign(buf, len);
    }
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

typedef unsigned long long time_type;

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

class benchmark {
public:
  benchmark() { s = getTime(); }

  time_type getDiff() { return getTime() - s; }

  void halt() { h = getTime(); }
  void resume() { s += getTime() - h; }

  ~benchmark() {
    time_type diff = getTime() - s;
    std::cerr << "took: " << diff / 1000000.0 << " ms" << std::endl;
  }

private:
  __attribute__((always_inline)) time_type getTime() {
    return getNanoSeconds();
  }

  time_type h;
  time_type s;
};

//
// OSVersion struct to ease OS Version comparison
//

struct OSVersion {
  constexpr OSVersion(int major, int minor, int patch = 0)
      : major(major), minor(minor), patch(patch) {}
  constexpr OSVersion() : major(), minor(), patch() {}

  constexpr int Num() const {
    return major * 10000 + minor * 100 + patch;
  };

  constexpr bool operator>(const OSVersion &OSNum) const {
    return Num() > OSNum.Num();
  }

  constexpr bool operator>=(const OSVersion &OSNum) const {
    return Num() >= OSNum.Num();
  }

  constexpr bool operator<(const OSVersion &OSNum) const {
    return Num() < OSNum.Num();
  }

  constexpr bool operator<=(const OSVersion &OSNum) const {
    return Num() <= OSNum.Num();
  }

  constexpr bool operator!=(const OSVersion &OSNum) const {
    return Num() != OSNum.Num();
  }

  bool operator!=(const char *val) const {
    size_t c = 0;
    const char *p = val;

    while (*p) {
      if (*p++ == '.')
        ++c;
    }

    switch (c) {
    case 1:
      return shortStr() != val;
    case 2:
      return Str() != val;
    default:
      return true;
    }
  }

  std::string Str() const {
    std::stringstream tmp;
    tmp << major << "." << minor << "." << patch;
    return tmp.str();
  }

  std::string shortStr() const {
    std::stringstream tmp;
    tmp << major << "." << minor;
    return tmp.str();
  }

  int major;
  int minor;
  int patch;
};

static_assert(OSVersion(10, 6) != OSVersion(10, 5), "");

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

typedef OSVersion GCCVersion;
#define parseGCCVersion parseOSVersion

typedef OSVersion ClangVersion;
#define parseClangVersion parseOSVersion

//
// Default values for the Target struct
//

constexpr const char *getDefaultVendor() { return "apple"; }
constexpr const char *getDefaultTarget() { return OSXCROSS_TARGET; }
constexpr const char *getDefaultCompiler() { return "clang"; }
constexpr const char *getDefaultCXXCompiler() { return "clang++"; }
constexpr const char *getLinkerVersion() { return OSXCROSS_LINKER_VERSION; }

constexpr const char *getLibLTOPath() {
#ifdef OSXCROSS_LIBLTO_PATH
  return OSXCROSS_LIBLTO_PATH;
#else
  return nullptr;
#endif
}

constexpr const char *getOSXCrossVersion() {
#ifdef OSXCROSS_VERSION
  return OSXCROSS_VERSION[0] ? OSXCROSS_VERSION : "unknown";
#else
  return "unknown";
#endif
}

const char *getDefaultCStandard() { return getenv("OSXCROSS_C_STANDARD"); }
const char *getDefaultCXXStandard() { return getenv("OSXCROSS_CXX_STANDARD"); }

#ifdef OSXCROSS_OSX_VERSION_MIN
OSVersion getDefaultMinTarget() {
  if (!strcmp(OSXCROSS_OSX_VERSION_MIN, "default"))
    return OSVersion();

  return parseOSVersion(OSXCROSS_OSX_VERSION_MIN);
}
#else
constexpr OSVersion getDefaultMinTarget() { return OSVersion(); }
#endif

//
// Arch
//

enum Arch {
  x86_64,
  i386,
  unknown
};

constexpr const char *ArchNames[] = { "x86_64", "i386", "unknown" };
constexpr const char *ArchNames2[] = { "x86_64", "i686", "unknown" };

constexpr const char *getArchName(Arch arch) { return ArchNames[arch]; }

constexpr const char *getArchName2(Arch arch) { return ArchNames2[arch]; }

Arch parseArch(const char *arch) {
  if (arch[0] == 'i') {
    if (!strcmp(arch, "i386") || !strcmp(arch, "i486") ||
        !strcmp(arch, "i586") || !strcmp(arch, "i686")) {
      return Arch::i386;
    }
  } else if (!strcmp(arch, "x86_64")) {
    return Arch::x86_64;
  }
  return Arch::unknown;
}

//
// Stdlib
//

enum StdLib {
  unset,
  libcxx,
  libstdcxx
};

constexpr const char *StdLibNames[] = { "default", "libc++", "libstdc++" };

constexpr const char *getStdLibString(StdLib stdlib) {
  return StdLibNames[stdlib];
}

//
// Target struct
//

struct Target {
  Target()
      : vendor(getDefaultVendor()), target(getDefaultTarget()),
        stdlib(StdLib::unset), usegcclibs(), compiler(getDefaultCompiler()),
        lang(), langstd(), sourcefile(), outputname() {
    if (!getExecutablePath(execpath, sizeof(execpath)))
      abort();
  }

  OSVersion getSDKOSNum() const {
    if (target.size() < 7)
      return OSVersion();

    int n = atoi(target.c_str() + 6);
    return OSVersion(10, 4 + (n - 8));
  }

  bool getSDKPath(std::string &path) const {
    OSVersion SDKVer = getSDKOSNum();

    path = execpath;
    path += "/../SDK/MacOSX";
    path += SDKVer.shortStr();

    if (SDKVer <= OSVersion(10, 4))
      path += "u";

    path += ".sdk";
    return dirExists(path);
  }

  void addArch(const Arch arch) {
    auto &v = targetarch;
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i] == arch) {
        v.erase(v.begin() + i);
        addArch(arch);
        return;
      }
    }
    v.push_back(arch);
  }

  bool hasLibCXX() const { return getSDKOSNum() >= OSVersion(10, 7); }

  bool libCXXIsDefaultCXXLib() const {
    return stdlib != libstdcxx && hasLibCXX() && OSNum >= OSVersion(10, 9);
  }

  bool isLibCXX() const {
    return stdlib == StdLib::libcxx || libCXXIsDefaultCXXLib();
  }

  bool isLibSTDCXX() const { return stdlib == StdLib::libstdcxx; }

  bool haveSourceFile() { return sourcefile != nullptr; }
  bool haveOutputName() { return outputname != nullptr; }

  bool isC(bool r = false) {
    if (!r && isCXX())
      return false;

    if (langGiven() && lang[0] != 'o' &&
        (!strcmp(lang, "c") || !strcmp(lang, "c-header")))
      return true;

    if (haveSourceFile()) {
      const char *ext = strrchr(sourcefile, '.');

      if (ext && !strcmp(ext, ".c"))
        return true;
    }

    return compiler.find("++") == std::string::npos && !isObjC(true);
  }

  bool isCXX() {
    bool CXXCompiler = compiler.find("++") != std::string::npos;

    if (!langGiven() && CXXCompiler && !isObjC(true))
      return true;

    if (langGiven() && !strncmp(lang, "c++", 3))
      return true;

    constexpr const char *CXXFileExts[] = { ".C",   ".cc", ".cpp", ".CPP",
                                            ".c++", ".cp", ".cxx" };

    if (haveSourceFile()) {
      const char *ext = strrchr(sourcefile, '.');

      if (ext) {
        for (auto &cxxfe : CXXFileExts) {
          if (!strcmp(ext, cxxfe))
            return true;
        }
      }
    }

    return CXXCompiler && !isC(true) && !isObjC(true);
  }

  bool isObjC(bool r = false) {
    if (!r && isCXX())
      return false;

    if (langGiven() && lang[0] == 'o')
      return true;

    if (haveSourceFile()) {
      const char *ext = strrchr(sourcefile, '.');

      if (ext && (!strcmp(ext, ".m") || !strcmp(ext, ".mm")))
        return true;
    }

    return false;
  }

  bool isGCH() {
    if (haveOutputName()) {
      const char *ext = strrchr(outputname, '.');

      if (!ext)
        return false;

      return !strcmp(ext, ".gch");
    }
    return false;
  }

  bool isClang() const { return !compiler.compare(0, 4, "clang", 4); }

  bool isGCC() const {
    return !compiler.compare(0, 3, "gcc") || !compiler.compare(0, 3, "g++");
  }

  bool isKnownCompiler() const { return isClang() || isGCC(); }

  bool langGiven() const { return lang != nullptr; }
  bool langStdGiven() const { return langstd != nullptr; }

  const char *getLangName() {
    if (isC())
      return "C";
    else if (isCXX())
      return "C++";
    else if (isObjC())
      return "Obj-C";
    else
      return "unknown";
  }

  bool isCXX11orNewer() const {
    if (!langStdGiven())
      return false;

    constexpr const char *STD[] = { "c++0x", "gnu++0x", "c++11", "gnu++11",
                                    "c++1y", "gnu++1y", "c++14", "gnu++14",
                                    "c++1z", "gnu++1z" };

    for (auto std : STD) {
      if (!strcmp(langstd, std))
        return true;
    }

    return false;
  }

  const std::string &getTriple() const { return triple; }

  const std::string getFullCompilerName() const {
    std::string compiler;

    if (isGCC()) {
      compiler = execpath;
      compiler += "/";
      compiler += getTriple();
      compiler += "-";
    }

    if (isGCC())
      compiler += "base-";

    compiler += this->compiler;
    return compiler;
  }

  bool findClangIntrinsicHeaders(std::string &path) const {
    std::string clangbin;
    static std::stringstream dir;

    assert(isClang());

    getPathOfCommand(compiler.c_str(), clangbin);

    if (clangbin.empty())
      return false;

    static ClangVersion clangversion;
    static std::string pathtmp;

    dir.str(std::string());
    clangversion = ClangVersion();
    pathtmp.clear();

    auto check = []()->bool {

      listFiles(dir.str().c_str(), nullptr, [](const char *file) {

        if (file[0] != '.' && isDirectory(file, dir.str().c_str())) {
          ClangVersion cv = parseClangVersion(file);

          if (cv != ClangVersion()) {
            static std::stringstream tmp;
            tmp.str(std::string());

            tmp << dir.str() << "/" << file << "/include";

            if (dirExists(tmp.str())) {
              if (cv > clangversion) {
                clangversion = cv;
                pathtmp = tmp.str();
              }
            }
          }

          return true;
        }

        return true;
      });

      return clangversion != ClangVersion();
    };

    dir << clangbin << "/../lib/clang";

    if (!check()) {
      dir.str(std::string());

#ifdef __APPLE__
      constexpr const char *OSXIntrinDirs[] = {
        "/Library/Developer/CommandLineTools/usr/lib/clang",
        "/Applications/Contents/Developer/Toolchains/"
        "XcodeDefault.xctoolchain/usr/lib/clang"
      };

      for (auto intrindir : OSXIntrinDirs) {
        dir << intrindir;
        if (check()) {
          break;
        }
        dir.str(std::string());
      }
#endif

      if (!dir.rdbuf()->in_avail()) {
        dir << clangbin << "/../include/clang";

        if (!check())
          return false;
      }
    }

    path.swap(pathtmp);
    return clangversion != ClangVersion();
  }

  bool Setup() {
    if (!isKnownCompiler()) {
      std::cerr << "warning: unknown compiler '" << compiler << "'"
                << std::endl;
    }

    std::string SDKPath;
    if (!getSDKPath(SDKPath)) {
      std::cerr << "cannot find Mac OS X SDK (expected in: " << SDKPath << ")"
                << std::endl;
      return false;
    }

    if (targetarch.empty())
      targetarch.push_back(arch);

    if (!langStdGiven()) {
      if (isC())
        langstd = getDefaultCStandard();
      else if (isCXX())
        langstd = getDefaultCXXStandard();
    }

    triple = getArchName(arch);
    triple += "-";
    triple += vendor;
    triple += "-";
    triple += target;

    otriple = getArchName(Arch::x86_64);
    otriple += "-";
    otriple += vendor;
    otriple += "-";
    otriple += target;

    if (!OSNum.Num()) {
      if (stdlib != StdLib::libcxx) {
        OSNum = getDefaultMinTarget();
      } else {
        OSNum = OSVersion(10, 7); // Hack
      }
    } else {
      if (OSNum > getSDKOSNum()) {
        std::cerr << "targeted OS X Version must be <= " << getSDKOSNum().Str()
                  << " (SDK)" << std::endl;
        return false;
      } else if (OSNum < OSVersion(10, 4)) {
        std::cerr << "targeted OS X Version must be >= 10.4" << std::endl;
        return false;
      }
    }

    if (stdlib == StdLib::unset) {
      if (libCXXIsDefaultCXXLib()) {
        stdlib = StdLib::libcxx;
      } else {
        stdlib = StdLib::libstdcxx;
      }
    } else if (stdlib == StdLib::libcxx) {
      if (!hasLibCXX()) {
        std::cerr
            << "you need a newer SDK (10.7 at least) if you want to use libc++"
            << std::endl;
        return false;
      }

      if (OSNum.Num() && OSNum < OSVersion(10, 7)) {
        std::cerr
            << "you must target OS X 10.7 or newer if you want to use libc++"
            << std::endl;
        return false;
      }
    }

    std::string CXXHeaderPath = SDKPath;
    string_vector AdditionalCXXHeaderPaths;

    auto addCXXPath = [&](const std::string &path) {
      std::string tmp;
      tmp = CXXHeaderPath;
      tmp += "/";
      tmp += path;
      AdditionalCXXHeaderPaths.push_back(tmp);
    };

    auto addAbsoluteCXXPath = [&](const std::string &path) {
      AdditionalCXXHeaderPaths.push_back(path);
    };

    (void)addAbsoluteCXXPath;

    GCCVersion gccversion;

    switch (stdlib) {
    case StdLib::libcxx: {
      CXXHeaderPath += "/usr/include/c++/v1";
      if (!dirExists(CXXHeaderPath)) {
        std::cerr << "cannot find " << getStdLibString(stdlib) << " headers"
                  << std::endl;
        return false;
      }
      break;
    }
    case StdLib::libstdcxx: {
      if (isGCC() && /*isCXX11orNewer()*/ true)
        break;

      if (usegcclibs) {
#ifndef _WIN32
        // Use libs from './build_gcc.sh' installation

        CXXHeaderPath += "/../../";
        CXXHeaderPath += otriple;
        CXXHeaderPath += "/include/c++";

        static std::vector<GCCVersion> v;
        v.clear();

        listFiles(CXXHeaderPath.c_str(), nullptr, [](const char *path) {
          if (path[0] != '.')
            v.push_back(parseGCCVersion(path));
          return false;
        });

        if (v.empty()) {
          std::cerr << "'-oc-use-gcc-libs' requires gcc to be installed "
                       "(./build_gcc.sh)" << std::endl;
          return false;
        }

        std::sort(v.begin(), v.end());
        gccversion = v[v.size() - 1];

        CXXHeaderPath += "/";
        CXXHeaderPath += gccversion.Str();

        addCXXPath(otriple);
#else
        std::cerr << "'-oc-use-gcc-libs' not implemented" << std::endl;
        return false;
#endif
      } else {
        // Use SDK libs
        std::string tmp;

        if (getSDKOSNum() <= OSVersion(10, 5))
          CXXHeaderPath += "/usr/include/c++/4.0.0";
        else
          CXXHeaderPath += "/usr/include/c++/4.2.1";

        tmp = getArchName2(arch);
        tmp += "-apple-";
        tmp += target;
        addCXXPath(tmp);
      }

      if (!dirExists(CXXHeaderPath)) {
        std::cerr << "cannot find " << getStdLibString(stdlib) << " headers"
                  << std::endl;
        return false;
      }

      break;
    }
    case StdLib::unset:
      abort();
    }

    fargs.push_back(getFullCompilerName());

    if (isClang()) {
      std::string tmp;

      fargs.push_back("-target");
      fargs.push_back(getTriple());

      tmp = "-mlinker-version=";
      tmp += getLinkerVersion();

      fargs.push_back(tmp);
      tmp.clear();

#ifndef __APPLE__
      if (!findClangIntrinsicHeaders(tmp)) {
        std::cerr << "cannot find clang intrinsic headers, please report this "
                     "issue to the OSXCross project" << std::endl;
      } else {
        fargs.push_back("-isystem");
        fargs.push_back(tmp);
      }

      tmp.clear();
#endif

      fargs.push_back("-isysroot");
      fargs.push_back(SDKPath);

      if (isCXX()) {
        tmp = "-stdlib=";
        tmp += getStdLibString(stdlib);
        fargs.push_back(tmp);

        if (stdlib == StdLib::libcxx ||
            (stdlib == StdLib::libstdcxx && usegcclibs)) {
          fargs.push_back("-nostdinc++");
          fargs.push_back("-Qunused-arguments");
        }

        if (stdlib == StdLib::libstdcxx && usegcclibs) {
          // Use libs from './build_gcc' installation

          if (targetarch.size() > 1) {
            std::cerr
                << "'-oc-use-gcc-libs' does not support multiple arch flags"
                << std::endl;
            return false;
          }

          fargs.push_back("-nodefaultlibs");

          std::stringstream GCCLibSTDCXXPath;
          std::stringstream GCCLibPath;
          std::stringstream tmp;

          GCCLibSTDCXXPath << SDKPath << "/../../" << otriple << "/lib";
          GCCLibPath << SDKPath << "/../../lib/gcc/" << otriple << "/"
                     << gccversion.Str();

          if (targetarch[0] == Arch::i386) {
            GCCLibSTDCXXPath << "/" << getArchName(Arch::i386);
            GCCLibPath << "/" << getArchName(Arch::i386);
          }

          fargs.push_back("-Qunused-arguments");

          tmp << GCCLibSTDCXXPath.str() << "/libstdc++.a";
          fargs.push_back(tmp.str());

          tmp.str(std::string());
          tmp << GCCLibSTDCXXPath.str() << "/libsupc++.a";
          fargs.push_back(tmp.str());

          tmp.str(std::string());
          tmp << GCCLibPath.str() << "/libgcc.a";
          fargs.push_back(tmp.str());

          tmp.str(std::string());
          tmp << GCCLibPath.str() << "/libgcc_eh.a";
          fargs.push_back(tmp.str());

          fargs.push_back("-lc");
        }
      }
    } else if (isGCC()) {

      if (isLibCXX()) {
        if (!langStdGiven())
          langstd = "c++0x";
        else if (!isCXX11orNewer()) {
          std::cerr << "warning: libc++ requires -std=c++11 (or later) with gcc"
                    << std::endl;
        }
      }

      /* TODO: libgcc */

      if (isCXX() && (/*!isCXX11orNewer() ||*/ isLibCXX())) {
        fargs.push_back("-nostdinc++");
        fargs.push_back("-nodefaultlibs");

        if (haveSourceFile() && !isGCH()) {
          std::string tmp;

          tmp = "-L";
          tmp += SDKPath;
          tmp += "/usr/lib";

          fargs.push_back(tmp);
          fargs.push_back("-lc");

          if (isLibCXX()) {
            fargs.push_back("-lc++");
            fargs.push_back("-lc++abi");
          } else if (isLibSTDCXX()) {
            // Hack: Use SDKs libstdc++ as long
            // >= -std=c++11 is not given.

            fargs.push_back("-lstdc++");
          }

          fargs.push_back(OSNum <= OSVersion(10, 4) ? "-lgcc_s.10.4"
                                                    : "-lgcc_s.10.5");
        }
      } else if (!isLibCXX() /*&& isCXX11orNewer()*/ && !isGCH()) {
        fargs.push_back("-static-libgcc");
        fargs.push_back("-static-libstdc++");
      }
    }

    auto addCXXHeaderPath = [&](const std::string &path) {
      fargs.push_back(isClang() ? "-cxx-isystem" : "-isystem");
      fargs.push_back(path);
    };

    addCXXHeaderPath(CXXHeaderPath);

    for (auto &path : AdditionalCXXHeaderPaths)
      addCXXHeaderPath(path);

    if (langGiven()) {
      fargs.push_back("-x");
      fargs.push_back(lang);
    }

    if (langStdGiven()) {
      std::string tmp;
      tmp = "-std=";
      tmp += langstd;
      fargs.push_back(tmp);
    }

    if (OSNum.Num()) {
      std::string tmp;
      tmp = "-mmacosx-version-min=";
      tmp += OSNum.Str();
      fargs.push_back(tmp);
    }

    for (auto arch : targetarch) {
      switch (arch) {
      case Arch::i386:
      case Arch::x86_64:
        if (isGCC()) {
          if (targetarch.size() > 1) {
            std::cerr << "gcc does not support multiple arch flags"
                      << std::endl;
            return false;
          }
          fargs.push_back(arch == Arch::i386 ? "-m32" : "-m64");
        } else {
          fargs.push_back("-arch");
          fargs.push_back(getArchName(arch));
        }
        break;
      default:
        std::cerr << "unknown architecture" << std::endl;
        return false;
      }
    }

    if (haveOutputName()) {
      fargs.push_back("-o");
      fargs.push_back(outputname);
    }

    return true;
  }

  const char *vendor;
  Arch arch;
  std::vector<Arch> targetarch;
  std::string target;
  OSVersion OSNum;
  StdLib stdlib;
  bool usegcclibs;
  std::string compiler;
  std::string triple;
  std::string otriple;
  const char *lang;
  const char *langstd;
  string_vector fargs;
  string_vector args;
  const char *sourcefile;
  const char *outputname;
  char execpath[PATH_MAX + 1];
};

//
// Program 'sw_vers'
//

__attribute__((noreturn)) void prog_sw_vers(int argc, char **argv) {

  auto genFakeBuildVer = [](std::string & build)->std::string & {
    std::stringstream tmp;

#if __has_builtin(__builtin_readcyclecounter)
    srand(static_cast<unsigned int>(__builtin_readcyclecounter()));
#else
    srand(static_cast<unsigned int>(getNanoSeconds()));
#endif

    for (int i = 0; i < 5; ++i)
      tmp << std::hex << (rand() % 16 + 1);

    build = tmp.str();
    build.resize(5);

    return build;
  };

  auto getProductVer = []()->OSVersion {
    char *p = getenv("OSXCROSS_SW_VERS_OSX_VERSION");

    if (!p)
      p = getenv("MACOSX_DEPLOYMENT_TARGET");

    if (p)
      return parseOSVersion(p);

    return getDefaultMinTarget();
  };

  if (argc == 2) {
    std::stringstream str;

    if (!strcmp(argv[1], "-productName")) {
      str << "Mac OS X";
    } else if (!strcmp(argv[1], "-productVersion")) {
      str << getProductVer().shortStr();
    } else if (!strcmp(argv[1], "-buildVersion")) {
      std::string build;
      str << genFakeBuildVer(build);
    } else {
      exit(EXIT_FAILURE);
    }

    std::cout << str.str() << std::endl;
  } else if (argc == 1) {
    std::string build;

    std::cout << "ProductName:    Mac OS X" << std::endl;
    std::cout << "ProductVersion: " << getProductVer().shortStr() << std::endl;
    std::cout << "BuildVersion:   " << genFakeBuildVer(build) << std::endl;
  }

  exit(EXIT_SUCCESS);
}

//
// Program 'osxcross'
//

__attribute__((noreturn)) void prog_osxcross(int argc, char **argv) {
  (void)argc;
  (void)argv;

  std::cout << "version: " << getOSXCrossVersion() << std::endl;
  exit(EXIT_SUCCESS);
}

//
// Program 'osxcross-env'
//

__attribute__((noreturn)) void prog_osxcross_conf(const Target &target) {
  std::string sdkpath;
  const char *ltopath = getLibLTOPath();

  if (!target.getSDKPath(sdkpath)) {
    std::cerr << "cannot find Mac OS X SDK!" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (!ltopath)
    ltopath = "";

  std::cout << "export OSXCROSS_VERSION=" << getOSXCrossVersion() << std::endl;
  std::cout << "export OSXCROSS_OSX_VERSION_MIN="
            << getDefaultMinTarget().shortStr() << std::endl;
  std::cout << "export OSXCROSS_TARGET=" << getDefaultTarget() << std::endl;
  std::cout << "export OSXCROSS_SDK_VERSION=" << target.getSDKOSNum().shortStr()
            << std::endl;
  std::cout << "export OSXCROSS_SDK=" << sdkpath << std::endl;
  std::cout << "export OSXCROSS_TARBALL_DIR=" << target.execpath
            << "/../../tarballs" << std::endl;
  std::cout << "export OSXCROSS_PATCH_DIR=" << target.execpath
            << "/../../patches" << std::endl;
  std::cout << "export OSXCROSS_TARGET_DIR=" << target.execpath << "/.."
            << std::endl;
  std::cout << "export OSXCROSS_BUILD_DIR=" << target.execpath << "/../../build"
            << std::endl;
  std::cout << "export OSXCROSS_CCTOOLS_PATH=" << target.execpath << std::endl;
  std::cout << "export OSXCROSS_LIBLTO_PATH=" << ltopath << std::endl;
  std::cout << "export OSXCROSS_LINKER_VERSION=" << getLinkerVersion()
            << std::endl;

  exit(EXIT_SUCCESS);
}

//
// Program 'osxcross-env'
//

__attribute__((noreturn)) void prog_osxcross_env(int argc, char **argv) {
  char epath[PATH_MAX + 1];
  char *oldpath = getenv("PATH");
  char *oldlibpath = getenv("LD_LIBRARY_PATH");
  constexpr const char *ltopath = getLibLTOPath();

  assert(oldpath);

  if (!getExecutablePath(epath, sizeof(epath)))
    exit(EXIT_FAILURE);

  // TODO: escape?

  auto check = [](const char * p, const char * desc)->const char * {
    if (!p)
      return nullptr;

    const char *pp = p;

    for (; *p; ++p) {
      auto badChar = [&](const char *p) {
        std::cerr << desc << " should not contain '" << *p << "'" << std::endl;

        const char *start =
            p - std::min(static_cast<size_t>(p - pp), static_cast<size_t>(30));

        size_t len = std::min(strlen(start), static_cast<size_t>(60));
        std::cerr << std::string(start, len) << std::endl;

        while (start++ != p)
          std::cerr << " ";

        std::cerr << "^" << std::endl;

        exit(EXIT_FAILURE);
      };
      switch (*p) {
      case '"':
      case '\'':
      case '$':
      case ' ':
      case ';':
        badChar(p);
      }
    }
    return pp;
  };

  if (argc <= 1) {
    const std::string &pname = getParentProcessName();

    if (pname == "csh" || pname == "tcsh") {
      std::cerr << std::endl << "you are invoking this program from a C shell, "
                << std::endl << "please use " << std::endl << std::endl
                << "setenv PATH `" << epath << "/osxcross-env -v=PATH`"
                << std::endl << "setenv LD_LIBRARY_PATH `" << epath
                << "/osxcross-env -v=LD_LIBRARY_PATH`" << std::endl << std::endl
                << "instead." << std::endl << std::endl;
    }
  }

  auto hasPath = [](const char * ov, const char * v, const char * vs)->bool {
    // ov = old value
    // v = value
    // vs = value suffix

    if (!ov || !v)
      return false;

    bool hasPathSeparator = false;

    for (auto p = ov; *p; ++p) {
      if (*p == ':') {
        hasPathSeparator = true;
        break;
      }
    }

    static std::string tmp;

    auto check = [&](int t)->bool {
      tmp.clear();

      if (t == 0)
        tmp = ':';

      tmp += v;

      if (vs)
        tmp += vs;

      if (t == 1)
        tmp += ':';

      return strstr(ov, tmp.c_str()) != nullptr;
    };

    return ((hasPathSeparator && (check(0) || check(1))) || check(-1));
  };

  check(oldpath, "PATH");
  check(oldlibpath, "LD_LIBRARY_PATH");
  check(ltopath, "LIB LTO PATH");

  std::stringstream path;
  std::stringstream librarypath;
  std::map<std::string, std::string> vars;

  path << oldpath;

  if (!hasPath(oldpath, epath, nullptr))
    path << ":" << epath;

  if (oldlibpath)
    librarypath << oldlibpath;

  if (!hasPath(oldlibpath, epath, "/../lib"))
    librarypath << ":" << epath << "/../lib";

  if (ltopath && !hasPath(oldlibpath, ltopath, nullptr))
    librarypath << ":" << ltopath;

  vars["PATH"] = path.str();
  vars["LD_LIBRARY_PATH"] = librarypath.str();

  auto printVariable = [&](const std::string &var) {
    auto it = vars.find(var);
    if (it == vars.end()) {
      std::cerr << "unknown variable '" << var << "'" << std::endl;
      exit(EXIT_FAILURE);
    }
    std::cout << it->second << std::endl;
  };

  if (argc <= 1) {
    std::cout << std::endl;
    for (auto &v : vars) {
      std::cout << "export " << v.first << "=";
      printVariable(v.first);
      std::cout << std::endl;
    }
  } else {
    if (strncmp(argv[1], "-v=", 3))
      exit(EXIT_FAILURE);

    const char *var = argv[1] + 3;
    printVariable(var);
  }

  exit(EXIT_SUCCESS);
}

//
// Program 'dsymutil'
//

__attribute__((noreturn)) void prog_dsymutil(int argc, char **argv) {
  (void)argc;
  (void)argv;

  exit(EXIT_SUCCESS);
}

//
// detectTarget():
//  - detect target and setup invocation command
//

bool detectTarget(int argc, char **argv, Target &target) {
  const char *cmd = argv[0];
  const char *p = strrchr(cmd, '/');
  size_t i = 0;

  if (p)
    cmd = &p[1];

  target.args.reserve(static_cast<size_t>(argc));

  auto warnExtension = [](const char *extension) {
    std::cerr << "warning: '" << extension << "' is an OSXCross extension"
              << std::endl;
  };

  auto parseArgs = [&]()->bool {

    auto getVal = [&](char * arg, const char * flag, int & i)->const char * {
      const char *val = arg + strlen(flag);

      if (!*val) {
        val = argv[++i];

        if (i >= argc) {
          std::cerr << "missing argument for '" << val << "'" << std::endl;
          return nullptr;
        }
      }

      return val;
    };

    for (int i = 1; i < argc; ++i) {
      char *arg = argv[i];

      if (!strncmp(arg, "-mmacosx-version-min=", 21)) {
        const char *val = arg + 21;
        target.OSNum = parseOSVersion(val);

        if (target.OSNum != val) {
          std::cerr << "warning: '-mmacosx-version-min=' ("
                    << target.OSNum.Str() << " != " << val << ")" << std::endl;
        }
      } else if (!strncmp(arg, "-stdlib=", 8)) {
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
      } else if (!strcmp(arg, "-oc-use-gcc-libs")) {
        if (target.isGCC()) {
          std::cerr << "warning: '" << arg << "' has no effect" << std::endl;
          continue;
        }
        target.usegcclibs = true;
      } else if (!strncmp(arg, "-o", 2)) {
        target.outputname = getVal(arg, "-o", i);
      } else if (!strncmp(arg, "-x", 2)) {
        target.lang = getVal(arg, "-x", i);
      } else if (!strcmp(arg, "-m32")) {
        target.addArch(Arch::i386);
      } else if (!strcmp(arg, "-m64")) {
        target.addArch(Arch::x86_64);
      } else if (!strncmp(arg, "-arch", 5)) {
        const char *val = getVal(arg, "-arch", i);

        if (!val)
          return false;

        Arch arch = parseArch(val);

        if (arch == Arch::unknown) {
          std::cerr << "warning '-arch': unknown architecture '" << val << "'"
                    << std::endl;
        }

        const char *name = getArchName(arch);

        if (strcmp(val, name)) {
          std::cerr << "warning '-arch': " << val << " != " << name
                    << std::endl;
        }

        target.addArch(arch);
      } else {
        if (arg[0] != '-') {
          // Detect source file

          const char *prevarg = "";

          if (i > 1) {
            prevarg = argv[i - 1];

            if (prevarg[0] == '-' && strlen(prevarg) > 2)
              prevarg = "";
          }

          if (prevarg[0] != '-' || !strcmp(prevarg, "-c")) {
            const char *ext = strrchr(arg, '.');

            if (!ext || (strcmp(ext, ".o") && strcmp(ext, ".a")))
              target.sourcefile = arg;
          }
        }

        target.args.push_back(arg);
      }
    }

    return true;
  };

  auto checkCXXLib = [&]() {
    if (target.compiler.rfind("-libc++") == (target.compiler.size() - 7)) {
      if (target.stdlib != StdLib::unset && target.stdlib != StdLib::libcxx) {
        std::cerr << "warning: '-stdlib=" << getStdLibString(target.stdlib)
                  << "' will be ignored" << std::endl;
      }

      target.compiler.resize(target.compiler.size() - 7);
      target.stdlib = StdLib::libcxx;
    }
  };

  if (!strcmp(cmd, "sw_vers"))
    prog_sw_vers(argc, argv);
  else if (!strcmp(cmd, "osxcross"))
    prog_osxcross(argc, argv);
  else if (!strcmp(cmd, "osxcross-env"))
    prog_osxcross_env(argc, argv);
  else if (!strcmp(cmd, "osxcross-conf"))
    prog_osxcross_conf(target);
  else if (!strcmp(cmd, "dsymutil"))
    prog_dsymutil(argc, argv);

  for (auto arch : ArchNames) {
    const size_t len = strlen(arch);
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
      else if (target.compiler == "wrapper")
        exit(EXIT_SUCCESS);
      else if (target.compiler == "sw_vers")
        prog_sw_vers(argc, argv);
      else if (target.compiler == "osxcross")
        prog_osxcross(argc, argv);
      else if (target.compiler == "osxcross-env")
        prog_osxcross_env(argc, argv);
      else if (target.compiler == "osxcross-conf")
        prog_osxcross_conf(target);
      else if (target.compiler == "dsymutil")
        prog_dsymutil(argc, argv);

      if (target.target != getDefaultTarget()) {
        std::cerr << "warning: target mismatch (" << target.target
                  << " != " << getDefaultTarget() << ")" << std::endl;
      }

      if (!parseArgs())
        return false;

      checkCXXLib();
      return target.Setup();
    }
  }

  if (!strncmp(cmd, "o32", 3))
    target.arch = Arch::i386;
  else if (!strncmp(cmd, "o64", 3))
    target.arch = Arch::x86_64;
  else
    return false;

  if (cmd[3])
    target.compiler = &cmd[4];

  if (!parseArgs())
    return false;

  checkCXXLib();
  return target.Setup();
}

} // unnamed namespace

//
// Main routine
//

int main(int argc, char **argv) {
  char bbuf[sizeof(benchmark)];
  auto b = new (bbuf) benchmark;
  Target target;
  bool debug = false;

  if (!detectTarget(argc, argv, target)) {
    std::cerr << "cannot detect target" << std::endl;
    return 1;
  }

  if (char *p = getenv("OCDEBUG")) {
    debug = (p[0] == '1');
  }

  if (debug) {
    b->halt();
    std::cerr << "detected target triple: " << target.getTriple() << std::endl;
    std::cerr << "detected compiler: " << target.compiler << std::endl;

    std::cerr << "detected stdlib: " << getStdLibString(target.stdlib)
              << std::endl;

    // std::cerr << "detected source file: "
    //           << (target.sourcefile ? target.sourcefile : "") << std::endl;

    std::cerr << "detected language: " << target.getLangName() << std::endl;
    b->resume();
  }

  auto cargs = new char *[target.fargs.size() + target.args.size() + 1];
  size_t i = 0;

  for (auto &arg : target.fargs) {
    cargs[i++] = const_cast<char *>(arg.c_str());
  }

  for (auto &arg : target.args) {
    cargs[i++] = const_cast<char *>(arg.c_str());
  }

  cargs[i] = nullptr;

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

    std::cerr << "command (in): " << in << std::endl;
    std::cerr << "command (out): " << out << std::endl;
  };

  concatEnvVariable("COMPILER_PATH", target.execpath);

  if (debug) {
    time_type diff = b->getDiff();
    printCommand();
    std::cerr << "time spent in wrapper: " << diff / 1000000.0 << " ms"
              << std::endl;
  }

  if (execvp(cargs[0], cargs)) {
    std::cerr << "invoking compiler failed" << std::endl;

    if (!debug)
      printCommand();

    return 1;
  }

  __builtin_unreachable();
}
