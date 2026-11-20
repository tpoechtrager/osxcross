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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/tools.h"
#include "tools/plist.h"

namespace tools {
namespace plist {
namespace {

constexpr int MaxDepth = 64;

const Value NoValue;

// The shortest text that reads back as the same double, such as "10.5".
// Infinity and NaN are written as in XML property lists.
std::string formatReal(double real) {
  if (isnan(real))
    return "nan";

  if (isinf(real))
    return real < 0 ? "-infinity" : "+infinity";

  char text[32];

  for (int precision = 1; precision <= 17; ++precision) {
    snprintf(text, sizeof(text), "%.*g", precision, real);
    if (strtod(text, nullptr) == real)
      break;
  }

  return text;
}

// Seconds from the Unix epoch (1970-01-01) to Apple's reference date
// (2001-01-01 00:00:00 UTC), from which binary property lists count dates.
constexpr int64_t AppleEpochOffset = 978307200;

constexpr int64_t SecondsPerDay = 86400;

// A date in the proleptic Gregorian calendar.
struct CivilDate {
  int64_t year;
  int64_t month; // 1 to 12
  int64_t day;   // 1 to 31
};

// The date of a day count since 1970-01-01, after Howard Hinnant's
// civil_from_days(). It works in eras of 400 years that start on March 1,
// so that the leap day is the last day of a year.
CivilDate civilFromDays(int64_t days) {
  days += 719468; // days from 0000-03-01 to 1970-01-01

  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const int64_t dayOfEra = days - era * 146097;
  const int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 -
                             dayOfEra / 146096) / 365;
  const int64_t dayOfYear =
      dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const int64_t monthIndex = (5 * dayOfYear + 2) / 153; // 0: March

  CivilDate date;
  date.day = dayOfYear - (153 * monthIndex + 2) / 5 + 1;
  date.month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
  date.year = yearOfEra + era * 400 + (date.month <= 2);
  return date;
}

// A date of a binary property list, given in seconds since 2001-01-01
// 00:00:00 UTC, in ISO 8601 as XML property lists write it:
// "2026-11-19T12:00:00Z". Fractions of a second are dropped. False if the
// date is outside the years -9999 to 9999 (about +-3e11 seconds).
bool formatDate(double seconds, std::string &text) {
  if (isnan(seconds) || fabs(seconds) > 3.2e11)
    return false;

  // Whole seconds since the Unix epoch, split into days and the time of
  // day; the time of day is never negative.
  const int64_t unixSeconds =
      static_cast<int64_t>(floor(seconds)) + AppleEpochOffset;
  int64_t days = unixSeconds / SecondsPerDay;
  int64_t secondOfDay = unixSeconds % SecondsPerDay;

  if (secondOfDay < 0) {
    secondOfDay += SecondsPerDay;
    --days;
  }

  const CivilDate date = civilFromDays(days);

  if (date.year < -9999 || date.year > 9999)
    return false;

  // Large enough for any long long, so that GCC does not warn about
  // truncation; the year is checked above.
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%04lld-%02lld-%02lldT%02lld:%02lld:%02lldZ",
           static_cast<long long>(date.year),
           static_cast<long long>(date.month),
           static_cast<long long>(date.day),
           static_cast<long long>(secondOfDay / 3600),
           static_cast<long long>(secondOfDay / 60 % 60),
           static_cast<long long>(secondOfDay % 60));
  text = buffer;
  return true;
}

//
// XML property lists
//

// The bytes that base64 text stands for; whitespace is ignored. False if
// the text has other characters, data after the '=' padding, or a length
// that does not fit: groups of four characters, where the last group may
// lack its '=' padding but must have at least two data characters.
bool decodeBase64(const std::string &text, std::string &out) {
  unsigned buffer = 0;
  int bits = 0;
  int padding = 0;
  size_t numChars = 0; // data characters

  for (char c : text) {
    int sextet;

    if (c >= 'A' && c <= 'Z')
      sextet = c - 'A';
    else if (c >= 'a' && c <= 'z')
      sextet = c - 'a' + 26;
    else if (c >= '0' && c <= '9')
      sextet = c - '0' + 52;
    else if (c == '+')
      sextet = 62;
    else if (c == '/')
      sextet = 63;
    else if (c == '=') {
      ++padding;
      continue;
    } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      continue;
    } else {
      return false;
    }

    if (padding)
      return false;

    ++numChars;
    buffer = (buffer << 6) | static_cast<unsigned>(sextet);
    bits += 6;

    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((buffer >> bits) & 0xFF);
    }
  }

  const size_t lastGroup = numChars % 4;

  if (lastGroup == 1)
    return false;

  if (!padding)
    return true;

  return lastGroup && lastGroup + padding == 4;
}

