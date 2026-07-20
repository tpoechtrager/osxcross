
## macOS Cross-Toolchain for Linux and \*BSD

### OSXCross build flavors

OSXCross provides three build flavors:

`stable` (default):

* Uses `cctools 986` and `ld64 711`.
* Known to be stable and well-working.

`latest`:

* Uses `cctools 1030.6.3` and `ld64 956.6`.
* Takes the longest to build because the newer tool versions require additional dependencies.

`llvm`:

* Uses LLVM tools and `ld64.lld` instead of cctools and `ld64`.
* Is the easiest flavor to build and is recommended for new projects.
* Supports `arm64`, `arm64e`, and `x86_64` targets. It does not support `i386` or `x86_64h`.
* Can optionally use `cctools lipo` instead of `llvm-lipo` for improved compatibility.

---

### What is OSXCross

**OSXCross** provides a macOS cross-compilation toolchain.

### Supported OSes and architectures

- **Host OSes**: Linux, *BSD
- **Host architectures**: x86, x86\_64, ARM, AArch64/arm64
- **Target architectures**: arm64, arm64e, x86\_64, i386

---

### How It Works

OSXCross combines a host compiler, a packaged macOS SDK, Darwin-compatible
binary utilities, and a compiler wrapper into a cross-compilation toolchain.

The selected build flavor determines which binary utilities and linker are used:

