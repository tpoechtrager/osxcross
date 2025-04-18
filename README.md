## macOS Cross-Toolchain for Linux and \*BSD

### 2.0-llvm branch

**This branch is solely based on LLVM tooling**  
***(... besides the option to build a cctools-based lipo to improve compatibility)***

Please report any issues you encounter.

Crashes or bugs in LLVM tools (like `lipo`, etc.) should be reported to the [LLVM project](https://github.com/llvm/llvm-project).

This branch may be merged into master once the macOS LLVM tools are stable enough,  
fully replacing `cctools` and `ld64` with LLVM equivalents.

---

### What is OSXCross

**OSXCross** provides a macOS cross-compilation toolchain.

### Supported OSes and architectures

- **Host OSes**: Linux, *BSD
- **Host architectures**: x86, x86\_64, ARM, AArch64/arm64
- **Target architectures**: arm64, arm64e, x86\_64

This branch does not support `i386` as target, as `ld64.lld` does not - and likely never will - support it.

---

### How It Works

macOS cross-compilation requires:

- Clang/LLVM (cross-compilation supported by default)
- A macOS SDK

This branch of OSXCross is fully LLVM-based. All necessary tools (compiler, linker, binary utilities) are provided by LLVM.

OSXCross provides scripts for preparing the macOS SDK and setting up the compiler wrapper.

It also includes scripts for optionally building:

- Up-to-date LLVM tools and clang (`./build_clang.sh`, `./build_apple_clang.sh`)
- Vanilla GCC as a cross-compiler for target macOS (`./build_gcc.sh`
- The "compiler-rt" runtime library (`./build_compiler_rt.sh`)

---

### Package Manager

A minimal MacPorts package manager is included.
See [README.MACPORTS.md](README.MACPORTS.md).

---

### Installation

Place your [packaged SDK](https://github.com/tpoechtrager/osxcross#packaging-the-sdk) in the `tarballs/` directory.

Install the following dependencies:

```
clang, llvm, bash, patch, xz, bzip2
```

#### Build Clang (Optional - if you need an up-to-date version of LLVM/Clang)

```sh
./build_clang.sh           # Builds mainline Clang
./build_apple_clang.sh     # Builds Apple's Clang
INSTALLPREFIX=/opt/clang ./build_clang.sh  # Custom install path
```

#### Build OSXCross

By default, this installs the osxcross toolchain into `<current-directory>/target`.  
To specify a different installation path or run the build unattended,  
set the `TARGET_DIR` and/or `UNATTENDED` environment variables accordingly.

```sh
[TARGET_DIR=/usr/local/osxcross] [UNATTENDED=1] ./build.sh 
```

Add `<target>/bin` to your `PATH` after installation.

#### Build GCC (Optional)

```sh
./build_gcc.sh
[GCC_VERSION=14.2.0] [ENABLE_FORTRAN=1] ./build_gcc.sh
```

Install GCC dependencies:

```sh
sudo apt-get install gcc g++ zlib1g-dev libmpc-dev libmpfr-dev libgmp-dev
```

**Notes:**

- To enable `-Werror=implicit-function-declaration`, set `OSXCROSS_ENABLE_WERROR_IMPLICIT_FUNCTION_DECLARATION=1`
- To disable static linking: `OSXCROSS_GCC_NO_STATIC_RUNTIME=1`
- `*-g++-libc++` uses Clang's libc++ — only use if needed

---

### Packaging the SDK

**[Please ensure you have read and understood the Xcode license terms before continuing.](https://www.apple.com/legal/sla/docs/xcode.pdf)**

SDKs can be extracted either from the full Xcode or from the Xcode Command Line Tools.

### On macOS

**From Full Xcode**

1. [Download Xcode](https://developer.apple.com/download/all/?q=xcode)
2. Mount `Xcode.dmg` (Right-click → Open With → DiskImageMounter)
   - If you see a crossed-circle dialog when mounting, ignore it — installation of Xcode is not required
3. Run: `./tools/gen_sdk_package.sh` (from the OSXCross package)
4. Copy the resulting SDK (`*.tar.*` or `*.pkg`) to a USB stick
5. On Linux/BSD, move the SDK to the `tarballs/` directory of OSXCross

**From Command Line Tools**

1. [Download Command Line Tools](https://developer.apple.com/download/more)
2. Mount the `Command_Line_Tools_for_Xcode.dmg` (Open With → DiskImageMounter)
3. Install `Command Line Tools.pkg` (Open With → Installer)
4. Run: `./tools/gen_sdk_package_tools.sh`
5. Copy the resulting SDK (`*.tar.*` or `*.pkg`) to a USB stick
6. On Linux/BSD, move the SDK to the `tarballs/` directory of OSXCross

### On Linux (and others)

**Method 1 (Xcode > 8.0)**\
*Requires up to 45 GB free disk space. SSD strongly recommended.*

1. Download Xcode as described above
2. Install: `clang`, `make`, `libssl-devel`, `lzma-devel`, and `libxml2-devel`
3. Run: `./tools/gen_sdk_package_pbzx.sh <xcode>.xip`
4. Move the SDK to the `tarballs/` directory

**Method 2 (up to Xcode 7.3)**

1. Download Xcode as described above
2. Install: `cmake`, `libxml2-dev`, and `fuse`
3. Run: `./tools/gen_sdk_package_darling_dmg.sh <xcode>.dmg`
4. Move the SDK to the `tarballs/` directory

**Method 3 (up to Xcode 7.2)**

1. Download Xcode as described above
2. Ensure `clang` and `make` are installed
3. Run: `./tools/gen_sdk_package_p7zip.sh <xcode>.dmg`
4. Move the SDK to the `tarballs/` directory

**Method 4 (Xcode 4.2)**

1. Download Xcode 4.2 for Snow Leopard (ensure it's the correct version)
2. Install `dmg2img`
3. As root, run: `./tools/mount_xcode_image.sh /path/to/xcode.dmg`
4. Follow the on-screen instructions from the script
5. Move the SDK to the `tarballs/` directory

**From Xcode Command Line Tools**

1. Download Command Line Tools as described above
2. Install: `clang`, `make`, `libssl-devel`, `lzma-devel`, and `libxml2-devel`
3. Run: `./tools/gen_sdk_package_tools_dmg.sh <command_line_tools_for_xcode>.dmg`
4. Move the SDK to the `tarballs/` directory

---

### Usage Examples

#### Compile test.cpp

- x86_64: `x86_64-apple-darwinXX-clang++ test.cpp -O3 -o test`
- arm64: `arm64-apple-darwinXX-clang++ test.cpp -O3 -o test`
- arm64e: `arm64e-apple-darwinXX-clang++ test.cpp -O3 -o test`

Or by using xcrun:

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

- SDK <= 10.13: macOS 10.6
- SDK >= 10.14: macOS 10.9

Override via:

1. `OSX_VERSION_MIN=10.x ./build.sh`
2. `-mmacosx-version-min=10.x` to compiler
3. `MACOSX_DEPLOYMENT_TARGET` env var

Note: >= 10.9 defaults to `libc++`, override via `-stdlib=libstdc++`

---

### Projects Using OSXCross

- [multiarch/crossbuild](https://github.com/multiarch/crossbuild):  
  various cross-compilers  
  (**Systems**: Linux, macOS, Windows, **Archs**: x86\_64,i386, arm, ppc, mips)  
  in Docker. OSXCross powers the Darwin builds.
- [Smartmontools](https://www.smartmontools.org)

---

### License

- `scripts/wrapper`: GPLv2
- `cctools/ld64`: APSL 2.0 (legacy)
- `xar`: New BSD