// text without leading and trailing whitespace.
std::string trim(const std::string &text) {
  const char *space = " \t\n\r";
  const size_t first = text.find_first_not_of(space);

  if (first == std::string::npos)
    return std::string();

  return text.substr(first, text.find_last_not_of(space) - first + 1);
}

// An <integer>: decimal with an optional sign, or hexadecimal with "0x",
// from -2^63 to 2^64 - 1, the same range as in binary property lists.
bool isInteger(const std::string &text) {
  size_t pos = 0;
  const bool negative = !text.empty() && text[0] == '-';

  if (!text.empty() && (text[0] == '+' || text[0] == '-'))
    ++pos;

  const bool hex = !text.compare(pos, 2, "0x") || !text.compare(pos, 2, "0X");
  const uint64_t base = hex ? 16 : 10;
  uint64_t magnitude = 0;

  if (hex)
    pos += 2;

  if (pos == text.size())
    return false;

  for (; pos < text.size(); ++pos) {
    const char c = text[pos];
    uint64_t digit;

    if (c >= '0' && c <= '9')
      digit = c - '0';
    else if (hex && c >= 'a' && c <= 'f')
      digit = c - 'a' + 10;
    else if (hex && c >= 'A' && c <= 'F')
      digit = c - 'A' + 10;
    else
      return false;

    if (magnitude > (UINT64_MAX - digit) / base)
      return false;

    magnitude = magnitude * base + digit;
  }

  return !negative || magnitude <= (uint64_t(1) << 63);
}

// A tag: <name ...>, <name .../> or </name>.
struct Tag {
  Tag() : closing(false), empty(false), hasAttributes(false) {}

  std::string name;
  bool closing;       // </name>
  bool empty;         // <name/>
  bool hasAttributes; // <name a="b">; only <plist> may have them
};

// Recursive descent over the XML text; pos is the next character to read.
struct XMLParser {
  explicit XMLParser(const std::string &input) : text(input), pos(0) {}

  const std::string &text;
  size_t pos;

  bool atEnd() const { return pos >= text.size(); }
  char peek() const { return atEnd() ? '\0' : text[pos]; }

  bool startsWith(const char *prefix) const {
    return !text.compare(pos, strlen(prefix), prefix);
  }

  void skipSpace() {
    while (!atEnd() && (text[pos] == ' ' || text[pos] == '\t' ||
                        text[pos] == '\n' || text[pos] == '\r'))
      ++pos;
  }

  // Skip to the end of terminator; false if it does not come.
  bool skipPast(const char *terminator) {
    const size_t end = text.find(terminator, pos);

    if (end == std::string::npos)
      return false;

    pos = end + strlen(terminator);
    return true;
  }

  // Skip whitespace, comments, the XML declaration and the DOCTYPE.
  bool skipMisc() {
    for (;;) {
      skipSpace();

      if (startsWith("<!--")) {
        if (!skipPast("-->"))
          return false;
      } else if (startsWith("<?")) {
        if (!skipPast("?>"))
          return false;
      } else if (startsWith("<!DOCTYPE")) {
        // An internal subset in [...] may contain '>'.
        const size_t close = text.find('>', pos);
        const size_t bracket = text.find('[', pos);

        if (bracket != std::string::npos && bracket < close) {
          pos = bracket;
          if (!skipPast("]"))
            return false;
        }

        if (!skipPast(">"))
          return false;
      } else {
        return true;
      }
    }
  }

  bool readTag(Tag &tag) {
    if (peek() != '<')
      return false;

    ++pos;
    tag = Tag();
    tag.closing = peek() == '/';

    if (tag.closing)
      ++pos;

    while (!atEnd()) {
      const char c = text[pos];
      const bool nameChar = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                            c == '.' || c == ':';
      if (!nameChar)
        break;

      tag.name += c;
      ++pos;
    }

    if (tag.name.empty())
      return false;

    // An end tag has nothing but whitespace after its name.
    if (tag.closing) {
      skipSpace();

      if (peek() != '>')
        return false;

      ++pos;
      return true;
    }

    // Attributes, such as version="1.0", are skipped here; the caller
    // decides whether the element may have them.
    char last = '\0';
    size_t numChars = 0; // characters other than whitespace

    while (!atEnd() && text[pos] != '>') {
      const char c = text[pos++];

      if (c == '"' || c == '\'') {
        const size_t end = text.find(c, pos);
        if (end == std::string::npos)
          return false;
        pos = end + 1;
        last = c;
        ++numChars;
      } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
        last = c;
        ++numChars;
      }
    }

