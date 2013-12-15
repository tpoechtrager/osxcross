#!/usr/bin/env bash

set -e

prog="cpucount"

pushd "${0%/*}" >/dev/null 2>&1

case "$(uname -s)" in
  *NT*)
    prog="${prog}.exe" ;;
esac

test ! -f $prog && cc cpucount.c -o cpucount

eval "./${prog}"
