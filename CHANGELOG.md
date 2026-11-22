# Changelog

All notable changes to OSXCross, newest first.

## v3.1.3

### Added

- wrapper: The compiler wrapper now reads response files (`@file`), so options such as `-arch`,  
  `-isysroot` and `-mmacosx-version-min` take effect when build systems like CMake and Ninja put  
  them in a file to stay within command-line length limits. After expanding a response file, the  
  wrapper passes all arguments to the compiler in a single temporary response file.
  - With the `llvm` flavor, the `as` and `ld` shims also read response files. This lets `as` honor  
    `-o` in the file and lets `ld` handle options such as `-macos_version_min` and `-ld_classic`  
    before invoking `ld64.lld`.
  - This implementation has limitations that are unlikely to affect typical macOS cross-builds:
    - The wrapper passes UTF-16 response files and FIFOs through to Clang and `ld64.lld`. It does  
      the same for GCC response files that GCC 14 and GCC 15 interpret differently. In each case,  
      it prints a warning, and options in the file do not affect the wrapper.
    - The wrapper rejects Clang response files when `--rsp-quoting=windows` or `--driver-mode=cl`  
      selects Windows quoting.

    These limitations can be addressed if they turn out to affect real builds.

### Fixed

- wrapper: Darwin versions 20 to 22 without a minor version, such as `darwin22` in  
  `x86_64-apple-darwin22-clang`, were parsed wrong. The wrapper then looked for `MacOSX13.-1.sdk`  
  and stopped with "cannot find macOS SDK".

## v3.1.2

### Added

- wrapper: `xcodebuild`: Answers SDK queries. `xcodebuild -version -sdk <sdk> [<key>]` prints the  
  value of a key such as `Path` or `SDKVersion`; without a key, it lists all of the SDK's metadata.  
  Together with `xcodebuild -license check`, which now succeeds, `qmake` should work with OSXCross.  
  `qmake` calls `/usr/bin/xcrun` and `/usr/bin/xcode-select` by absolute path, so these must exist.

### Changed

- Build scripts now require GNU sed (`gsed`) on macOS hosts.
- wrapper: `xcrun`: When given several query options, such as `--show-sdk-path` and  
  `--show-sdk-version`, `xcrun` runs only the last query and uses the last `-sdk` option.
- wrapper: When an SDK is missing, the error reports which option or environment variable selected  
  its path, for example `cannot find macOS SDK (expected in: /x, from SDKROOT)`.
- wrapper: Deployment targets below macOS 10.6, including 10.5 patch releases such as 10.5.8, now  
  get the same warning with every compiler:  
  `deployment targets below macOS 10.6 are unsupported; the resulting binaries may not work`.  
  Previously, GCC and Clang with GCC's libstdc++ stopped with an error, and 10.5 patch releases got  
  no warning. For cctools-based flavors, a note adds that the linker may abort with an assertion.  
  `OSXCROSS_NO_10_5_DEPRECATION_WARNING` suppresses both. For older macOS versions, the  
  `osxcross-1.1` branch is still available and maintained.
- wrapper: `xcodebuild`: `-version` now reports Xcode 26.6 (build 17F113) instead of 11.0.0. The SDK  
  for `-version -sdk` comes only from `-sdk`; `SDKROOT` is not used. Unknown SDK keys print nothing.  
  An unknown option, a second `-sdk` with a value or an SDK that cannot be found ends with status  
  64, as with Apple's xcodebuild.
- wrapper: `xcrun`: Follows Apple's command-line handling more closely. Extra arguments after  
  `-f <tool>` or after a query such as `--show-sdk-path` are rejected, as is a call with neither a  
  tool nor a query, such as `xcrun -sdk macosx`. Usage errors exit with status 64. The tool name no  
  longer has to follow `-f` directly: `xcrun -f -sdk macosx clang` finds `clang`. `xcrun --version`  
  and `xcrun -h` no longer need an SDK.

### Fixed

- wrapper: `xcrun`: `-sdk` accepts any SDK directory, including a relative path or a directory not  
  named `MacOSX*.sdk`. Previously, such a directory was rejected with `'-sdk': expected macOS SDK`.
