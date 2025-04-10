/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2025 by Thomas Poechtrager                      *
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


#include "proginc.h"

using namespace tools;

namespace program {
namespace llvm {

int as(int argc, char **argv) {
    /*
     * Try to replicate the original as driver as much as possible:
     * https://github.com/tpoechtrager/cctools-port/blob/93ffa47ee2139aba177deb07de9b6626486037ae/cctools/as/driver.c#L300
     * 
     * clang is used for assembling as long as -Q is not given.
     */

    bool Qflag = false;
    bool some_input_files = false;
    bool oflag_specified = false;

    std::vector<char*> given_args;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "-Q") == 0) {
            Qflag = true;
            continue;
        }
    
        /*
         * If we have not seen some some_input_files or a "-" or "--" to
         * indicate we are assembling stdin add a "-" so clang will
         * assemble stdin as as(1) would.
         */
        if (strcmp(arg, "--") == 0 || strcmp(arg, "-") == 0 || arg[0] != '-') {
            some_input_files = true;
        }
    
        // Track if -o is specified
        if (strcmp(arg, "-o") == 0 && i + 1 < argc) {
            oflag_specified = true;
        }

        given_args.push_back(argv[i]);
    }
    

    if (Qflag) {
        std::vector<char*> llvm_args;
        llvm_args.push_back(const_cast<char*>("llvm-as"));
        llvm_args.insert(llvm_args.end(), given_args.begin(), given_args.end());
        llvm_args.push_back(nullptr);

        execvp(llvm_args[0], llvm_args.data());
        return 1;
    }

    std::vector<char*> args;

    args.push_back(const_cast<char*>("xcrun"));
    args.push_back(const_cast<char*>("clang"));

    /*
     * Add "-x assembler" in case the input does not end in .s this must
     * come before "-" or the clang driver will issue an error:
     * "error: -E or -x required when input is from standard input"
     */
    args.push_back(const_cast<char*>("-x"));
    args.push_back(const_cast<char*>("assembler"));

    /*
     * If we have not seen some some_input_files or a "-" or "--" to
     * indicate we are assembling stdin add a "-" so clang will
     * assemble stdin as as(1) would.
     */
    if (!some_input_files) {
        args.push_back(const_cast<char*>("-"));
    }

    for (char* arg : given_args) {
        /*
         * Translate as(1) use of "--" for stdin to clang's use of "-".
         */
        if (strcmp(arg, "--") == 0) {
            args.push_back(const_cast<char*>("-"));
        }
        /*
         * Do not pass command line argument that are Unknown to
         * to clang.
         */
        else if (strcmp(arg, "-V") != 0) {
            args.push_back(arg);
        }
    }

    /*
     * clang requires a "-o a.out" if not -o is specified.
     */
    if (!oflag_specified) {
        args.push_back(const_cast<char*>("-o"));
        args.push_back(const_cast<char*>("a.out"));
    }

    /* Add -integrated-as or clang will run as(1). */
    args.push_back(const_cast<char*>("-integrated-as"));

    /* Add -c or clang will run ld(1). */
    args.push_back(const_cast<char*>("-c"));

    /* Silence clang warnings for unused -I etc. */
    args.push_back(const_cast<char*>("-Wno-unused-command-line-argument"));

    args.push_back(nullptr);

    execvp(args[0], args.data());
    return 1;
}

} // namespace llvm
} // namespace program
