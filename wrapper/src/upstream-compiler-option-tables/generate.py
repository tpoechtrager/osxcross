#!/usr/bin/env python3
#
# Generates options.cpp: the options of Clang and GCC whose value is in the
# following argument(s). The wrapper only needs them to skip their values
# while parsing; see the comment in options.cpp.
#
# Usage: ./generate.py <llvm-project source dir> <gcc source dir>
#
# Needs llvm-tblgen (from PATH, or set LLVM_TBLGEN).
#

import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUTPUT = os.path.join(HERE, "options.cpp")
MAIN_CPP = os.path.join(HERE, "..", "main.cpp")

LICENSE = """\
/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2026 by Thomas Poechtrager                      *
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
"""

# An option is (mode, number of arguments). Modes as in OptParser::ValueMode.
SEPARATE = "SEPARATE"
JOINED_OR_SEPARATE = "JOINED_OR_SEPARATE"
JOINED_AND_SEPARATE = "JOINED_AND_SEPARATE"
MULTI_ARG = "MULTI_ARG"
REMAINING_ARGS = "REMAINING_ARGS"


def clang_options(llvm):
    """Returns ({name: (mode, n)}, {names of options without a value})."""
    tblgen = os.environ.get("LLVM_TBLGEN", "llvm-tblgen")
    td = os.path.join(llvm, "clang/include/clang/Options/Options.td")
    if not os.path.exists(td):  # Clang < 22
        td = os.path.join(llvm, "clang/include/clang/Driver/Options.td")
    dump = subprocess.run(
        [tblgen, "-dump-json", "-I", os.path.join(llvm, "llvm/include"),
         "-I", os.path.dirname(td), "-I", os.path.join(llvm, "clang/include"),
         td], check=True, capture_output=True, text=True).stdout
    records = json.loads(dump)

    kinds = {
        "KIND_SEPARATE": SEPARATE,
        "KIND_JOINED_OR_SEPARATE": JOINED_OR_SEPARATE,
        "KIND_JOINED_AND_SEPARATE": JOINED_AND_SEPARATE,
        "KIND_MULTIARG": MULTI_ARG,
        "KIND_REMAINING_ARGS": REMAINING_ARGS,
    }
    options, flags = {}, set()

    for rec_name in records["!instanceof"]["Option"]:
        rec = records[rec_name]
        visibility = [v["def"] for v in rec.get("Visibility") or []]
        # The driver sees options with the default visibility or ClangOption
        if visibility and not {"DefaultVis", "ClangOption"} & set(visibility):
            continue
        for prefix in rec.get("Prefixes") or []:
            if not prefix.startswith("-"):  # clang-cl: /option
                continue
            name = prefix + rec["Name"]
            kind = rec["Kind"]["def"]
            if kind in kinds:
                n = (rec.get("NumArgs") or 1) if kind == "KIND_MULTIARG" else 1
                options[name] = (kinds[kind], n)
            elif kind == "KIND_FLAG":
                flags.add(name)

    return options, flags


def gcc_options(gcc):
    """Returns ({name: (mode, n)}, {names of options without a value})."""
    # config.gcc adds rpath.opt and darwin.opt for Darwin
    files = ["gcc/common.opt", "gcc/c-family/c.opt", "gcc/config/rpath.opt",
             "gcc/config/darwin.opt", "gcc/config/i386/i386.opt",
             "gcc/config/aarch64/aarch64.opt"]
    options, flags = {}, set()

    for path in files:
        text = open(os.path.join(gcc, path)).read()
        for block in re.split(r"\n\s*\n", text):
            lines = [l for l in block.split("\n")
                     if l and not l.startswith(";")]
            if len(lines) < 2 or re.match(
                    r"(Variable|Enum|EnumValue|TargetSave|TargetVariable|"
                    r"HeaderInclude|SourceInclude|Language|Init|###)\b",
                    lines[0]):
                continue
            name = "-" + lines[0]
            props = re.findall(r"\w+(?:\([^)]*\))?", lines[1])
            words = {re.sub(r"\(.*", "", p) for p in props}
            # NoDriverArg: the driver takes the option without its argument
            if "Separate" in words and "NoDriverArg" not in words:
                args = [int(m) for p in props
                        for m in re.findall(r"^Args\((\d+)\)$", p)]
                n = args[0] if args else 1
                if n > 1:
                    options[name] = (MULTI_ARG, n)
                elif "Joined" in words or "JoinedOrMissing" in words:
                    options[name] = (JOINED_OR_SEPARATE, 1)
                else:
                    options[name] = (SEPARATE, 1)
            elif "Joined" not in words and "JoinedOrMissing" not in words:
                flags.add(name)

    return options, flags


def merge(a, b):
    """The mode for an option both compilers have; None if they differ in
    the number of arguments."""
    if a[1] != b[1]:
        return None
    if a[0] == b[0]:
        return a
    # "-x" consumes the next argument in both; the joined form only exists
    # in one, it consumes nothing there either.
    modes = {a[0], b[0]}
    if modes == {SEPARATE, JOINED_OR_SEPARATE}:
        return (JOINED_OR_SEPARATE, 1)
    if modes == {MULTI_ARG}:
        return a
    return None


VALUE_MODES = {
    SEPARATE: "ValueMode::SEPARATE",
    MULTI_ARG: "ValueMode::SEPARATE",
    JOINED_OR_SEPARATE: "ValueMode::JOINED | ValueMode::SEPARATE",
    JOINED_AND_SEPARATE: "ValueMode::JOINED_AND_SEPARATE",
    REMAINING_ARGS: "ValueMode::REMAINING_ARGS",
}


