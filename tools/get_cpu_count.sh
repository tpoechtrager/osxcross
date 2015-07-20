#!/usr/bin/env bash

set -e

which cc &>/dev/null || { echo "1" && exit 0; }

prog="cpucount"

pushd "${0%/*}" >/dev/null 2>&1

case "$(uname -s)" in
  *NT* | CYGWIN*)
    prog="${prog}.exe" ;;
esac

[ ! -f $prog ] && cc cpucount.c -o cpucount &>/dev/null

./$prog
