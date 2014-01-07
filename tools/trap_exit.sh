#!/usr/bin/env bash

function _exit()
{
    EC=$?
    if [ $EC -ne 0 ]; then
        test -z "$SCRIPT" && SCRIPT=`basename $0`
        echo ""
        echo "exiting with abnormal exit code ($EC)"
        test -n "$OCDEBUG" || echo "run 'OCDEBUG=1 ./$SCRIPT' to enable debug messages"
        echo "removing stale locks..."
        remove_locks
        echo "if it is happening the first time, then just re-run the script"
        echo ""
    fi
}

trap _exit EXIT