def entries(options):
    """Table entries, sorted by name. "-name" and "--name" with the same
    arguments become one entry with ParseMode::DOUBLE_DASH."""
    lines = []
    for name in sorted(options):
        if name.startswith("--") and options.get(name[1:]) == options[name]:
            continue  # with "-name"
        mode, n = options[name]
        parse_mode = "ParseMode::FORWARD"
        if options.get("-" + name) == options[name]:
            parse_mode += " | ParseMode::DOUBLE_DASH"
        args = ['"%s"' % name, "nullptr", VALUE_MODES[mode], parse_mode]
        if n > 1:
            args.append(str(n))
        lines.append(wrap(args))
    return lines


def table(name, comment, lines):
    return ("// %s\nconst std::vector<Parser::Option> %s = {\n%s\n};\n"
            % (comment, name, "\n".join(lines)))


def wrap(args):
    """One table entry, broken after a comma where it exceeds 80 columns."""
    lines, line = [], "  {" + args[0]
    for arg in args[1:]:
        if len(line) + len(", " + arg) + 2 > 80:
            lines.append(line + ",")
            line = "   " + arg
        else:
            line += ", " + arg
    lines.append(line + "},")
    return "\n".join(lines)


def wrapper_options():
    """The spellings of the options in the wrapper's table in main.cpp."""
    text = open(MAIN_CPP).read()
    table = text[text.index("Parser options = {{"):
                 text.index("}, getDebugLevel() > 0};")]
    names = set()
    for entry in re.findall(r'\{"([^"]+)",[^{}]*\}', table):
        names.add(entry)
    for entry in re.findall(r'\{"(-[^-"][^"]*)",[^{}]*DOUBLE_DASH[^{}]*\}',
                            table):
        names.add("-" + entry)
    return names


def clang_version(llvm):
    path = os.path.join(llvm, "cmake/Modules/LLVMVersion.cmake")
    if not os.path.exists(path):  # LLVM < 18
        path = os.path.join(llvm, "llvm/CMakeLists.txt")
    return ".".join(re.findall(
        r"set\(LLVM_VERSION_(?:MAJOR|MINOR|PATCH) (\d+)\)",
        open(path).read())[:3])


# The beginning of options.cpp after the license; %s are the versions of
# Clang and GCC.
HEADER = """\
// Generated by generate.py from the options of Clang %s
// (Options.td) and GCC %s (*.opt). Do not edit.
//
// Why these tables exist
//
// The wrapper looks for a few options of its own on the command line
// (-arch, -isysroot, -stdlib=, -c, ...; the table in main.cpp) and
// passes all other arguments on to the compiler unchanged. To find
// its options reliably, it must know which arguments are the value
// of another option: in "-o -c", "-c" is the name of the output
// file, not the option -c, and in "-MF -nostdinc" the dependency
// file is not -nostdinc.
//
// These tables list the options of Clang and GCC whose value is in
// the following argument(s): the options of both compilers, of Clang
// only and of GCC only. The wrapper does nothing with them and does
// not look at their values: parse() in main.cpp adds the table of both
// compilers and the one of the compiler in use to its table only so
// that it skips the values and passes option and values on as they
// are.
//
// Rules
//
// - Only options that take the following argument(s) as their value
//   belong here. Flags (-O2) and options with an attached value
//   (-Wl,..., -std=c++17) cannot hide another option and are not
//   listed.
// - No option may be listed twice, neither in these tables nor here
//   and in the table in main.cpp. The wrapper's own options (and their other
//   spellings, e.g. --stdlib for -stdlib=) are only in main.cpp;
//   generate.py leaves them out. After adding an option to main.cpp,
//   run generate.py again.

#include "upstream-compiler-option-tables/options.h"

namespace commandopts {

"""


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: %s <llvm-project dir> <gcc dir>" % sys.argv[0])
    llvm, gcc = sys.argv[1:]

    clang, _ = clang_options(llvm)
    gnu, _ = gcc_options(gcc)

    both, clang_only, gcc_only = {}, {}, {}
    for name in sorted(set(clang) | set(gnu)):
        if name in clang and name in gnu:
            merged = merge(clang[name], gnu[name])
            if not merged:
                sys.exit("%s: Clang %s, GCC %s" % (name, clang[name],
                                                   gnu[name]))
            both[name] = merged
        elif name in clang:
            clang_only[name] = clang[name]
        else:
            gcc_only[name] = gnu[name]

    # The wrapper's own options are in main.cpp; each option may only be
    # in one of the tables.
    own = wrapper_options()
    for options in (both, clang_only, gcc_only):
        for name in own & set(options):
            del options[name]

    gcc_version = open(os.path.join(gcc, "gcc/BASE-VER")).read().strip()

    out = [LICENSE, "\n"]
    out.append(HEADER % (clang_version(llvm), gcc_version))

    tables = (("PassthroughCompilerOptions", "Clang and GCC", entries(both)),
              ("PassthroughClangOptions", "Clang only", entries(clang_only)),
              ("PassthroughGCCOptions", "GCC only", entries(gcc_only)))
    out.append("\n".join(table(*t) for t in tables))
    out.append("\n} // namespace commandopts\n")

    spellings = list(both) + list(clang_only) + list(gcc_only)
    duplicates = sorted({s for s in spellings if spellings.count(s) > 1} |
                        (set(spellings) & own))
    if duplicates:
        sys.exit("options listed twice: " + " ".join(duplicates))

    for line in "".join(out).split("\n"):
        assert len(line) <= 80, line

    with open(OUTPUT, "w") as f:
        f.write("".join(out))

    print("%s: %s" % (os.path.relpath(OUTPUT), ", ".join(
        "%s %d" % (name, len(lines)) for name, _, lines in tables)))


if __name__ == "__main__":
    main()