    if (atEnd())
      return false;

    ++pos; // '>'
    tag.empty = last == '/';
    tag.hasAttributes = numChars > (tag.empty ? 1u : 0u);
    return true;
  }

  // An entity after '&': &lt; &gt; &amp; &quot; &apos; &#NN; or &#xHH;.
  bool readEntity(std::string &out) {
    const size_t semicolon = text.find(';', pos);

    if (semicolon == std::string::npos || semicolon - pos > 12)
      return false;

    const std::string name = text.substr(pos + 1, semicolon - pos - 1);

    if (name == "lt") {
      out += '<';
    } else if (name == "gt") {
      out += '>';
    } else if (name == "amp") {
      out += '&';
    } else if (name == "quot") {
      out += '"';
    } else if (name == "apos") {
      out += '\'';
    } else if (name.size() > 1 && name[0] == '#') {
      // XML allows only a lowercase 'x', as in "&#x41;".
      const bool hex = name[1] == 'x';
      const std::string digits = name.substr(hex ? 2 : 1);
      char *end;

      // Unlike strtoul(), XML allows no sign or whitespace.
      if (digits.empty() || digits.size() > 8 ||
          digits.find_first_not_of(hex ? "0123456789abcdefABCDEF"
                                       : "0123456789") != std::string::npos)
        return false;

      const unsigned long code = strtoul(digits.c_str(), &end, hex ? 16 : 10);

      if (*end || !code || code > 0x10FFFF ||
          (code >= 0xD800 && code <= 0xDFFF))
        return false;

      appendUTF8(out, static_cast<unsigned>(code));
    } else {
      return false;
    }

    pos = semicolon + 1;
    return true;
  }

  // The text of an element up to its end tag </name>, with entities and
  // CDATA sections resolved.
  bool readText(const std::string &name, std::string &out) {
    while (!atEnd()) {
      if (startsWith("<![CDATA[")) {
        const size_t start = pos + strlen("<![CDATA[");
        const size_t end = text.find("]]>", start);

        if (end == std::string::npos)
          return false;

        out.append(text, start, end - start);
        pos = end + strlen("]]>");
      } else if (startsWith("<!--")) {
        if (!skipPast("-->"))
          return false;
      } else if (text[pos] == '<') {
        Tag tag;
        return readTag(tag) && tag.closing && tag.name == name;
      } else if (text[pos] == '&') {
        if (!readEntity(out))
          return false;
      } else {
        out += text[pos++];
      }
    }

    return false;
  }

  // The end tag </name>, after whitespace and comments.
  bool readEndTag(const std::string &name) {
    Tag tag;
    return skipMisc() && readTag(tag) && tag.closing && tag.name == name;
  }

  // True if the next tag is an end tag; whitespace and comments before it
  // are skipped.
  bool atEndTag() {
    return skipMisc() && startsWith("</");
  }

  // The value that starts with tag; depth is the number of dictionaries
  // and arrays it is nested in.
  bool parseValue(const Tag &tag, Value &value, int depth) {
    const std::string &name = tag.name;

    if (tag.closing || tag.hasAttributes)
      return false;

    if (name == "dict")
      return depth < MaxDepth && parseDict(tag, value, depth);

    if (name == "array")
      return depth < MaxDepth && parseArray(tag, value, depth);

    if (name == "true" || name == "false") {
      value.type = Value::BOOLEAN;
      value.boolean = name == "true";
      return tag.empty || readEndTag(name);
    }

    std::string content;

    if (!tag.empty && !readText(name, content))
      return false;

    if (name == "string") {
      value.type = Value::STRING;
      value.string = content;
    } else if (name == "integer") {
      value.type = Value::INTEGER;
      value.string = trim(content);
      return isInteger(value.string);
    } else if (name == "real") {
      value.type = Value::REAL;
      value.string = trim(content);
      return !value.string.empty();
    } else if (name == "date") {
      value.type = Value::DATE;
      value.string = trim(content);
      return !value.string.empty();
    } else if (name == "data") {
      value.type = Value::DATA;
      return decodeBase64(content, value.string);
    } else {
      return false; // an unknown element, or a <key> outside a dictionary
    }

    return true;
  }

  bool parseValue(Value &value, int depth) {
    Tag tag;
    return skipMisc() && readTag(tag) && parseValue(tag, value, depth);
  }

  // <dict>: <key> elements, each followed by its value.
  bool parseDict(const Tag &tag, Value &value, int depth) {
    value.type = Value::DICT;

    if (tag.empty)
      return true;

    while (!atEndTag()) {
      Tag keyTag;
      std::string key;

      if (atEnd() || !readTag(keyTag) || keyTag.closing ||
          keyTag.hasAttributes || keyTag.name != "key")
        return false;

      if (!keyTag.empty && !readText("key", key))
        return false;

      value.keys.push_back(key);
      value.values.push_back(Value());

      if (!parseValue(value.values.back(), depth + 1))
        return false;
    }

    return readEndTag("dict");
  }

  bool parseArray(const Tag &tag, Value &value, int depth) {
    value.type = Value::ARRAY;

    if (tag.empty)
      return true;

    while (!atEndTag()) {
      if (atEnd())
        return false;

      value.values.push_back(Value());

      if (!parseValue(value.values.back(), depth + 1))
        return false;
    }

    return readEndTag("array");
  }

  // The whole document: one value, usually inside <plist version="1.0">.
  bool parseDocument(Value &value) {
    // A UTF-8 byte order mark is allowed; other encodings are not supported.
    if (startsWith("\xEF\xBB\xBF"))
      pos += 3;

    Tag tag;

    if (!skipMisc() || !readTag(tag))
      return false;

    if (tag.name == "plist" && !tag.closing) {
      if (tag.empty || !parseValue(value, 0) || !readEndTag("plist"))
        return false;
    } else if (!parseValue(tag, value, 0)) {
      return false;
    }

    return skipMisc() && atEnd();
  }
};

