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

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "tools/tools.h"
#include "tools/response-file.h"
#include "tools/process.h"

namespace tools {
namespace responsefile {

//
// Reading response files
//

namespace {

// Characters that separate arguments in a response file. GCC also counts
// \v and \f, as isspace() does; LLVM does not.
bool isWhitespace(char c, Syntax syntax) {
  if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
    return true;

  return syntax == Syntax::GCC && (c == '\v' || c == '\f');
}

// Splits text[begin..end) into arguments. A backslash quotes the next
// character, except in single quotes for GCC. At the end of the text, GCC
// drops it and LLVM keeps it.
void splitArguments(const std::string &text, size_t begin, size_t end,
                    Syntax syntax, std::vector<std::string> &args) {
  std::string arg;
  bool inArg = false; // also true for an empty quoted argument such as ""
  char quote = '\0';

  for (size_t i = begin; i < end; ++i) {
    const char c = text[i];

    if (c == '\\' && (quote != '\'' || syntax != Syntax::GCC)) {
      inArg = true;

      if (i + 1 < end)
        arg += text[++i];
      else if (syntax != Syntax::GCC)
        arg += c;
    } else if (quote) {
      if (c == quote)
        quote = '\0';
      else
        arg += c;
    } else if (isWhitespace(c, syntax)) {
      if (inArg)
        args.push_back(arg);

      arg.clear();
      inArg = false;
    } else {
      inArg = true;

      if (c == '\'' || c == '"')
        quote = c;
      else
        arg += c;
    }
  }

  if (inArg)
    args.push_back(arg);
}

// GCC 15 changed how response files treat backslashes. Earlier versions
// take the character after a backslash literally, wherever it is. Since
// GCC 15, a backslash in single quotes is an ordinary character, in double
// quotes it only escapes \ " $ ` and newline, and before a newline it
// continues the line. A file with a backslash in one of these places means
// something different to each GCC version, and the wrapper does not know
// which version it starts.
//
// Returns the position of the first such backslash in text[0..end), or
// std::string::npos if all GCC versions read the file the same way.
size_t findGCCVersionDifference(const std::string &text, size_t end) {
  char quote = '\0';

  for (size_t i = 0; i < end; ++i) {
    const char c = text[i];

    if (c != '\\') {
      if (quote && c == quote)
        quote = '\0';
      else if (!quote && (c == '\'' || c == '"'))
        quote = c;

      continue;
    }

    if (quote == '\'')
      return i;

    if (i + 1 == end)
      break;

    const char next = text[i + 1];

    if (next == '\n' || (quote == '"' && !strchr("\\\"$`", next)))
      return i;

    ++i;
  }

  return std::string::npos;
}

// Warns that file is passed on unread because it contains a backslash at
// pos that GCC versions read differently.
void warnGCCVersionDifference(const char *file, const std::string &text,
                              size_t pos) {
  const size_t lineStart = text.rfind('\n', pos) + 1; // 0 if there is none
  size_t lineEnd = text.find('\n', pos);
  const size_t lineNumber =
      1 + std::count(text.begin(), text.begin() + lineStart, '\n');

  if (lineEnd == std::string::npos)
    lineEnd = text.size();

  std::string line = text.substr(lineStart, lineEnd - lineStart);

  if (line.size() > 60)
    line = line.substr(0, 57) + "...";

  warn << "response file '" << file << "' is passed to gcc unread"
       << warn.endl();
  warnInfo << "line " << lineNumber << " has a backslash that GCC 14 and "
           << "earlier read differently than GCC 15 and later: " << line
           << warnInfo.endl();
  warnInfo << "options such as -arch, -isysroot and -mmacosx-version-min in "
           << "this file therefore have no effect on osxcross"
           << warnInfo.endl();
  warnInfo << "write each backslash as \\\\ and use double quotes instead "
           << "of single quotes to make the file readable for osxcross"
           << warnInfo.endl();
}

class Expander {
public:
  Expander(Syntax syntax, bool printWarnings)
      : syntax(syntax), printWarnings(printWarnings), expanded() {}

