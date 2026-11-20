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

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include "tools/tools.h"
#include "tools/process.h"

namespace tools {
namespace process {

// Replace the current process with path; args[0] becomes its argv[0].
int execProgram(const std::string &path,
                const std::vector<std::string> &args) {
  std::vector<char *> argv;
  argv.reserve(args.size() + 1);

  for (const std::string &arg : args)
    argv.push_back(const_cast<char *>(arg.c_str()));

  argv.push_back(nullptr);
  execvp(path.c_str(), argv.data());

  const int error = errno;
  err << "cannot execute '" << path << "': " << strerror(error) << err.endl();
  return 1;
}

// Like above, but pass on argv[1..] and use path as argv[0].
int execProgram(const std::string &path, int argc, char **argv) {
  std::vector<std::string> args(1, path);

  if (argc > 1)
    args.insert(args.end(), argv + 1, argv + argc);

  return execProgram(path, args);
}

// Debug output: the original command line and the one passed to
// execProgram().
void printExecProgramArgs(int argc, char **argv,
                          const std::vector<std::string> &args) {
  std::string in;
  std::string out;

  for (int i = 0; i < argc; ++i) {
    in += argv[i];
    in += " ";
  }

  for (const std::string &arg : args) {
    out += arg;
    out += " ";
  }

  dbg << "--> " << in << dbg.endl();
  dbg << "<-- " << out << dbg.endl();
}

bool getProgramOutput(const std::vector<std::string> &args,
                      std::string &output) {
  output.clear();

  if (args.empty())
    return false;

  int fds[2];

  if (pipe(fds) != 0)
    return false;

  // An inherited SIG_IGN would discard the child status before waitpid().
  // Temporarily use the default action, also clearing SA_NOCLDWAIT.
  struct sigaction savedAction, action = {};
  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);

  if (sigaction(SIGCHLD, &action, &savedAction) != 0) {
    close(fds[0]);
    close(fds[1]);
    return false;
  }

  const pid_t pid = fork();

  if (pid < 0) {
    close(fds[0]);
    close(fds[1]);
    sigaction(SIGCHLD, &savedAction, nullptr);
    return false;
  }

  if (pid == 0) {
    close(fds[0]);

    // Install stdout first: the pipe may occupy a standard descriptor
    // when the caller started us with stdin or stderr closed.
    if (dup2(fds[1], STDOUT_FILENO) < 0)
      _exit(127);
    if (fds[1] != STDOUT_FILENO)
      close(fds[1]);

    const int devNull = open("/dev/null", O_RDWR);

    if (devNull < 0 || dup2(devNull, STDIN_FILENO) < 0 ||
        dup2(devNull, STDERR_FILENO) < 0)
      _exit(127);
    if (devNull > STDERR_FILENO)
      close(devNull);

    std::vector<char *> argv;
    argv.reserve(args.size() + 1);

    for (const std::string &arg : args)
      argv.push_back(const_cast<char *>(arg.c_str()));

    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(fds[1]);

  char buf[4096];

  for (;;) {
    const ssize_t n = read(fds[0], buf, sizeof(buf));

    if (n > 0)
      output.append(buf, static_cast<size_t>(n));
    else if (n == 0 || errno != EINTR)
      break;
  }

  close(fds[0]);

  int status;
  pid_t waited;

  do {
    waited = waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);

  const bool success =
      waited == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  sigaction(SIGCHLD, &savedAction, nullptr);
  return success;
}

} // namespace process
} // namespace tools