//
// Binary property lists (bplist00)
//

// The layout, after the 8-byte header "bplist00": the objects, then a table
// with the offset of each object, then a 32-byte trailer. Objects refer to
// each other by their index in the offset table.
struct BinaryParser {
  explicit BinaryParser(const std::string &input)
      : data(input), offsetSize(), refSize(), numObjects(), topObject(),
        offsetTable(), memoryUsed(), memoryLimit() {}

  const std::string &data;
  size_t offsetSize;    // bytes per entry of the offset table
  size_t refSize;       // bytes per object reference
  uint64_t numObjects;
  uint64_t topObject;   // the root object
  uint64_t offsetTable; // where the offset table starts; the objects end here
  uint64_t memoryUsed;  // bytes of values, strings and data created so far
  uint64_t memoryLimit;

  // Reserve n bytes of the memory budget; false if it is used up.
  bool reserveBytes(uint64_t n) {
    if (n > memoryLimit - memoryUsed)
      return false;

    memoryUsed += n;
    return true;
  }

  // Reserve the memory of n values, not counting their strings and data.
  bool reserveValues(uint64_t n) {
    return n <= UINT64_MAX / sizeof(Value) && reserveBytes(n * sizeof(Value));
  }

  // A big-endian unsigned number of size bytes at pos; the caller checks
  // that they are inside data.
  uint64_t readUInt(uint64_t pos, size_t size) const {
    uint64_t result = 0;

    for (size_t i = 0; i < size; ++i)
      result = (result << 8) | static_cast<unsigned char>(data[pos + i]);

    return result;
  }

  bool readTrailer() {
    if (data.size() < 8 + 32)
      return false;

    const uint64_t trailer = data.size() - 32;

    offsetSize = static_cast<unsigned char>(data[trailer + 6]);
    refSize = static_cast<unsigned char>(data[trailer + 7]);
    numObjects = readUInt(trailer + 8, 8);
    topObject = readUInt(trailer + 16, 8);
    offsetTable = readUInt(trailer + 24, 8);

    if (offsetSize < 1 || offsetSize > 8 || refSize < 1 || refSize > 8)
      return false;

    if (!numObjects || topObject >= numObjects)
      return false;

    // The offset table lies between the objects and the trailer.
    if (offsetTable < 8 || offsetTable > trailer ||
        numObjects > (trailer - offsetTable) / offsetSize)
      return false;

    // Binary objects can be shared, but the parsed tree owns a copy for each
    // reference. Charge each value and each copied string or data payload
    // against a budget of 128 times the input size plus 1 MiB.
    // Temporary reference vectors and allocator overhead are outside this
    // estimate.
    memoryLimit = 128 * static_cast<uint64_t>(data.size()) + 1024 * 1024;
    return true;
  }

