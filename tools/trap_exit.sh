#!/usr/bin/env bash

function check_for_bug_1242300()
{
  if [ -e /etc/issue ]; then
    if [ "`grep -i ubuntu.13.10 /etc/issue`" ]; then
      echo "Ubuntu 13.10 detected. if there was a 'configure:' error"
      echo "please see https://bugs.launchpad.net/ubuntu/+source/llvm-defaults/+bug/1242300"
    fi
  fi
}

function _exit()
{
  EC=$?
  if [ $EC -ne 0 ]; then
    test -z "$SCRIPT" && SCRIPT=`basename $0`
    echo ""
    echo "exiting with abnormal exit code ($EC)"
    test -n "$OCDEBUG" || echo "run 'OCDEBUG=1 ./$SCRIPT' to enable debug messages"
    if [ -n "$CURRENT_BUILD_PROJECT_NAME" ]; then
      ## Build failed. Rebuild everything ##
      rm -f "build/*_built_successfully"
    fi
    echo ""
    test $SCRIPT = "build.sh" && check_for_bug_1242300
  fi
}

trap _exit EXIT
