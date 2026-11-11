#include "compat.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

#include "tools.h"
#include "progs.h"

namespace program {

using tools::dbg;
using tools::err;

// Replace the current process with path; args[0] becomes its argv[0].
int execProgram(const std::string &path,
                const std::vector<std::string> &args) {
  std::vector<char *> argv;
  argv.reserve(args.size() + 1);

  for (const std::string &arg : args)
    argv.push_back(const_cast<char *>(arg.c_str()));

  argv.push_back(nullptr);
  execvp(path.c_str(), argv.data());

  const int error = errno;
  err << "cannot execute '" << path << "': " << strerror(error) << err.endl();
  return 1;
}

// Like above, but pass on argv[1..] and use path as argv[0].
int execProgram(const std::string &path, int argc, char **argv) {
  std::vector<std::string> args(1, path);

  if (argc > 1)
    args.insert(args.end(), argv + 1, argv + argc);

  return execProgram(path, args);
}

// Debug output: the original command line and the one passed to
// execProgram().
void printExecProgramArgs(int argc, char **argv,
                          const std::vector<std::string> &args) {
  std::string in;
  std::string out;

  for (int i = 0; i < argc; ++i) {
    in += argv[i];
    in += " ";
  }

  for (const std::string &arg : args) {
    out += arg;
    out += " ";
  }

  dbg << "--> " << in << dbg.endl();
  dbg << "<-- " << out << dbg.endl();
}

namespace {

int dummy(int, char **, Target &) { return 0; }

constexpr prog programs[] = {
  // Built-in tools
  { "sw_vers",          sw_vers },
  { "xcrun",            xcrun },
  { "xcodebuild",       xcodebuild },

  // LLVM tools

  // These are LLVM tools where we must modify the passed arguments to it.
  { "ld",               llvm::ld },
  { "lipo",             llvm::lipo },
  { "as",               llvm::clang::as },

  // Used 'as-is', besides making them believe
  // they are invoked as "llvm-<tool>" instead of "<tool>".
  { "otool",            "llvm-otool" },

  { "nm",               "llvm-nm" },
  { "ar",               "llvm-ar" },
  { "libtool",          "llvm-libtool-darwin" },
  { "install_name_tool","llvm-install-name-tool" },
  { "ranlib",           "llvm-ranlib" },
  { "readtapi",         "llvm-readtapi" },
  { "objdump",          "llvm-objdump" },
  { "strip",            "llvm-strip" },
  { "strings",          "llvm-strings" },
  { "size",             "llvm-size" },
  { "symbolizer",       "llvm-symbolizer" },
  { "cov",              "llvm-cov" },
  { "profdata",         "llvm-profdata" },
  { "readobj",          "llvm-readobj" },
  { "readelf",          "llvm-readelf" },
  { "dwarfdump",        "llvm-dwarfdump" },
  { "cxxfilt",          "llvm-cxxfilt" },
  { "objcopy",          "llvm-objcopy" },
  { "config",           "llvm-config" },
  { "dis",              "llvm-dis" },
  { "link",             "llvm-link" },
  { "lto",              "llvm-lto" },
  { "lto2",             "llvm-lto2" },
  { "bcanalyzer",       "llvm-bcanalyzer" },
  { "bitcode_strip",    "llvm-bitcode-strip" },

  // OSXCross tools
  { "osxcross",         osxcross::version },
  { "osxcross-env",     osxcross::env },
  { "osxcross-conf",    osxcross::conf },

  // Tools where we must modify the passed arguments to it.
  { "osxcross-man",     osxcross::man },
  { "pkg-config",       osxcross::pkg_config },

  // Dummy tool. No-op.
  { "wrapper",          dummy }
};

} // unnamed namespace

const prog *getprog(const std::string &name) {
  for (const prog &p : programs) {
    if (name == p.name)
      return &p;
  }

  return nullptr;
}

} // namespace program