  // The start of object ref in data.
  bool objectStart(uint64_t ref, uint64_t &pos) const {
    if (ref >= numObjects)
      return false;

    pos = readUInt(offsetTable + ref * offsetSize, offsetSize);
    return pos >= 8 && pos < offsetTable;
  }

  // The size of a string, array and the like: the low nibble of the marker,
  // or an integer object that follows the marker if the nibble is 0xF.
  bool readCount(unsigned nibble, uint64_t &pos, uint64_t &count) const {
    if (nibble != 0xF) {
      count = nibble;
      return true;
    }

    if (pos >= offsetTable)
      return false;

    const unsigned marker = static_cast<unsigned char>(data[pos++]);
    const size_t size = size_t(1) << (marker & 0x0F);

    if ((marker & 0xF0) != 0x10 || size > 8 || size > offsetTable - pos)
      return false;

    count = readUInt(pos, size);
    pos += size;
    return true;
  }

  // Read count references of refSize bytes at pos.
  bool readRefs(uint64_t pos, uint64_t count, std::vector<uint64_t> &refs) {
    if (count > (offsetTable - pos) / refSize)
      return false;

    refs.resize(count);

    for (uint64_t i = 0; i < count; ++i)
      refs[i] = readUInt(pos + i * refSize, refSize);

    return true;
  }

  bool parseObject(uint64_t ref, Value &value, int depth) {
    uint64_t pos;

    // The caller has reserved this value in the budget.
    if (depth > MaxDepth || !objectStart(ref, pos))
      return false;

    const unsigned marker = static_cast<unsigned char>(data[pos++]);
    const unsigned nibble = marker & 0x0F;
    const uint64_t available = offsetTable - pos;
    uint64_t count = 0;

    switch (marker >> 4) {
    case 0x0:
      if (marker == 0x00) // null
        return true;

      if (marker != 0x08 && marker != 0x09)
        return false;

      value.type = Value::BOOLEAN;
      value.boolean = marker == 0x09;
      return true;
    case 0x1:
      return parseInteger(pos, nibble, value);
    case 0x2:
      return parseReal(pos, nibble, value);
    case 0x3: {
      if (marker != 0x33 || available < 8)
        return false;

      const uint64_t bits = readUInt(pos, 8);
      double seconds;
      memcpy(&seconds, &bits, sizeof(seconds));
      value.type = Value::DATE;
      return formatDate(seconds, value.string);
    }
    case 0x4: // data
    case 0x5: // ASCII string
      if (!readCount(nibble, pos, count) || count > offsetTable - pos ||
          !reserveBytes(count))
        return false;

      value.type = marker >> 4 == 0x4 ? Value::DATA : Value::STRING;
      value.string.assign(data, pos, count);

      if (value.type == Value::STRING) {
        for (char c : value.string) {
          if (static_cast<unsigned char>(c) >= 0x80)
            return false;
        }
      }

      return true;
    case 0x6: // UTF-16BE string; count is the number of 16-bit units
      // In UTF-8, two bytes of UTF-16 take up to three bytes.
      if (!readCount(nibble, pos, count) || count > (offsetTable - pos) / 2 ||
          !reserveBytes(3 * count))
        return false;

      value.type = Value::STRING;
      return decodeUTF16(pos, count, value.string);
    case 0x8: { // UID
      const size_t size = nibble + 1;

      if (size > 8 || size > available)
        return false;

      value.type = Value::UID;
      value.string = std::to_string(readUInt(pos, size));
      return true;
    }
    case 0xA: // array
    case 0xB: // ordered set
    case 0xC: { // set
      std::vector<uint64_t> refs;

      if (!readCount(nibble, pos, count) ||
          !reserveValues(count) || !readRefs(pos, count, refs))
        return false;

      value.type = Value::ARRAY;
      value.values.resize(count);

      for (uint64_t i = 0; i < count; ++i) {
        if (!parseObject(refs[i], value.values[i], depth + 1))
          return false;
      }

      return true;
    }
    case 0xD: { // dictionary: count key references, then count values
      std::vector<uint64_t> refs;

      // A key is a value of its own while it is read.
      if (!readCount(nibble, pos, count) || count > UINT64_MAX / 2 ||
          !reserveValues(2 * count) || !readRefs(pos, 2 * count, refs))
        return false;

      value.type = Value::DICT;
      value.keys.resize(count);
      value.values.resize(count);

      for (uint64_t i = 0; i < count; ++i) {
        Value key;

        if (!parseObject(refs[i], key, depth + 1) || !key.isString())
          return false;

        value.keys[i] = std::move(key.string);

        if (!parseObject(refs[count + i], value.values[i], depth + 1))
          return false;
      }

      return true;
    }
    default:
      return false;
    }
  }

