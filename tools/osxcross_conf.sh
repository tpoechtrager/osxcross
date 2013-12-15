#!/usr/bin/env bash

pushd "${0%/*}" &>/dev/null

OSXCROSS_CONF=`which osxcross-conf 2>/dev/null`
test $? -eq 0 || OSXCROSS_CONF="../target/bin/osxcross-conf"
test -f $OSXCROSS_CONF || exit 1

$OSXCROSS_CONF || exit 1
`dirname $OSXCROSS_CONF`/osxcross-env

popd &>/dev/null
