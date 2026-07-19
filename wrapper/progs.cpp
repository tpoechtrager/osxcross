#include "programs/proginc.h"

namespace program {

using tools::dbg;
using tools::err;

int executeExternalTool(const char *toolName, int argc, char **argv) {
  std::vector<char *> args;
  args.reserve(argc + 1);
  args.push_back(const_cast<char *>(toolName));

  for (int i = 1; i < argc; ++i)
    args.push_back(argv[i]);

  args.push_back(nullptr);
  execvp(toolName, args.data());

  err << "Error: cannot execute '" << toolName << "'" << err.endl();
  return 1;
}

void printExternalToolArgs(int argc, char **argv, std::vector<char *> &args) {
  std::string in;
  std::string out;

  for (int i = 0; i < argc; ++i) {
    in += argv[i];
    in += " ";
  }

  for (std::vector<char *>::const_iterator it = args.begin();
       it != args.end(); ++it) {
    if (*it) {
      out += *it;
      out += " ";
    }
  }

  dbg << "--> " << in << dbg.endl();
  dbg << "<-- " << out << dbg.endl();
}

} // namespace program
