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

#include <string>
#include <utility>
#include <vector>

#include <string.h>

#include "tools/tools.h"
#include "tools/json.h"

namespace tools {
namespace json {
namespace {

constexpr int MaxDepth = 64;

const Value NoValue;

// Recursive descent over text; pos is the next character to read.
struct Parser {
  explicit Parser(const std::string &input) : text(input), pos(0) {}

  const std::string &text;
  size_t pos;

  bool atEnd() const { return pos >= text.size(); }
  char peek() const { return atEnd() ? '\0' : text[pos]; }

  void skipSpace() {
    while (!atEnd() && (text[pos] == ' ' || text[pos] == '\t' ||
                        text[pos] == '\n' || text[pos] == '\r'))
      ++pos;
  }

  // Skip whitespace and the character c, if it comes next.
  bool consume(char c) {
    skipSpace();

    if (atEnd() || text[pos] != c)
      return false;

    ++pos;
    return true;
  }

  bool parseValue(Value &value, int depth) {
    skipSpace();

    if (atEnd())
      return false;

    // depth: the number of objects and arrays this value is nested in.
    switch (text[pos]) {
    case '{':
      return depth < MaxDepth && parseObject(value, depth);
    case '[':
      return depth < MaxDepth && parseArray(value, depth);
    case '"':
      value.type = Value::STRING;
      return parseString(value.string);
    case 't':
      value.boolean = true;
      return parseLiteral("true", value, Value::BOOLEAN);
    case 'f':
      return parseLiteral("false", value, Value::BOOLEAN);
    case 'n':
      return parseLiteral("null", value, Value::NONE);
    default:
      return parseNumber(value);
    }
  }

  bool parseObject(Value &value, int depth) {
    value.type = Value::OBJECT;
    ++pos; // '{'

    if (consume('}'))
      return true;

    do {
      std::string key;

      skipSpace();

      if (peek() != '"' || !parseString(key) || !consume(':'))
        return false;

      value.keys.push_back(std::move(key));
      value.values.push_back(Value());

      if (!parseValue(value.values.back(), depth + 1))
        return false;
    } while (consume(','));

    return consume('}');
  }

  bool parseArray(Value &value, int depth) {
    value.type = Value::ARRAY;
    ++pos; // '['

    if (consume(']'))
      return true;

    do {
      value.values.push_back(Value());

      if (!parseValue(value.values.back(), depth + 1))
        return false;
    } while (consume(','));

    return consume(']');
  }

  bool parseLiteral(const char *word, Value &value, Value::Type type) {
    const size_t len = strlen(word);

    if (text.compare(pos, len, word))
      return false;

    value.type = type;
    pos += len;
    return true;
  }

  // -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?, kept as written.
  bool parseNumber(Value &value) {
    const size_t start = pos;

    auto digits = [&]() {
      const size_t first = pos;
      while (peek() >= '0' && peek() <= '9')
        ++pos;
      return pos > first;
    };

    if (peek() == '-')
      ++pos;

    if (peek() == '0')
      ++pos;
    else if (!digits())
      return false;

    if (peek() == '.') {
      ++pos;
      if (!digits())
        return false;
    }

    if (peek() == 'e' || peek() == 'E') {
      ++pos;
      if (peek() == '+' || peek() == '-')
        ++pos;
      if (!digits())
        return false;
    }

    value.type = Value::NUMBER;
    value.string.assign(text, start, pos - start);
    return true;
  }

  // Four hex digits after \u.
  bool parseHex4(unsigned &code) {
    code = 0;

    for (int i = 0; i < 4; ++i) {
      const char c = peek();
      code <<= 4;

      if (c >= '0' && c <= '9')
        code |= c - '0';
      else if (c >= 'a' && c <= 'f')
        code |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F')
        code |= c - 'A' + 10;
      else
        return false;

      ++pos;
    }

    return true;
  }

  // \uXXXX, with a surrogate pair for characters above U+FFFF.
  bool parseUnicodeEscape(std::string &out) {
    unsigned code;

    if (!parseHex4(code))
      return false;

    if (code >= 0xDC00 && code <= 0xDFFF) // low surrogate without high one
      return false;

    if (code >= 0xD800 && code <= 0xDBFF) {
      unsigned low;

      if (text.compare(pos, 2, "\\u"))
        return false;

      pos += 2;

      if (!parseHex4(low) || low < 0xDC00 || low > 0xDFFF)
        return false;

      code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
    }

    appendUTF8(out, code);
    return true;
  }

  bool parseString(std::string &out) {
    ++pos; // '"'

    while (!atEnd()) {
      const char c = text[pos++];

      if (c == '"')
        return true;

      // Control characters must be escaped.
      if (static_cast<unsigned char>(c) < 0x20)
        return false;

      if (c != '\\') {
        out += c;
        continue;
      }

      if (atEnd())
        return false;

      switch (text[pos++]) {
      case '"': out += '"'; break;
      case '\\': out += '\\'; break;
      case '/': out += '/'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      case 'u':
        if (!parseUnicodeEscape(out))
          return false;
        break;
      default:
        return false;
      }
    }

    return false; // no closing '"'
  }
};

} // unnamed namespace

const Value &Value::operator[](const char *key) const {
  if (type == OBJECT) {
    // The last member with this key counts, as with Clang and Apple's
    // CoreFoundation.
    for (size_t i = keys.size(); i > 0; --i) {
      if (keys[i - 1] == key)
        return values[i - 1];
    }
  }

  return NoValue;
}

const Value &Value::operator[](int index) const {
  if (type == ARRAY && index >= 0 && static_cast<size_t>(index) < size())
    return values[index];

  return NoValue;
}

bool parse(const std::string &text, Value &value) {
  Parser parser(text);
  Value result;

  // Read the whole input before changing value, which may own text.
  if (parser.parseValue(result, 0)) {
    parser.skipSpace();

    if (parser.atEnd()) {
      value = std::move(result);
      return true;
    }
  }

  value = Value();
  return false;
}

} // namespace json
} // namespace tools
