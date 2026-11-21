#!/usr/bin/env bash
#
# Check the sources of the wrapper (src/) against CODING_STYLE.md:
#
#   - names and parameter comments such as /*strict=*/ (clang-tidy, rules
#     in .clang-tidy)
#   - lines longer than 80 columns
#   - tabs and trailing whitespace
#   - "*" without a space in front of it (Target*, char**)
#   - license header
#
# Prints the violations and exits with 1 if there are any; prints nothing
# and exits with 0 otherwise.
#
# Usage: ./check_coding_style.sh
#        CLANG_TIDY=/opt/llvm/21.1.0/bin/clang-tidy ./check_coding_style.sh
#

pushd "${0%/*}/src" &>/dev/null

CLANG_TIDY=${CLANG_TIDY:-clang-tidy}
SOURCES=(*.cpp upstream-compiler-option-tables/*.cpp tools/*.cpp programs/*.cpp
         programs/*/*.cpp)
FILES=("${SOURCES[@]}" *.h upstream-compiler-option-tables/*.h tools/*.h
       programs/*.h programs/*/*.h)
status=0

# report <message> <grep/awk output>: print the lines and fail if there are any
report() {
  if [ -n "$2" ]; then
    echo "$1:"
    echo "$2" | sed 's/^/  /'
    status=1
  fi
}

# Names and parameter comments
if command -v "$CLANG_TIDY" &>/dev/null; then
  # The defines that the Makefile passes; their values do not matter here.
  FLAGS=(
    -std=c++0x -I.
    -DOSXCROSS_VERSION='"0"'
    -DOSXCROSS_BUILD_FLAVOR='"llvm"'
    -DOSXCROSS_TARGET='"darwin27.3"'
    -DOSXCROSS_DEFAULT_DEPLOYMENT_TARGET='"11.0"'
    -DOSXCROSS_LINKER_VERSION='"0"'
    -DOSXCROSS_LIBLTO_PATH='""'
    -DOSXCROSS_BUILD_DIR='""'
    -DOSXCROSS_SUPPORTED_ARCHS='"x86_64"'
    -DOSXCROSS_GCC_TARGET_ARCHS='"x86_64"'
  )

  if [ "$(uname -s)" = "Linux" ]; then
    FLAGS+=(-isystem quirks/include)
  fi

  # The headers are checked through the sources that include them.
  for f in "${SOURCES[@]}"; do
    "$CLANG_TIDY" --quiet --warnings-as-errors='*' "$PWD/$f" -- "${FLAGS[@]}" ||
      status=1
  done
else
  echo "$CLANG_TIDY not found; set CLANG_TIDY to its path" 1>&2
  status=1
fi

# Formatting
report "lines longer than 80 columns" \
  "$(awk 'length($0) > 80 { print FILENAME ":" FNR ": " length($0) }' \
     "${FILES[@]}")"

report "tabs" \
  "$(grep -n -F "$(printf '\t')" "${FILES[@]}")"

report "trailing whitespace" \
  "$(grep -n -E ' +$' "${FILES[@]}")"

report "\"*\" without a space in front of it" \
  "$(grep -n -E '[A-Za-z_>]\*+([,)=]| [A-Za-z_])' "${FILES[@]}" |
     grep -v -E '^[^:]+:[0-9]+:[[:space:]]*(//|/\*|\*)')"

# License header
missing=""
for f in "${FILES[@]}"; do
  if ! head -20 "$f" | grep -q "GNU General Public License"; then
    missing+="$f"$'\n'
  fi
done
report "files without a license header" "${missing%$'\n'}"

popd &>/dev/null
exit $status