  // Appends arg, or the arguments in the file if arg is @<file>, to args.
  // Like the compiler, it leaves @<file> as it is if the file cannot be
  // read. Only regular files are read, as GCC does. Clang and ld64.lld also
  // read FIFOs such as @<(command); the wrapper passes them on, since
  // reading would consume them. Opening a FIFO without a writer would also
  // block.
  bool expand(const std::string &arg, std::vector<std::string> &args) {
    if (arg.size() < 2 || arg[0] != '@') {
      args.push_back(arg);
      return true;
    }

    const char *file = &arg[1];
    struct stat st;
    const bool found = stat(file, &st) == 0;

    if (!found || !S_ISREG(st.st_mode)) {
      if (found && S_ISFIFO(st.st_mode) && syntax != Syntax::GCC)
        warnPassedOn(file, "osxcross reads regular files only");

      args.push_back(arg);
      return true;
    }

    const int fd = open(file, O_RDONLY | O_NOCTTY);
    std::string content;
    const bool readable = fd >= 0 && getFileContent(fd, content);

    if (fd >= 0)
      close(fd);

    if (!readable) {
      args.push_back(arg);
      return true;
    }

    size_t begin = 0;
    size_t end = content.size();

    if (syntax == Syntax::GCC) {
      // GCC reads the file as a C string, up to the first null byte.
      end = std::min(end, content.find('\0'));

      const size_t pos = findGCCVersionDifference(content, end);

      if (pos != std::string::npos) {
        if (printWarnings)
          warnGCCVersionDifference(file, content, pos);

        args.push_back(arg);
        return true;
      }
    } else if (!content.compare(0, 3, "\xEF\xBB\xBF")) {
      begin = 3; // a UTF-8 byte order mark
    } else if (!content.compare(0, 2, "\xFF\xFE") ||
               !content.compare(0, 2, "\xFE\xFF")) {
      warnPassedOn(file, "osxcross cannot read UTF-16 response files");
      args.push_back(arg);
      return true;
    }

    const FileID id(st.st_dev, st.st_ino);

    for (const FileID &openFile : openFiles) {
      if (openFile == id) {
        err << "response file '" << file << "' includes itself"
            << err.endl();
        return false;
      }
    }

    if (openFiles.size() == MaxNestingDepth) {
      err << "response files are nested too deeply at '" << file << "'"
          << err.endl();
      return false;
    }

    std::vector<std::string> fileArgs;
    splitArguments(content, begin, end, syntax, fileArgs);

    openFiles.push_back(id);
    expanded = true;

    for (const std::string &fileArg : fileArgs) {
      if (!expand(fileArg, args))
        return false;
    }

    openFiles.pop_back();
    return true;
  }

  bool fileExpanded() const { return expanded; }

private:
  typedef std::pair<dev_t, ino_t> FileID;

  static constexpr size_t MaxNestingDepth = 256;

  // Warns that file is passed on unread to clang or ld64.lld, for the given
  // reason.
  void warnPassedOn(const char *file, const char *reason) const {
    if (!printWarnings)
      return;

    const bool lld = syntax == Syntax::LLD;

    warn << "response file '" << file << "' is passed to "
         << (lld ? "ld64.lld" : "clang") << " unread" << warn.endl();
    warnInfo << reason << "; options such as -arch and "
             << (lld ? "-syslibroot" : "-isysroot")
             << " in it have no effect on osxcross" << warnInfo.endl();
  }

  const Syntax syntax;
  const bool printWarnings;      // warn about files left unread
  std::vector<FileID> openFiles; // the files being read, to detect cycles
  bool expanded;                 // a file was replaced with its arguments
};

// The expanded arguments of all calls of expand(). A deque does not move
// its elements when it grows, so pointers to them stay valid.
std::deque<std::string> argStorage;
std::deque<std::vector<char *> > argvStorage;

} // unnamed namespace

bool expand(int argc, char **argv, Syntax syntax, ExpandedArgs &expanded,
            bool printWarnings) {
  expanded.argc = argc;
  expanded.argv = argv;
  expanded.fileExpanded = false;

  bool haveFileArg = false;
  bool windowsQuoting = false;

  // Clang reads response files with Windows rules if --rsp-quoting=windows
  // or --driver-mode=cl is on the command line. Before reading any response
  // file, it compares each argument with these strings, without parsing the
  // options: even "-o --rsp-quoting=windows" switches to Windows rules. The
  // loop checks each argument the same way.
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '@')
      haveFileArg = true;
    else if (!strcmp(argv[i], "--rsp-quoting=windows") ||
             !strcmp(argv[i], "--driver-mode=cl"))
      windowsQuoting = true;
  }