  // An integer of 1, 2, 4, 8 or 16 bytes. Those of 1, 2 and 4 bytes are
  // unsigned, those of 8 bytes signed.
  bool parseInteger(uint64_t pos, unsigned nibble, Value &value) const {
    const size_t size = size_t(1) << nibble;

    if (nibble > 4 || size > offsetTable - pos)
      return false;

    value.type = Value::INTEGER;

    if (size < 8) {
      value.string = std::to_string(readUInt(pos, size));
      return true;
    }

    const uint64_t bits = readUInt(pos + size - 8, 8);

    // 16 bytes: Apple's and Python's writers use them for numbers from 2^63
    // to 2^64 - 1, with the upper 8 bytes 0. The range from -2^63 to
    // 2^64 - 1 is accepted, the same as for XML.
    if (size == 16) {
      const uint64_t high = readUInt(pos, 8);

      if (!high) {
        value.string = std::to_string(bits);
        return true;
      }

      if (high != UINT64_MAX || !(bits >> 63))
        return false;
    }

    int64_t integer;
    memcpy(&integer, &bits, sizeof(integer));
    value.string = std::to_string(integer);
    return true;
  }

  // A real of 4 (float) or 8 bytes (double).
  bool parseReal(uint64_t pos, unsigned nibble, Value &value) const {
    double real;

    if (nibble == 2 && offsetTable - pos >= 4) {
      const uint32_t bits = static_cast<uint32_t>(readUInt(pos, 4));
      float single;
      memcpy(&single, &bits, sizeof(single));
      real = single;
    } else if (nibble == 3 && offsetTable - pos >= 8) {
      const uint64_t bits = readUInt(pos, 8);
      memcpy(&real, &bits, sizeof(real));
    } else {
      return false;
    }

    value.type = Value::REAL;
    value.string = formatReal(real);
    return true;
  }

  // count UTF-16BE units at pos, as UTF-8. False for a surrogate without
  // its other half.
  bool decodeUTF16(uint64_t pos, uint64_t count, std::string &out) const {
    for (uint64_t i = 0; i < count; ++i) {
      unsigned code = static_cast<unsigned>(readUInt(pos + 2 * i, 2));

      if (code >= 0xDC00 && code <= 0xDFFF)
        return false;

      if (code >= 0xD800 && code <= 0xDBFF) {
        if (++i == count)
          return false;

        const unsigned low = static_cast<unsigned>(readUInt(pos + 2 * i, 2));

        if (low < 0xDC00 || low > 0xDFFF)
          return false;

        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
      }

      appendUTF8(out, code);
    }

    return true;
  }

  bool parseDocument(Value &value) {
    return readTrailer() && reserveValues(1) &&
           parseObject(topObject, value, 0);
  }
};

} // unnamed namespace

const Value &Value::operator[](const char *key) const {
  if (type == DICT) {
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

bool parse(const std::string &data, Value &value) {
  Value result;
  bool ok;

  // Newer binary formats, such as bplist15 and bplist16, are not supported.
  if (!data.compare(0, 6, "bplist")) {
    BinaryParser parser(data);
    ok = !data.compare(0, 8, "bplist00") && parser.parseDocument(result);
  } else {
    XMLParser parser(data);
    ok = parser.parseDocument(result);
  }

  // value is replaced only now, as data may be part of it.
  if (!ok)
    result = Value();

  std::swap(value, result);
  return ok;
}

} // namespace plist
} // namespace tools
