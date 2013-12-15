#!/usr/bin/env bash
# help clang to find its own intrinsic headers
# this issue appears to be fixed in 3.4+

pushd "${0%/*}" &>/dev/null

set -e

SDK_DIR="$1"

test -n "$SDK_DIR" || { echo "no SDK directory given" && exit 1; }
test -e "$SDK_DIR" || { echo "$SDK_DIR does not exist" && exit 1; }

CLANG_VERSION=`echo "int main(){printf(\"%d.%d\",__clang_major__,__clang_minor__);return 0;}" | clang -xc -ansi -otest - 2>/dev/null && ./test && rm test`
CLANG_DIR=`dirname \`which clang\``

CLANG_INTRIN_DIR="$CLANG_DIR/../include/clang/$CLANG_VERSION/include"

test ! -e "$CLANG_INTRIN_DIR" && CLANG_INTRIN_DIR="$CLANG_DIR/../lib/clang/$CLANG_VERSION/include"
test ! -e "$CLANG_INTRIN_DIR" && CLANG_INTRIN_DIR="$CLANG_DIR/../include/clang/$CLANG_VERSION/include"
test ! -e "$CLANG_INTRIN_DIR" && CLANG_INTRIN_DIR="$CLANG_DIR/../include/clang/$CLANG_VERSION"

test -e "$CLANG_INTRIN_DIR" || { echo "can not find clang intrinsics directory" && exit 1; }
test -f "$CLANG_INTRIN_DIR/xmmintrin.h" || { echo "xmmintrin.h does not exist in $CLANG_INTRIN_DIR" && exit 1; }

echo "found clang intrinsic headers: $CLANG_INTRIN_DIR"

test -f $CLANG_INTRIN_DIR/float.h && ln -sf $CLANG_INTRIN_DIR/float.h $SDK_DIR/usr/include
test -f $CLANG_INTRIN_DIR/stdarg.h && ln -sf $CLANG_INTRIN_DIR/stdarg.h $SDK_DIR/usr/include

ln -sf $CLANG_INTRIN_DIR/*intrin*.h $SDK_DIR/usr/include
ln -sf $CLANG_INTRIN_DIR/mm*.h $SDK_DIR/usr/include
ln -sf $CLANG_INTRIN_DIR/*va*.h $SDK_DIR/usr/include
ln -sf $CLANG_INTRIN_DIR/*cpu*.h $SDK_DIR/usr/include
ln -sf $CLANG_INTRIN_DIR/*math*.h $SDK_DIR/usr/include
ln -sf $CLANG_INTRIN_DIR/*iso*.h $SDK_DIR/usr/include