- wrapper: SDKs before 11.0, which have no `SDKSettings.json`, now get their version from  
  `SDKSettings.plist`. Previously, it came only from the directory name, so an SDK directory with  
  another name, such as a plain `MacOSX.sdk`, had no version and could not be used. An invalid  
  version in either file, such as `10.9beta`, is now ignored, and the next source is used.
- wrapper: `xcrun`: Skips non-executable target-prefixed files when looking up a tool by name.
- wrapper: On OpenBSD, tools can now be invoked using a relative path such as `bin/o64-clang`.
- wrapper: `as`: With the `llvm` build flavor, the `as` shim, which invokes Clang to assemble GCC's  
  output, now uses GCC's SDK. Previously, it chose the SDK itself: after `gcc -isysroot` it  
  assembled with another SDK, and an invalid `SDKROOT` that GCC ignored made the call fail.

## v3.1.1

### Added

- wrapper: Optional check against the SDK's minimum deployment target, enabled by setting  
  `OSXCROSS_CHECK_MIN_DEPLOYMENT_TARGET`. Deployment targets below `MinimumDeploymentTarget` in  
  `SDKSettings.json` (e.g. 12.0 for SDK 27.3) are then rejected. SDKs before 11.0 do not state a  
  minimum; for them, the wrapper prints a warning instead.

### Changed

- `build.sh`: The default deployment target is now set with `DEFAULT_DEPLOYMENT_TARGET`, and  
  `osxcross-conf` prints it as `OSXCROSS_DEFAULT_DEPLOYMENT_TARGET`.
- wrapper: Deployment targets with a patch level, such as `-mmacosx-version-min=27.3.1` with SDK  
  27.3, are now accepted. The upper limit is the SDK's `MaximumDeploymentTarget` from  
  `SDKSettings.json` (27.3.99 for SDK 27.3). SDKs before 11.0 have no `SDKSettings.json`; for them,  
  the limit is the SDK version with patch level 99 (10.13.99 for SDK 10.13).

### Fixed

- wrapper: When the wrapper looks for the compiler or another tool in `PATH`, it now skips  
  directories and files that are not executable, as a shell does.
- wrapper: `pkg-config` no longer joins a user-set sysroot and the MacPorts sysroot with `:`. This  
  happened when MacPorts packages were installed and `OSXCROSS_PKG_CONFIG_SYSROOT_DIR` (or  
  `PKG_CONFIG_SYSROOT_DIR` with `OSXCROSS_PKG_CONFIG_USE_NATIVE_VARIABLES`) was set, and it produced  
  broken `-I` and `-L` flags. The user's sysroot is now used as is, and the MacPorts packages are  
  not added.
- wrapper: With the `llvm` build flavor, the `as` shim, which invokes Clang to assemble GCC's  
  output, now gets the full deployment target, such as 10.6.1. Previously, it got only 10.6, which  
  the object files then recorded. Linked executables were not affected.
- `build.sh`: Heading underlines in the build banner did not always match the length of the heading.

### Removed

- wrapper: The environment variable `OSXCROSS_ENABLE_WERROR_IMPLICIT_FUNCTION_DECLARATION`. Clang 16  
  and later, like Apple Clang, already reject implicit function declarations in C99 and later modes.
- wrapper: The fixed minimum deployment target of 10.4.

## v3.1.0

### Added

- Support for the macOS 27.2 and 27.3 SDKs.
- `build_gcc.sh`: `GCC_FAST_BUILD=1` builds GCC without host optimization, debug information and  
  checking, for faster builds.
