#include <vector>
#include <iostream>
#include <unistd.h>
#include <string.h>

#include "progs.h"

namespace program {

namespace llvm {

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
    
} // namespace llvmWrapper
} // namespace program
