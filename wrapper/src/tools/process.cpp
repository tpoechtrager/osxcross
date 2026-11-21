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
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

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

int execProgramWithCallbacks(const std::string &path,
                             std::vector<std::string> args,
                             const PrepareFunction &prepare,
                             const CleanupFunction &cleanup) {
  //
  // Signals
  //
  // The signals that compiler drivers clean up for are passed on to the
  // program, unless they are ignored. The program then ends, and the
  // wrapper cleans up and ends with the same signal. A terminal sends
  // SIGINT and SIGQUIT to both, so the program gets them twice.
  //
  // The handler only records a signal. The loop that waits for the program
  // passes it on while the program runs. A signal that arrives after the
  // program ended ends the wrapper after cleanup(), as it would end a
  // compiler driver. An unavoidable window remains between waitpid() and
  // kill(): a signal passed on there goes to a program that has just ended
  // and is lost.
  //
  // These signals and SIGCHLD stay blocked from before prepare() until
  // after cleanup(), except while sigsuspend() waits for the program. So
  // none ends the wrapper before cleanup(). A handler for SIGCHLD replaces
  // an inherited SIG_IGN, which would discard the status of the program.
  //

  static const int Signals[] = { SIGHUP,  SIGINT,  SIGQUIT, SIGTERM,
                                 SIGPIPE, SIGUSR2, SIGCHLD };
  static const size_t NumSignals = sizeof(Signals) / sizeof(Signals[0]);
  static volatile sig_atomic_t received[NumSignals];
  struct sigaction savedActions[NumSignals];
  sigset_t blocked;
  sigset_t savedMask;
  sigset_t waitMask;

  sigemptyset(&blocked);

  for (size_t i = 0; i < NumSignals; ++i) {
    sigaddset(&blocked, Signals[i]);
    received[i] = 0;
  }

  sigprocmask(SIG_BLOCK, &blocked, &savedMask);

  waitMask = savedMask;
  sigdelset(&waitMask, SIGCHLD);

  for (size_t i = 0; i < NumSignals; ++i) {
    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    sigaction(Signals[i], nullptr, &savedActions[i]);

    if (Signals[i] == SIGCHLD) {
      action.sa_handler = [](int) {};
      action.sa_flags = SA_NOCLDSTOP;
    } else if (savedActions[i].sa_handler == SIG_IGN) {
      action.sa_handler = SIG_IGN;
    } else {
      action.sa_handler = [](int sig) {
        for (size_t j = 0; j < NumSignals; ++j) {
          if (Signals[j] == sig)
            received[j] = 1;
        }
      };
    }

    sigaction(Signals[i], &action, nullptr);
  }

  //
  // Preparation
  //

  if (prepare && !prepare(args)) {
    for (size_t i = 0; i < NumSignals; ++i)
      sigaction(Signals[i], &savedActions[i], nullptr);

    sigprocmask(SIG_SETMASK, &savedMask, nullptr);
    return 1;
  }

  std::vector<char *> argv;
  argv.reserve(args.size() + 1);

  for (const std::string &arg : args)
    argv.push_back(const_cast<char *>(arg.c_str()));

  argv.push_back(nullptr);

  //
  // Start of the program
  //
  // The child reports a failed execvp() through this pipe; execvp() closes
  // it on success.
  //

  int errorPipe[2] = { -1, -1 };
  int error = 0; // of pipe(), fcntl(), fork() or execvp()
  pid_t pid = -1;

  if (pipe(errorPipe) != 0 ||
      fcntl(errorPipe[0], F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(errorPipe[1], F_SETFD, FD_CLOEXEC) != 0 ||
      (pid = fork()) < 0)
    error = errno;

  if (pid == 0) {
    close(errorPipe[0]);

    for (size_t i = 0; i < NumSignals; ++i)
      sigaction(Signals[i], &savedActions[i], nullptr);

    sigprocmask(SIG_SETMASK, &savedMask, nullptr);
    execvp(path.c_str(), argv.data());

    const int execError = errno;

    while (write(errorPipe[1], &execError, sizeof(execError)) < 0 &&
           errno == EINTR) {
    }

    _exit(127);
  }

  //
  // Waiting for the program
  //

  if (errorPipe[1] >= 0)
    close(errorPipe[1]);

  int status = 0;
  int waitError = 0;

  if (pid > 0) {
    for (;;) {
      const pid_t waited = waitpid(pid, &status, WNOHANG);

      if (waited == pid)
        break;

      if (waited < 0 && errno != EINTR) {
        waitError = errno;
        break;
      }

      if (waited == 0) {
        // The program still runs, so its PID cannot be in use by another
        // process.
        for (size_t i = 0; i < NumSignals; ++i) {
          if (received[i]) {
            received[i] = 0;
            kill(pid, Signals[i]);
          }
        }

        sigsuspend(&waitMask);
      }
    }

    // The program has ended, so the pipe holds the error of execvp() or
    // nothing.
    int execError = 0;
    ssize_t n;

    while ((n = read(errorPipe[0], &execError, sizeof(execError))) < 0 &&
           errno == EINTR) {
    }

    if (!waitError && n == sizeof(execError))
      error = execError;
  }

  if (errorPipe[0] >= 0)
    close(errorPipe[0]);

  //
  // Cleanup
  //
  // A signal that arrived after the program ended was not passed on. It is
  // raised again, or is still pending if it arrived after the last
  // sigsuspend(). Once the mask is restored, it ends the wrapper with its
  // original action.
  //

  for (size_t i = 0; i < NumSignals; ++i)
    sigaction(Signals[i], &savedActions[i], nullptr);

  if (cleanup)
    cleanup();

  for (size_t i = 0; i < NumSignals; ++i) {
    if (received[i])
      raise(Signals[i]);
  }

  sigprocmask(SIG_SETMASK, &savedMask, nullptr);

  //
  // Result: return if the program could not be started, otherwise end the
  // wrapper as the program ended
  //

  if (error) {
    err << "cannot execute '" << path << "': " << strerror(error)
        << err.endl();
    return 1;
  }

  if (waitError) {
    err << "cannot wait for '" << path << "': " << strerror(waitError)
        << err.endl();
    exit(EXIT_FAILURE);
  }

  if (WIFSIGNALED(status)) {
    // End with the same signal; like a shell, without writing a core file.
    const int sig = WTERMSIG(status);
    struct rlimit noCoreFile = {};
    sigset_t set;

    setrlimit(RLIMIT_CORE, &noCoreFile);
#ifdef __linux__
    // Linux ignores RLIMIT_CORE when core_pattern passes the core dump to
    // a program such as systemd-coredump.
    prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
#endif
    signal(sig, SIG_DFL);
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    raise(sig);
    exit(128 + sig);
  }

  exit(WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
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