- `build_gcc.sh`: optional download of GCC's build prerequisites. The script asks first (default:  
  no); `DOWNLOAD_GCC_PREREQUISITES=1` downloads them without asking  
  ([#403](https://github.com/tpoechtrager/osxcross/issues/403)).
- `build_compiler_rt.sh`: support for Clang 24 (main).

### Changed

- `latest` flavor: updated to cctools 1035.1.102 and ld64 957.1.
- `build_clang.sh`: updated Clang to 23.1.5.
- `build_apple_clang.sh`: support for Apple Clang 23, now the default.
- `build_compiler_rt.sh`: Clang 23 uses `release/23.x` instead of `main`.
- Clang, GCC and SDK packaging instructions moved to `README.BUILD-CLANG.md`, `README.BUILD-GCC.md`  
  and `README.SDK.md`.
- `build.sh` warns if a macOS 27 SDK is used with an `ld64.lld` older than 23.1.3.
- macOS hosts: the `stable` and `latest` flavors use the system libxar and no longer build xar  
  ([#331](https://github.com/tpoechtrager/osxcross/pull/331)).

### Fixed

- `build.sh` now defaults to macOS 10.13 with 13.x SDKs and macOS 12.0 with 27.x SDKs, instead of a  
  target below those SDKs' minima ([#449](https://github.com/tpoechtrager/osxcross/issues/449)).
- `build_gcc.sh` now passes the selected deployment target to GCC runtime builds and overrides the  
  libgcc/CRT minimum versions where supported by GCC's makefiles  
  ([#485](https://github.com/tpoechtrager/osxcross/issues/485)):
  - `libgcc_s.1.dylib` is linked for at least macOS 10.7 (ld64 needs that for  
    `-reexported_symbols_list`) and not built with SDKs older than 10.7.
  - The arm64 `libgcc` honors the deployment target instead of a fixed macOS 11.0, while retaining  
    the architecture's minimum of 11.0.
- `gen_sdk_package.sh` packaged an SDK several times if Xcode contains aliases of it  
  ([#402](https://github.com/tpoechtrager/osxcross/issues/402)).
- `gen_sdk_package.sh` did not find the `MacOSX*.pkg` SDKs of Xcode 3.2.x ("No SDK found").

### Removed

- Support for the macOS 10.16 SDK (only used by Xcode 12 betas; it no longer worked since darwin20  
  maps to macOS 11). This also fixes SDK 11.0 installed as `MacOSX11.sdk`, which was not found.

### Compiler wrapper

The wrapper was refactored extensively. The user-visible changes are listed below.

**Option parsing**

- The OSXCross wrapper now recognizes Clang and GCC options that take one or more following  
  arguments as values. It needs this information to distinguish those values from options that  
  configure the wrapper itself. For example, in `-o -resource-dir`, `-resource-dir` is the output  
  file name for Clang or GCC, not an option to configure Clang's resource directory. Previously, the  
  wrapper interpreted it as an option and failed with a missing-value error. These options and their  
  values are now passed to Clang or GCC unchanged. This also covers multi-value options such as  
  `-sectcreate`; arguments after `--` are passed on without further parsing.
- Options passed through `-Xclang`, `-Xlinker`, `-Xassembler`, `-Xpreprocessor`, `-Xanalyzer`,  
  `-Xopenmp-target` and `-Xarch_<arch>` are forwarded with their values.
- The wrapper rejects `-stdlib=`, `-isysroot`, `--sysroot` and certain header/setup options after  
  `-Xarch_<arch>`, with an explanation that these settings apply to all architectures.
- Clang's other spellings of the wrapper's options are recognized too: `--stdlib`, `--language`,  
  `--include-directory`, `--compile`, `--preprocess`, `--assemble`, `--dependencies` and  
  `--user-dependencies`. They use the same setup and validation as their short spellings.
- The longest matching option name wins; `-isystem-after <dir>` is no longer parsed as `-isystem`  
  with the value `-after`.

**SDK selection**

- The wrapper selects the SDK by the following priority, regardless of argument order:
  1. `-isysroot` (the last one wins)
  2. `--sysroot` (the last one wins)
  3. `SDKROOT`, if it is an absolute path to an existing directory other than `/` (other values such  
     as `macosx` are ignored)
  4. `OSXCROSS_SDKROOT`
  5. the SDK installed by `build.sh`
- The selected SDK is now used consistently for compiler arguments and for the wrapper's  
  deployment-target, architecture and C++ library checks. Previously, an SDK override could change  
  the headers used by Clang while those checks still used the installed SDK; GCC continued to use  
  the installed SDK altogether.
- `osxcross-conf`, `osxcross-man`, `sw_vers` and the `ld` shim now honor `SDKROOT` directly, in  
  addition to `OSXCROSS_SDKROOT`, without requiring a call through `xcrun`.
- `xcrun --show-sdk-path` and `--show-sdk-version` now report the currently selected SDK, including  
  selections through `-sdk`, `SDKROOT` or `OSXCROSS_SDKROOT`, instead of the SDK found at startup.  
  `-sdk` takes precedence; a set `SDKROOT` takes precedence over `OSXCROSS_SDKROOT`. An empty  
  `SDKROOT`, `/`, `macosx` or `macosx.internal` selects the default SDK and clears  
  `OSXCROSS_SDKROOT`.
- An empty `OSXCROSS_SDKROOT` counts as not set; before, every compiler call failed with  
  `cannot find macOS SDK (expected in: )`. An empty `-isysroot` or `--sysroot` is an error.
- `OSXCROSS_SDK_SEARCH_DIR` (undocumented) is removed. The wrapper stops with an error if it is  
  nonempty; use `OSXCROSS_SDKROOT` instead.

**SDK version**

- The version is read from the SDK's `SDKSettings.json` (SDKs 11.0 and later), as Apple's clang and  
  xcrun do. Xcode's `MacOSX.sdk` and SDKs named only after their major version (`MacOSX15.sdk`) now  
  work and get their exact version. If the file has no usable version, the wrapper falls back to the  
  directory name, as for older SDKs.
- If neither has a version, the wrapper stops with a clear error instead of  
  `targeted macOS version must be <= 0.0.0 (SDK)`.
- A `MacOSX.sdk` symlink to a versioned SDK no longer reports 0.0.
- Darwin targets with a two-digit minor version are read correctly: `darwin24.10` is macOS 15.10,  
  not 15.1.

**Clang's intrinsic headers**

- If the headers are not in one of the known directories, the wrapper asks the compiler  
  (`clang -print-resource-dir`). This fixes compilers whose `bin/clang` is a wrapper script or lives  
  in another prefix than its headers ([#342](https://github.com/tpoechtrager/osxcross/issues/342),  
  [#136](https://github.com/tpoechtrager/osxcross/issues/136)).  
  `OSXCROSS_FIND_INTRINSICS_VIA_PRINT_RESOURCE_DIR=1` skips the known directories. The lookup now  
  also runs on macOS hosts.
- Gentoo's clang layout (`/usr/lib/llvm/<version>`) is known  
  ([#487](https://github.com/tpoechtrager/osxcross/pull/487), Leah Anderson).
- `-foc-intrinsic-path` accepts absolute paths and paths relative to the compiler's `bin` directory.
- The headers are not added if `-resource-dir`, `-nostdinc` (including `--no-standard-includes`) or  
  `-nobuiltininc` is given; these decide which builtin headers clang uses.
- A resource directory named `64bit` is no longer read as clang 64.

**GCC's libstdc++ (`clang++-gstdc++`, `-foc-use-gcc-libstdc++`)**

- `-femulated-tls` is the default, to match the TLS ABI of GCC's libstdc++; `-fno-emulated-tls`  
  turns it off.
- With GCC runtimes providing `libemutls_w.a`, the system unwinder is used instead of `libgcc_eh`.  
  `libemutls_w` (and `libheapt_w`, if present) is linked  
  ([#484](https://github.com/tpoechtrager/osxcross/issues/484)).
- `clang++-gstdc++` now locates GCC's headers and runtime libraries relative to the OSXCross  
  installation instead of deriving their location from the SDK path. Selecting an SDK outside  
  `target/SDK` no longer makes it look for GCC in the wrong directory.
- GCC with libstdc++ no longer gets a useless `-isystem <SDK>`.

**`llvm` flavor: `as` and `ld`**

- `as` ran `xcrun clang` from `PATH`. Without `target/bin` in `PATH` it failed, with another  
  OSXCross in `PATH` it used that one, and without `-arch` it assembled for the default architecture  
  instead of the one in its name. It now runs the matching `<triple>-clang` directly.
- `as` now honors `SDKROOT` and preserves `OSXCROSS_SDKROOT` when `SDKROOT` is unset, using the same  
  environment-selection rules as `xcrun`. A nested `xcrun` keeps the SDK selected by the outer  
  `xcrun -sdk` invocation.
- `ld` passes `-syslibroot` with the SDK the compiler wrapper would use; without it, linking failed  
  with `library not found for -lSystem`.
- The SDK version in `-platform_version` comes from `-sdk_version` or the SDK given with  
  `-syslibroot`. `-sdk_version` is no longer passed on to `ld64.lld`, which ignored it.
- Without a deployment target, `ld` uses the build default (or the SDK version if no build default  
  exists), raised to the architecture's minimum, as in the compiler wrapper. An empty or invalid  
  `MACOSX_DEPLOYMENT_TARGET` counts as not set; before, `ld64.lld` failed.
- `ld -v` with further arguments printed the version but did not link.
- `-ld_classic` and `-ld_new` are dropped; `ld64.lld` read them as `-l<library>`.
- `-fuse-ld=lld` is only added when linking, so `-M`, `-MM`, `-fsyntax-only`, `--analyze` and  
  `-emit-ast` work with `-Werror`.

**Other changes**

- If a program cannot be started, the wrapper says why: `cannot execute '<path>': <reason>`. If it  
  cannot find its own directory, it prints an error instead of aborting.
- Started through a symlink outside the installation, the wrapper now finds the installation on  
  macOS and OpenBSD hosts too.
- `OCDEBUG=0` and negative values turn off all debug output. Before, the `as` and `ld` shims printed  
  debug output whenever `OCDEBUG` was set.
- On macOS hosts, the wrapper no longer links against CoreServices.
- The Cygwin code is removed (Cygwin has not been supported since v1.0).

**Code and build**

- The sources moved to `wrapper/src/`. `wrapper/src/Makefile` defaults to the `stable` flavor  
  (before: `unknown`) and rebuilds objects when a header changes.
- The helpers in `tools/` return `std::string` and containers instead of filling buffers. Tool  
  dispatch uses the common `execProgram()` helper.
- The headers are self-contained; code used only by one file moved into it. Unused functions are  
  removed.
- `Target::setup()` is divided into sections, and the members of `Target` are grouped and renamed.
- One naming and formatting style, checked by `wrapper/check_coding_style.sh` (clang-tidy).
- A small JSON reader (`tools/json.h`) for `SDKSettings.json`.
- `wrapper/src/passthrough-compiler-options/generate.py` generates the option table from Clang's  
  `Options.td` using `llvm-tblgen` and from GCC's `*.opt` files. It records how many following  
  arguments each option consumes and excludes options already handled by the wrapper. The parser  
  supports combined value modes, multi-value options and appending the generated table.

## v3.0

OSXCross now has three build flavors. 3.0 merges the `testing` branch (newer cctools and ld64) and  
the `2.0-llvm-based` feature branch (LLVM tools and `ld64.lld`) into master; there was no 2.0  
release. This version covers all changes since v1.5.

### Added

- Build flavors, selected interactively or with `BUILD_FLAVOR=stable|latest|llvm` (default for  
  unattended builds: `stable`):
  - `stable`: cctools 986 and ld64 711
  - `latest`: cctools 1030.6.3 and ld64 956.6
  - `llvm`: LLVM tools and `ld64.lld`, with `as`, `ld` and `lipo` wrappers (optionally cctools  
    `lipo`); no i386 and x86_64h
- Support for the macOS 14, 15, 26 and 27 SDKs.
- `build.sh`: `ENABLE_ARCHS` restricts the target architectures.
- `build.sh` detects existing OSXCross installations and warns before reusing build and target  
  directories.
- Experimental ARM64 GCC support.
- `build_compiler_rt.sh`: support for Clang versions through 23 (Clang 23 uses LLVM main).
- CMake launchers (`<triple>-cmake`, `-cmake-gcc`, `-cmake-clang-gstdc++`, ...) and Objective-C  
  settings in the CMake toolchain.
- `xcrun --show-sdk-platform-version`.
- Setting `OSXCROSS_PRETEND_TO_BE_APPLE_CLANG` makes the Clang wrapper add  
  `-D__apple_build_version__=1`, so code guarded by this Apple Clang macro is enabled with upstream  
  Clang too. Any set value, including `0` or an empty value, enables it.
- `osxcross-macports`: packages for "any" architecture, better version matching, arm64 by default  
  from macOS 11.

### Changed

- Default versions: GCC 16.1.0 (`./build_gcc.sh`), Clang 22.1.8 (`./build_clang.sh`) and Apple Clang  
  21 (`./build_apple_clang.sh`).
- `tools/get_dependencies.sh` is refactored and covers all flavors.

### Fixed

- `build.sh` patches the libc++ `<exception>` header in the 10.7 and 10.8 SDKs to remove the  
  misplaced `_ATTRIBUTE(noreturn)` from the friend declaration of `rethrow_exception`, fixing C++  
  compilation errors.
- `build.sh` patches `usr/include/math.h` in macOS 27 SDKs to define missing `NAN` and `INFINITY`  
  macros when compiling C++ with Clang older than 22, fixing libc++ compilation errors.
- When targeting macOS 11 or later with GCC, the wrapper no longer replaces the selected deployment  
  target with 10.16. That compatibility workaround is now limited to Clang older than 11, which  
  cannot parse macOS 11 version numbers.
- `build_clang.sh` adds missing `<cstdint>` includes to LLVM 11 through 18 sources, fixing builds  
  with recent GCC versions whose standard headers no longer include it indirectly.
- Darwin versions of recent targets ([#427](https://github.com/tpoechtrager/osxcross/pull/427)).

### Removed

- `build_llvm_dsymutil.sh`.
- `build_binutils.sh`, which built binutils and GDB.

## v1.5

### Changed

- Updated cctools to 986 and ld64 to 711
- Updated TAPI library to 1300.6.5

## v1.4

### Changed

- Updated cctools to 973.0.1 and ld64 to 609

## v1.3

### Added

- Support for arm64 and arm64e targets

### Changed

- Updated TAPI library to 1100.0.11

## v1.2

### Added

- Support for ld64 `-lto_library <path>`

### Changed

- Updated cctools to 949.0.1 and ld64 to 512.4

### Removed

- Support for <= 10.5 SDKs. Use `osxcross-1.1` branch for these.

## v1.1

### Added

- Support for OpenBSD again

### Changed

- Updated cctools to 927.0.2 and ld64 to 450.3
- TARGET_DIR can now be configured via env (`TARGET_DIR=<path> ./build.sh`)
- Various other improvements and fixes

## v1.0

### Added

- Support for TAPIv3 stubs (including "zippering" target)
- Support for macOS SDKs up to 10.15
- SDK packaging script for ".xip" Xcode images
  - `tools/gen_sdk_package_pbzx.sh <xcode.xip>`
    - tested up to Xcode 10.2.1

### Changed

- cctools, ld64, apple-libtapi and xar are now "git clone"'d and no longer come with OSXCross.
- Updated cctools to 921 and ld64 to 409.12

### Fixed

- Implemented fix for [#171](https://github.com/tpoechtrager/osxcross/issues/171)
- Implemented fix for [#178](https://github.com/tpoechtrager/osxcross/issues/178)
- Implemented fix for [#182](https://github.com/tpoechtrager/osxcross/issues/182)

### Removed

- Support for Cygwin and \*BSD (besides FreeBSD)
- Support for building OSXCross with GCC

<details>
<summary>v0.1 – v0.15</summary>

## v0.15

### Added

- Support for ThinLTO:
  - https://clang.llvm.org/docs/ThinLTO.html
  - http://blog.llvm.org/2016/06/thinlto-scalable-and-incremental-lto.html
  - Requires LLVM/Clang >= 3.8. LLVM/Clang >= 3.9 is recommended.
- `USE_CLANG_AS` build.sh option:
  - Use Clang as assembler.

### Changed

- Updated cctools to 895
- Updated ld64 to 274.2 (without TAPIv2 support)

## v0.14

### Changed

- Updated cctools to 886
- Updated ld64 to 264.3.102

## v0.13

### Changed

- Updated cctools to 877.8
- Updated ld64 to 253.9

## v0.12

### Added

- Support for Xcode 7.x / El Capitan

### Changed

- Updated cctools to 877.5
- Updated ld64 to 253.3

## v0.11

### Added

- Support for `-stdc++` and `-gstdc++` compiler "shortcuts":
  - `o32-clang++`: uses libstdc++ for <= 10.8 and libc++ for >= 10.9
  - `o32-clang++-libc++`: uses the SDK's libc++
  - `o32-clang++-stdc++`: uses the SDK's libstdc++
  - `o32-clang++-gstdc++`: uses GCC's (build_GCC.sh) libstdc++
- Unit tests (wrapper)
- llvm-dsymutil build script
- dsymutil is now wrapped to llvm-dsymutil (LLVM >= 3.8 only)

### Changed

- `-oc-use-GCC-libs` has been renamed to `-foc-use-GCC-libstdc++`
- Support for generating fat object files with GCC and `-foc-use-GCC-libstdc++` has been removed.
- Support for setting the language standard via `OSXCROSS_C_STANDARD` / `OSXCROSS_CXX_STANDARD`  
  (env) has been removed

## v0.10

### Added

- Windows support via Cygwin
- OpenBSD and DragonFlyBSD support
- Linux ARM support
- Linux PPC/PPC64 support: Totally untested, but compiles
- GCC compiler support: OSXCross can now be built with GCC
- `osxcross-man` man page tool
- p7zip sdk packaging script
- A minimalistic xcrun tool
- Support for ccache symlinks
- darling-dmg sdk packaging script
- include path warnings for /usr/include and /usr/local/include (can be switched off via  
  `OSXCROSS_NO_INCLUDE_PATH_WARNINGS=1`)
- An option to silence osxcross extension warnings (`OSXCROSS_NO_EXTENSION_WARNINGS=1`)

### Changed

- Improved and colorized wrapper error/warning/debug/info messages
- Updated cctools to 870
- Updated ld64 to 242

## v0.9

### Added

- binutils / gdb build script
- `OSXCROSS_GCC_NO_STATIC_RUNTIME` option (env)
- osxcross-macports: A minimalistic macports package manager

### Changed

- Updated cctools to 862
- Updated ld64 to 241.9

## v0.8

### Added

- Support for new `-arch x86_64h` (requires Clang 3.5+)
- Support for multiple `-arch` flags with GCC
- Scripts to make sdk extraction possible on linux (xcode 4.2 only)
- OS X 10.10 support (DP1)
- pkg-config wrapper

### Changed

- Updated cctools to 855
- Updated ld64 to 236.3
- GCC 4.9.0 -> GCC 4.9.2

## v0.7

### Added

- New compiler wrapper written in C++
- `-oc-use-GCC-libs` option (uses `./build_GCC.sh` libstdc++)
- 10.6 SDK support (10.4-10.9 are all supported now)
- `sw_vers` tool, which is required by some projects
- `-stdlib=` support for GCC (as an extension through the wrapper)
- `OSXCROSS_C_STANDARD` env var (set C standard globally)
- `OSXCROSS_CXX_STANDARD` env var (set C++ standard globally)
- macOS support (as host)

### Changed

- `osxcross-conf`, `osxcross-env` and the (fake) `dsymutil` are now implemented in the wrapper
- `<arch>-apple-darwinXX-cc/c++` defaults now to Clang
- Switched to two-space indents
- GCC 4.8.2 -> GCC 4.9.0

### Fixed

- `MACOSX_DEPLOYMENT_TARGET` env var is now handled correctly

## v0.6

### Added

- NetBSD support
- Dependencies installer script (Don Bright)
- SDK packaging script

## v0.5

### Added

- libc++ support (for both, Clang and GCC)
- FreeBSD support

### Changed

- Attempt to make the toolchain less path dependent
- Updated cctools to 845

### Fixed

- Help Clang to find its own intrinsic headers (this issue appears to be fixed in Clang 3.4+)
- Building the toolchain on 32 bit systems

## v0.4

### Added

- GCC build script
- A workaround for buggy unistd.h headers

## v0.3

### Added

- 10.9 SDK support
- `*-apple-darwin*-clang*` symlinks (same as invoking `oXX-clang`)

### Changed

- No need to build extra cctools binaries for targeting i386, symlinking the x86_64 builds works as  
  well

## v0.2

### Added

- A check for broken unistd.h
- Comments to build.sh

### Changed

- Updated cctools to 839 with ld64-134.9

### Fixed

- Check if macOS package is present in tarballs

## v0.1

Initial release.

</details>
