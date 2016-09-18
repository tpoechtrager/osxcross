#!/usr/bin/env bash

set -e
set -u

# Print number of enabled CPUs.  Use this as a simple, platform-independent
# replacement for nproc or ncpus.
#
# The shell script wraps a simple C++ tool which will be compiled on demand.


# This script's location.  The proper way to do this in bash is using
# ${BASH_SOURCE[0]}; ignore the possibility of softlinks.
script_dir=$(cd $(dirname ${BASH_SOURCE[0]}) && pwd)

prog="$script_dir/cpucount"
case "$(uname -s)" in
  *NT* | CYGWIN*)
    prog="${prog}.exe" ;;
esac

if [ ! -f $prog ]
then
    # Don't have cpucount.  Build it.

    if ! which c++ >/dev/null
    then
        # Can't compile cpucount.  Just give the safe answer.
        echo 1
        exit 0
    fi

    # Attempt to compile cpucount.cpp.
    if ! c++ $prog.cpp -o $prog &>/dev/null
    then
        # Okay, that didn't work...  Try it with gcc/clang's option to force
        # C++11.  Versions of gcc older than 6.x still default to C++98.
        c++ $prog.cpp -std=c++11 -o $prog >/dev/null
    fi
fi

# Now, at last: run cpucount.
$prog
