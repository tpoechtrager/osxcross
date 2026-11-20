/***********************************************************************
 *  OSXCross Compiler Wrapper                                          *
 *  Copyright (C) 2026 by Thomas Poechtrager                           *
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

#ifndef OSXCROSS_WRAPPER_PROGRAMS_ARGUMENT_PARSER_H
#define OSXCROSS_WRAPPER_PROGRAMS_ARGUMENT_PARSER_H

#include <cassert>
#include <vector>

namespace target {
struct Target;
}

namespace program {

using target::Target;

// Reads the command line of a program and runs the handlers of its options.
struct ArgumentParser {
  // When the handler of an option runs.
  enum class Kind {
    SETTING,          // while the arguments are read, e.g. -sdk
    ACTION,           // after reading; only the last action given runs
    FALLBACK_ACTION,  // after reading, only if no ACTION is given; only the
                      // last one runs, e.g. xcrun -f
    IMMEDIATE_ACTION, // at once; its result ends execute()
    IGNORE            // never; the option is accepted and dropped, e.g. -v
  };

  // What follows the option name.
  enum class Value {
    NONE,     // nothing, e.g. -version
    ONE,      // one argument, e.g. -sdk <sdk>
    OPTIONAL, // one argument if the next one is not an option; otherwise
              // the handler gets nullptr
    REST,     // all remaining arguments, at least one; reading stops
    OPERAND   // for a fallback action, the next operand, also after other
              // options, e.g. xcrun -f; nullptr if none follows
  };

  // Gets the arguments that follow the option name (nullptr for
  // Value::OPTIONAL and OPERAND without one) and returns an exit status. A
  // setting that returns anything but 0 ends execute() with that status.
  typedef int (*Handler)(Target &target, char **args);

  struct Option {
    Option() : name(), handler(), kind(Kind::SETTING), value(Value::NONE) {}

    Option(const char *name, Handler handler, Kind kind = Kind::SETTING,
           Value value = Value::NONE)
        : name(name), handler(handler), kind(kind), value(value) {}

    // An option without a handler, e.g. {"v", Kind::IGNORE}.
    Option(const char *name, Kind kind, Value value = Value::NONE)
        : name(name), handler(), kind(kind), value(value) {
      assert(kind == Kind::IGNORE);
    }

    const char *name; // without the leading '-'
    Handler handler;
    Kind kind;
    Value value;
  };

  // Reads argv[1] to argv[argc - 1] and runs the handlers. Returns the exit
  // status of the program.
  int execute(int argc, char **argv, Target &target) const;

  const char *program;         // the program name in error messages
  int usageError;              // exit status for an unknown option or a
                               // missing argument
  bool singleDash;             // options take one '-' only; otherwise any
                               // number, as in --sdk
  std::vector<Option> options;
  Option operand;              // handles the operands, the arguments that
                               // do not start with '-'; its handler gets
                               // the operand itself, and with Value::REST
                               // also all following arguments. As an
                               // action, it must not follow another action
                               // or an operand
  Handler prepare;             // runs before the action; nullptr: nothing
  Handler defaultAction;       // runs if no action was given; nullptr:
                               // execute() returns 0

private:
  const Option *findOption(const char *arg) const;
};

} // namespace program

#endif // OSXCROSS_WRAPPER_PROGRAMS_ARGUMENT_PARSER_H
