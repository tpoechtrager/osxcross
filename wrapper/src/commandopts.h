/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2014-2026 by Thomas Poechtrager                           *
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

#ifndef OSXCROSS_WRAPPER_COMMANDOPTS_H
#define OSXCROSS_WRAPPER_COMMANDOPTS_H

#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "tools/tools.h"

namespace target {
struct Target;
} // namespace target

// Parsing of the compiler command line (parse() in main.cpp).
namespace commandopts {

// How an option takes its value. Flags: JOINED | SEPARATE accepts "-Ifoo"
// and "-I foo".
enum class ValueMode : unsigned {
  NONE = 0,                      // No value, e.g. "-m64".
  JOINED = 1 << 0,               // Value attached, e.g. "-Ifoo".
  EQUALS = 1 << 1,               // Value after '=', e.g. "-stdlib=libc++".
  SEPARATE = 1 << 2,             // Value in the next argument (numArgs
                                 // values), e.g. "-arch arm64".
  JOINED_AND_SEPARATE = 1 << 3,  // Attached value and the next argument,
                                 // e.g. "-Xarch_arm64 -O2".
  REMAINING_ARGS = 1 << 4        // All following arguments ("--").
};

// How an option is spelled and what happens to it. Flags.
enum class ParseMode : unsigned {
  NONE = 0,             // The wrapper consumes the option.
  FORWARD = 1 << 0,     // Pass the option and its value on.
  DOUBLE_DASH = 1 << 1  // "--name" works too (for an option "-name").
};

constexpr ValueMode operator|(const ValueMode a, const ValueMode b) {
  return static_cast<ValueMode>(static_cast<unsigned>(a) |
                                static_cast<unsigned>(b));
}

constexpr ParseMode operator|(const ParseMode a, const ParseMode b) {
  return static_cast<ParseMode>(static_cast<unsigned>(a) |
                                static_cast<unsigned>(b));
}

template <typename Flags>
constexpr bool hasFlag(const Flags flags, const Flags flag) {
  return (static_cast<unsigned>(flags) & static_cast<unsigned>(flag)) != 0;
}

// Parser for compiler options such as -I, -arch, and -stdlib.
template <typename T> struct OptParser {
  struct Option {
    const char *name;
    size_t nameLen;
    T fun;
    ValueMode valueMode;
    ParseMode parseMode;
    int numArgs; // SEPARATE: number of values

    Option(const char *name, T fun, const ValueMode valueMode = ValueMode::NONE,
           const ParseMode parseMode = ParseMode::NONE, const int numArgs = 1)
        : name(name), nameLen(strlen(name)), fun(fun), valueMode(valueMode),
          parseMode(parseMode), numArgs(numArgs) {}

    // Returns the part of arg after the option name ("" if there is none),
    // or nullptr if arg is not this option.
    const char *match(const char *arg) const {
      if (hasFlag(parseMode, ParseMode::DOUBLE_DASH) && !strncmp(arg, "--", 2))
        ++arg;

      if (strncmp(arg, name, nameLen))
        return nullptr;

      const char *suffix = arg + nameLen;

      // The bare option always matches, so that the caller can report a
      // missing value instead of treating the option as unknown.
      if (!*suffix ||
          (*suffix == '=' && hasFlag(valueMode, ValueMode::EQUALS)) ||
          hasFlag(valueMode, ValueMode::JOINED) ||
          hasFlag(valueMode, ValueMode::JOINED_AND_SEPARATE))
        return suffix;

      return nullptr;
    }
  };

  std::vector<Option> options;
  const bool debug;

  OptParser(std::initializer_list<Option> options, const bool debug)
      : options(options), debug(debug) {}

  OptParser(const std::vector<Option> &options, const bool debug)
      : options(options), debug(debug) {}

  void add(const std::vector<Option> &more) {
    options.insert(options.end(), more.begin(), more.end());
  }

  // Returns the option with the longest name that accepts arg, as Clang
  // does: "-isystem-after" is not "-isystem" with the value "-after". Of
  // names with the same length, the first one in the table wins. suffix is
  // set to the part of arg after the name.
  const Option *parse(const char *arg, const char **suffix) const {
    const Option *match = nullptr;

    for (const Option &option : options) {
      const char *optionSuffix = option.match(arg);

      if (optionSuffix && (!match || optionSuffix > *suffix)) {
        match = &option;
        *suffix = optionSuffix;
      }
    }

    return match;
  }

  // Prints the resolved option, its optional value, and its forwarding policy
  // when debugging was enabled while constructing the parser.
  void printDebug(const Option &option, const char *arg,
                  const char *value) const {
    if (!debug)
      return;

    tools::dbg << "parsed option '" << option.name << "'";

    if (value)
      tools::dbg << " with value '" << value << "'";

    tools::dbg << " from argument '" << arg << "' ("
        << (hasFlag(option.parseMode, ParseMode::FORWARD) ? "forwarded"
                                                           : "consumed")
        << ")" << tools::dbg.endl();
  }
};

// An option of the wrapper as parse() found it on the command line.
struct ParsedOption {
  target::Target &target;
  const char *name; // as in the option table, such as "-x"
  const char *val;  // its value; nullptr if it has none
  char **args;      // the arguments after it
};

// Called for an option of the wrapper (see main.cpp).
typedef bool (*OptFun)(const ParsedOption &opt);

typedef OptParser<OptFun> Parser;

} // namespace commandopts

#endif // OSXCROSS_WRAPPER_COMMANDOPTS_H