- The `stable` and `latest` flavors build
  [cctools-port](https://github.com/tpoechtrager/cctools-port), including tools
  such as `ar`, `lipo`, and `otool`, together with the `ld64` linker.
- The `llvm` flavor uses the corresponding LLVM tools and `ld64.lld`. It can
  optionally build `cctools lipo` as a compatibility replacement for `llvm-lipo`.

During `./build.sh`, the SDK is installed into the target directory, the tools
and dependencies required by the selected flavor are prepared, and the OSXCross
wrapper is compiled. The installed target-prefixed commands configure the target
triple, SDK sysroot, deployment target, and linker for macOS automatically.

Additional scripts can build optional compiler and runtime components:

- [Current upstream LLVM/Clang and LLD](README.BUILD-CLANG.md)
  (`./build_clang.sh`)
- [Apple Clang](README.BUILD-CLANG.md)
  (`./build_apple_clang.sh`)
- [Vanilla GCC for x86_64, with i386 multilib where supported](README.BUILD-GCC.md)
  (`./build_gcc.sh`)
- [An experimental Darwin GCC fork for ARM64 and x86_64](README.BUILD-GCC.md)
  (`./build_gcc_with_arm64_support.sh`)
- The [compiler-rt runtime library](README.COMPILER-RT.md) (`./build_compiler_rt.sh`)

---

### Package Manager

A minimal MacPorts package manager is included.
See [README.MACPORTS.md](README.MACPORTS.md).

---

### CMake

For CMake projects, OSXCross provides architecture- and compiler-specific 
launchers together with an installed toolchain file.  
See [README.CMAKE.md](README.CMAKE.md) for setup, compiler selection, universal
binaries, package discovery, and complete examples.

---

### Installation

#### Prerequisites

1. [Generate the SDK](README.SDK.md) and place the resulting archive in the `tarballs/` directory.

2. Install the OSXCross build dependencies:

   See [README.BUILD-DEPENDENCIES.md](README.BUILD-DEPENDENCIES.md) for required and optional dependencies,  
   Debian/Ubuntu installation commands, and the systems supported by `tools/get_dependencies.sh`.

3. Optionally, build a recent version of Clang and LLD:

   See [README.BUILD-CLANG.md](README.BUILD-CLANG.md) for dependencies and instructions for building  
   upstream LLVM/Clang or Apple Clang.

#### Build OSXCross

By default, the OSXCross toolchain is installed in `<current-directory>/target`.

`./build.sh` prompts you to select a build flavor. Press Enter to use the default `stable` flavor.  
Set `BUILD_FLAVOR` to `stable`, `latest`, or `llvm` to select a flavor without prompting.

When `UNATTENDED=1` is set, the specified `BUILD_FLAVOR` is used.  
If no flavor is specified, `stable` is selected automatically.

Use `TARGET_DIR` to specify a different installation directory.  
Use `ENABLE_ARCHS` to restrict the build to a supported set of architectures, for example `"arm64 x86_64"`.

```sh
./build.sh
BUILD_FLAVOR=llvm ./build.sh
UNATTENDED=1 BUILD_FLAVOR=latest ./build.sh
TARGET_DIR=/usr/local/osxcross OSX_VERSION_MIN=XX.X ENABLE_ARCHS="<ARCHS>" ./build.sh
```

Add `<target>/bin` to your `PATH` after installation.

#### Build GCC (Optional)

See [README.BUILD-GCC.md](README.BUILD-GCC.md) for dependencies and instructions for  
building the ARM64 Darwin GCC fork or vanilla GCC.

---

### Packaging the SDK

SDKs can be extracted either from the full Xcode or from the Xcode Command Line Tools.  
See [README.SDK.md](README.SDK.md) for step-by-step instructions  
on macOS and Linux.

---

### Usage Examples

#### Compile test.cpp

- i386: `i386-apple-darwinXX-clang++ test.cpp -O3 -o test` (if your SDK supports `i386`)
- x86_64: `x86_64-apple-darwinXX-clang++ test.cpp -O3 -o test`
- arm64: `arm64-apple-darwinXX-clang++ test.cpp -O3 -o test`
- arm64e: `arm64e-apple-darwinXX-clang++ test.cpp -O3 -o test`

Or by using xcrun:

- i386: `xcrun clang++ -arch i386 test.cpp -O3 -o test`
- x86_64: `xcrun clang++ -arch x86_64 test.cpp -O3 -o test`
- arm64: `xcrun clang++ -arch arm64 test.cpp -O3 -o test`
- arm64e: `xcrun clang++ -arch arm64e test.cpp -O3 -o test`

**GCC:** Replace `clang++` with `g++` as needed

Check your target version with `osxcross-conf`, see `TARGET`.

#### Build Makefile project

`xcrun -f clang` prints the path to clang.

```sh
make CC=$(xcrun -f clang) CXX=$(xcrun -f clang++)
```

#### Build autotools project

```sh
CC=x86_64-apple-darwinXX-clang CXX=x86_64-apple-darwinXX-clang++ ./configure --host=x86_64-apple-darwinXX
```

#### libc++ Example (macOS 10.7+ required)

```sh
xcrun clang++ -stdlib=libc++ -std=c++11 test.cpp -o test
```

Shortcut:

```sh
x86_64-apple-darwinXX-clang++-libc++ -std=c++11 test.cpp -o test
```

#### LTO Example

```sh
xcrun clang++ test1.cpp -O3 -flto -c
xcrun clang++ test2.cpp -O3 -flto -c
xcrun clang++ -O3 -flto test1.o test2.o -o test
```

#### Universal Binary

```sh
xcrun clang++ test.cpp -O3 -arch x86_64 -arch arm64 -o test
```

GCC:

```sh
xcrun g++ -arch x86_64 test.cpp -O3 -o test.x86_64
xcrun g++ -arch arm64 test.cpp -O3 -o test.arm64
xcrun lipo -create test.x86_64 test.arm64 -output test
```

---

### Deployment Target

Default:
* SDK ≤ 10.13 → macOS 10.6
* SDK ≥ 10.14 → macOS 10.9
* SDK ≥ 14.0 → macOS 10.13

Can be overriden via:

1. During the build: `OSX_VERSION_MIN=XX.X ./build.sh`.
2. By passing `-mmacos-version-min=XX.X` to the compiler.
3. By setting `MACOSX_DEPLOYMENT_TARGET=XX.X` env var.

Note: Deployment target ≥ 10.9 defaults to `libc++`.  
Can be explicitely overriden by setting the C++ library to `libstdc++` via `-stdlib=libstdc++`.

---

### Projects Using OSXCross

- [multiarch/crossbuild](https://github.com/multiarch/crossbuild):  
  various cross-compilers  
  (**Systems**: Linux, macOS, Windows, **Archs**: x86\_64,i386, arm, ppc, mips)  
  in Docker. OSXCross powers the Darwin builds.
- [Smartmontools](https://www.smartmontools.org)

---

### Legacy branches

The `ppc-test` branch is an older OSXCross branch with support for PowerPC targets.

---

### License

- `scripts/wrapper`: GPLv2
- `cctools/ld64`: APSL 2.0 (legacy)
- `xar`: New BSD
