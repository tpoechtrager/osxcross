#!/usr/bin/env bash

export LC_ALL="C"

which clang 2>&1 1>/dev/null || exit 1

x=`clang -target i386-apple-darwin9 2>&1`

case "$x" in
    *i386-apple-darwin9*)
        echo "-ccc-host-triple"
        exit 0
    ;;
esac

echo "-target"
