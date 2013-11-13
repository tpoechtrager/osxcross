#!/usr/bin/env bash

find . -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.h" -o -name "*.hpp" \) -print0 | \
xargs -0 sed -i "s/#include <unistd.h>/#undef __block\n#include <unistd.h>\n#define __block __attribute__((__blocks__(byref)))/g"
