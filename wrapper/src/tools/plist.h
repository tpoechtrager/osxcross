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

#ifndef OSXCROSS_WRAPPER_TOOLS_PLIST_H
#define OSXCROSS_WRAPPER_TOOLS_PLIST_H

#include <cstddef>
#include <string>
#include <vector>

// A reader for Apple's property lists, in XML and in the binary format
// (bplist00), for files such as SDKSettings.plist and SystemVersion.plist.
namespace tools {
namespace plist {

// A property list value. Dictionaries and arrays own their children.
struct Value {
  enum Type {
    NONE, STRING, INTEGER, REAL, BOOLEAN, DATE, DATA, ARRAY, DICT, UID
  };

  Value() : type(NONE), boolean(false) {}

  Type type;                     // NONE: a value that does not exist
  std::string string;            // STRING (UTF-8); INTEGER, REAL and UID as
                                 // text ("42", "10.5"); DATE in ISO 8601
                                 // ("2026-11-19T12:00:00Z"); DATA as the
                                 // raw bytes
  bool boolean;                  // BOOLEAN
  std::vector<std::string> keys; // DICT: the keys, in file order
  std::vector<Value> values;     // DICT: the value of each key;
                                 // ARRAY: the elements

  bool isString() const { return type == STRING; }
  bool isDict() const { return type == DICT; }
  bool isArray() const { return type == ARRAY; }

  // The number of members (DICT) or elements (ARRAY).
  size_t size() const { return values.size(); }

  // The member with this key (the last one if the key repeats), or the
  // element at this index. A NONE value if there is none, so lookups can be
  // chained: root["SupportedTargets"]["macosx"]["Archs"][0].string
  const Value &operator[](const char *key) const;
  const Value &operator[](int index) const;
};

// Reads XML property lists and binary property lists in bplist00 format.
// The format is detected from the first bytes. On a parse error, returns
// false and resets value to NONE. Values nested more than 64 levels deep
// are rejected. Binary input is also subject to an expansion budget.
//
// XML input is expected to be UTF-8, with an optional byte order mark.
// The optional <plist> wrapper may have attributes. Value and key elements
// must be attribute-free. Whitespace, comments, processing instructions
// and DOCTYPE declarations are skipped between elements and after the
// root. Text supports CDATA, predefined XML entities and numeric character
// references.
//
// XML integers accept decimal and 0x hexadecimal notation. In both formats
// the supported range is -2^63 through 2^64 - 1. XML data is decoded from
// base64, with whitespace and optional final padding. XML real and date
// values are kept as trimmed text and must be nonempty.
//
// Binary input supports null, booleans, integers, reals, dates, data,
// ASCII and UTF-16BE strings, UIDs, arrays, dictionaries and sets.
// Null becomes NONE. Sets and ordered sets become arrays. Newer binary
// versions and OpenStep text property lists are unsupported.
bool parse(const std::string &data, Value &value);

} // namespace plist
} // namespace tools

#endif // OSXCROSS_WRAPPER_TOOLS_PLIST_H
