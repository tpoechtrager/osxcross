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

#ifndef OSXCROSS_WRAPPER_TOOLS_JSON_H
#define OSXCROSS_WRAPPER_TOOLS_JSON_H

#include <string>
#include <vector>

#include <stddef.h>

// A small JSON reader (RFC 8259), for files such as SDKSettings.json.
namespace tools {
namespace json {

// A JSON value. Objects and arrays own their children.
struct Value {
  enum Type { NONE, STRING, NUMBER, BOOLEAN, OBJECT, ARRAY };

  Value() : type(NONE), boolean(false) {}

  Type type;                     // NONE: null, or a value that does not exist
  std::string string;            // STRING (UTF-8); NUMBER as written ("1e3")
  bool boolean;                  // BOOLEAN
  std::vector<std::string> keys; // OBJECT: the keys, in file order
  std::vector<Value> values;     // OBJECT: the value of each key;
                                 // ARRAY: the elements

  bool isString() const { return type == STRING; }
  bool isObject() const { return type == OBJECT; }
  bool isArray() const { return type == ARRAY; }

  // The number of members (OBJECT) or elements (ARRAY).
  size_t size() const { return values.size(); }

  // The member with this key (the last one if the key repeats), or the
  // element at this index. A NONE value if there is none, so lookups can be
  // chained: root["SupportedTargets"]["macosx"]["Archs"][0].string
  // (The index is an int, so that [0] is not ambiguous with a null key.)
  const Value &operator[](const char *key) const;
  const Value &operator[](int index) const;
};

// Parses exactly one JSON value with at most 64 nested objects or arrays.
// Expects UTF-8 without a byte order mark. Unescaped string bytes are copied
// as supplied. Unicode escapes are decoded and unpaired surrogates rejected.
// Returns false and resets value to NONE on a parse error.
bool parse(const std::string &text, Value &value);

} // namespace json
} // namespace tools

#endif // OSXCROSS_WRAPPER_TOOLS_JSON_H