  if (!haveFileArg)
    return true;

  // Windows rules make no sense for macOS builds. ld64.lld does not read
  // these options.
  if (windowsQuoting && syntax == Syntax::CLANG) {
    err << "response files with Windows quoting are not supported"
        << err.endl();
    return false;
  }

  Expander expander(syntax, printWarnings);
  std::vector<std::string> args(1, argv[0]);

  for (int i = 1; i < argc; ++i) {
    if (!expander.expand(argv[i], args))
      return false;
  }

  if (!expander.fileExpanded())
    return true;

  argvStorage.push_back(std::vector<char *>());
  std::vector<char *> &newArgv = argvStorage.back();
  newArgv.reserve(args.size() + 1);

  for (std::string &arg : args) {
    argStorage.push_back(std::string());
    argStorage.back().swap(arg);
    newArgv.push_back(&argStorage.back()[0]);
  }

  newArgv.push_back(nullptr);

  expanded.argc = static_cast<int>(args.size());
  expanded.argv = newArgv.data();
  expanded.fileExpanded = true;
  return true;
}

//
// Running a program with a response file
//

namespace {

// Quoted so that GCC and LLVM tools read arg back unchanged.
std::string quote(const std::string &arg) {
  std::string quoted = "\"";

  for (char c : arg) {
    if (c == '\\' || c == '"')
      quoted += '\\';

    quoted += c;
  }

  return quoted + "\"";
}

// Deletes file, with a warning if that fails.
void deleteResponseFile(const std::string &file) {
  if (unlink(file.c_str()) != 0 && errno != ENOENT) {
    const int error = errno;
    warn << "cannot delete the response file '" << file
         << "': " << strerror(error) << warn.endl();
  }
}

// Writes args[1..] to a temporary response file, whose name it stores in
// file, and replaces them with @<file>.
bool writeResponseFile(std::vector<std::string> &args, std::string &file) {
  std::string content;

  for (size_t i = 1; i < args.size(); ++i) {
    content += quote(args[i]);
    content += '\n';
  }

  file = getTempDir() + "/osxcross-XXXXXX";
  const int fd = mkstemp(&file[0]);

  // errno is saved before err prints anything, which may change it.
  if (fd < 0) {
    const int error = errno;
    err << "cannot create the response file '" << file
        << "': " << strerror(error) << err.endl();
    return false;
  }

  // With SIGXFSZ ignored, a file size limit (ulimit -f) makes write() fail
  // instead of ending the wrapper, so that the file can be deleted.
  struct sigaction ignore = {};
  struct sigaction savedAction;
  ignore.sa_handler = SIG_IGN;
  sigemptyset(&ignore.sa_mask);
  sigaction(SIGXFSZ, &ignore, &savedAction);

  int error = writeFileContent(fd, content) ? 0 : errno;

  if (close(fd) != 0 && !error)
    error = errno;

  sigaction(SIGXFSZ, &savedAction, nullptr);

  // The file is deleted first, in case writing the error fails as well.
  if (error) {
    deleteResponseFile(file);
    err << "cannot write the response file '" << file
        << "': " << strerror(error) << err.endl();
    return false;
  }

  if (getDebugLevel() > 0)
    dbg << "passing the arguments in '" << file << "'" << dbg.endl();

  args.resize(1);
  args.push_back("@" + file);
  return true;
}

} // unnamed namespace

int execProgram(const std::string &path,
                const std::vector<std::string> &args) {
  std::string file;

  return process::execProgramWithCallbacks(
      path, args,
      [&](std::vector<std::string> &childArgs) {
        return writeResponseFile(childArgs, file);
      },
      [&] { deleteResponseFile(file); });
}

} // namespace responsefile
} // namespace tools
