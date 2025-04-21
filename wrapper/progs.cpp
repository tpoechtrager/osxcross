#include <vector>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <string.h>

#include "progs.h"
#include "tools.h"

namespace program {

namespace llvm {

using tools::dbg;

int execute(const char *toolName, int argc, char **argv) {
  std::vector<char *> args;
  args.reserve(argc + 1);
  args.emplace_back(const_cast<char *>(toolName));
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  args.emplace_back(nullptr);

  execvp(toolName, args.data());

  std::cerr << "Error: cannot execute '" << toolName << "'" << std::endl;
  return 1;
}

void printArgs(int argc, char **argv, std::vector<char*> &args) {
  std::string in, out;

  for (int i = 0; i < argc; ++i) {
    in += argv[i]; in += " ";
  }

  for (size_t i = 0; i < args.size(); ++i) {
    out += args[i]; out += " ";
  }


  dbg << "--> " << in << dbg.endl();
  dbg << "<-- " << out << dbg.endl();
}
    
} // namespace llvmWrapper
} // namespace program
