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

#include "compat.h"

#include <cstring>

#include "tools/tools.h"
#include "programs/argument-parser.h"

namespace program {

using tools::err;

int ArgumentParser::execute(int argc, char **argv, Target &target) const {
  const Option *action = nullptr;   // the last action given
  char **actionArgs = nullptr;
  const Option *fallback = nullptr; // the last fallback action given
  char **fallbackArgs = nullptr;
  char **givenOperand = nullptr;    // the operand of a fallback action

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    const bool isOption = arg[0] == '-';
    const Option *option = isOption ? findOption(arg) : &operand;

    // The operand of a fallback action such as -f in "xcrun -f -sdk macosx
    // clang".
    if (!isOption && fallback && fallback->value == Value::OPERAND &&
        !fallbackArgs) {
      fallbackArgs = givenOperand = &argv[i];
      continue;
    }

    if (!option || (!option->handler && option->kind != Kind::IGNORE)) {
      err << program << ": unknown argument: '" << arg << "'" << err.endl();
      return usageError;
    }

    // An argument such as "true" in "xcrun -f clang true" would be a second
    // operand or action.
    if (!isOption && option->kind == Kind::ACTION &&
        (action || givenOperand)) {
      err << program << ": unexpected argument: '" << arg << "'" << err.endl();
      return usageError;
    }

    // An operand is its own value; the values of an option follow it.
    char **args = isOption ? &argv[i + 1] : &argv[i];
    const int numArgs = isOption ? argc - i - 1 : argc - i;
    bool lastArg = false;

    switch (option->value) {
    case Value::NONE:
      break;
    case Value::ONE:
      if (!numArgs) {
        err << program << ": too few arguments for '" << arg << "'"
            << err.endl();
        return usageError;
      }

      if (isOption)
        ++i;
      break;
    case Value::OPTIONAL:
      if (!numArgs || (isOption && args[0][0] == '-'))
        args = nullptr;
      else if (isOption)
        ++i;
      break;
    case Value::REST:
      if (!numArgs) {
        err << program << ": too few arguments for '" << arg << "'"
            << err.endl();
        return usageError;
      }

      lastArg = true;
      break;
    case Value::OPERAND:
      args = nullptr; // taken from a later argument
      break;
    }

    switch (option->kind) {
    case Kind::SETTING:
      if (int status = option->handler(target, args))
        return status;
      break;
    case Kind::ACTION:
      action = option;
      actionArgs = args;
      break;
    case Kind::FALLBACK_ACTION:
      // An operand given before stays, as in "xcrun -f clang -f ld".
      if (option->value == Value::OPERAND && givenOperand)
        break;

      fallback = option;
      fallbackArgs = args;
      break;
    case Kind::IMMEDIATE_ACTION:
      return option->handler(target, args);
    case Kind::IGNORE:
      break;
    }

    if (lastArg)
      break;
  }

  // An action comes first, as --show-sdk-path in "xcrun --show-sdk-path -f
  // clang".
  if (!action) {
    action = fallback;
    actionArgs = fallbackArgs;
  }

  if (!action)
    return defaultAction ? defaultAction(target, nullptr) : 0;

  if (prepare) {
    if (int status = prepare(target, nullptr))
      return status;
  }

  return action->handler(target, actionArgs);
}

// The option for arg, such as "-sdk"; nullptr if there is none.
const ArgumentParser::Option *
ArgumentParser::findOption(const char *arg) const {
  const char *name = arg + 1;

  if (singleDash) {
    if (*name == '-')
      return nullptr;
  } else {
    while (*name == '-')
      ++name;
  }

  for (const Option &option : options) {
    if (!strcmp(name, option.name))
      return &option;
  }

  return nullptr;
}

} // namespace program
