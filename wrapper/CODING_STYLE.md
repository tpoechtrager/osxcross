# Coding style of the compiler wrapper

This applies to the C++ code in `wrapper/`.

[`wrapper/check_coding_style.sh`](check_coding_style.sh) checks these rules, see
[Checking the style](#checking-the-style).

## Names

| What | Style | Examples |
|---|---|---|
| Types (classes, structs, enums, typedefs) | PascalCase | `Target`, `OSVersion`, `Stdlib`, `StringVector` |
| Functions and methods | camelCase | `getSDKPath()`, `isXcodeToolName()`, `num()` |
| Variables, parameters, members | camelCase | `sdkPath`, `deploymentTarget`, `wrapperArgs` |
| `const` with static storage (namespace scope or `static`) | PascalCase | `ArchNames`, `PathDiv`, `Programs`, `Needles`, `Options` |
| Local `const` and `constexpr` (not `static`) | camelCase | `libgccDir`, `envVars`, `xcodeTools` |
| Enumerators | UPPER_CASE | `Arch::X86_64`, `Stdlib::LIBCXX`, `ValueMode::JOINED_WITH_EQUALS` |
| Enumerator for an unknown value | `UNKNOWN_<ENUM>` | `UNKNOWN_ARCH`, `UNKNOWN_COMPILER`, `UNKNOWN_FLAVOR` |
| Namespaces | lower case | `target`, `tools`, `program` |
| Template parameters | PascalCase | `T`, `A` |
| Macros | UPPER_CASE | `OSXCROSS_VERSION` |

More rules:

- **Abbreviations:** An abbreviation at the start of a camelCase name is
  written in lower case as a whole (`sdkPath`, `cxxHeaderPath`, `osNum`),
  elsewhere in upper case (`getSDKPath()`, `isGCC()`, `requiredSDK`).
- **"stdlib"** is one word, as in the option `-stdlib=`: `Stdlib`,
  `getStdlibString()`, `stdlibName`.
- **Namespaces follow the directories:** the code in `tools/` is in
  `tools`, the code in `programs/` in `program`, and a subdirectory such
  as `programs/xcode/` in `program::xcode`. Each file in `tools/` except
  `tools.h` and `tools.cpp` has a namespace of its own, named after the
  file without hyphens: `tools/json.h` is `tools::json`,
  `tools/response-file.h` is `tools::responsefile`.
- **Members have no marker** (no `m_` prefix, no `_` suffix).
- **"unknown" enumerators** carry the name of their enum, so that the
  unscoped enums do not clash in a namespace.

Exceptions:

- Functions named after a tool keep the tool's name: `sw_vers`,
  `pkg_config`, `xcrun`, `xcodebuild`, `lipo`, `ld`, `as`.
- The handlers in `programs/xcode/xcrun.cpp` are named after their option
  (`sdk()`, `find()`, `run()`).
- Enumerators of `ColorCode` follow the ANSI names (`FG_RED`, `BG_BLUE`).
- Library names in identifiers keep their usual spelling: `libgccDir`,
  `libstdcxxDir`, `useGCCLibstdcxx`, `multilib`.

## Formatting

- Indent with 2 spaces, no tabs.
- No line is longer than 80 columns.
- `*` and `&` belong to the name: `Target *target`, `const char *path`,
  `Target &target`. Without a name they are still preceded by a space:
  `int version(Target *, char **)`.
- Comments are written in English.
- A `bool` or a literal whose meaning is not clear at the call site gets
  the parameter name as a comment: `parseOSVersion(text, /*strict=*/true)`.
- A namespace ends with a comment that names it: `} // namespace target`.
  An unnamed namespace ends with `} // unnamed namespace`, the term the C++
  standard uses, not "anonymous namespace".
- New files start with the license header of the other files, with the
  year the file was created.

## Language and platforms

- The wrapper is built with `-std=c++0x`; do not use newer language or
  library features.
- It must build and work on Linux, macOS, FreeBSD and OpenBSD. Code that
  only works on one system goes into an `#ifdef` branch, see the per-OS
  branches in `tools.cpp`.
- It must build without warnings with current Clang and GCC, and with the
  oldest host compilers that are still supported (GCC 9.5 and Clang 10).

## Checking the style

Run [`wrapper/check_coding_style.sh`](check_coding_style.sh). It checks:

- the names, with `clang-tidy` (tested with 21.1.0) and the rules in
  [`wrapper/src/.clang-tidy`](src/.clang-tidy),
- that a comment such as `/*strict=*/` names the parameter it stands
  for, also with `clang-tidy`,
- lines longer than 80 columns,
- tabs and trailing whitespace,
- a `*` without a space in front of it,
- the license header.

```sh
./wrapper/check_coding_style.sh
CLANG_TIDY=/opt/llvm/21.1.0/bin/clang-tidy ./wrapper/check_coding_style.sh
```

No output and exit code 0 mean that the sources follow the rules.
