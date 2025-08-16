
## macOS Cross-Toolchain for Linux and \*BSD

### OSXCross branches:

`master`:

* Uses `cctools 986` and `ld64 711`. Known to be stable and well-working.

`2.0-llvm-based`:

* OSXCross based on LLVM tools.  
  Easiest to build and recommended for new projects.  
  Does not support building for `i386`.

`testing`:

* Uses the latest versions of `cctools` and `ld64` — currently `cctools 1024.3` and `ld64 955.13`.  
  Takes the longest to build due to additional dependencies introduced in the latest versions.

`ppc-test`:

* An older branch with support for PPC targets.

---

### What is OSXCross

**OSXCross** provides a macOS cross-compilation toolchain.

### Supported OSes and architectures

- **Host OSes**: Linux, *BSD
- **Host architectures**: x86, x86\_64, ARM, AArch64/arm64
- **Target architectures**: arm64, arm64e, x86\_64, i386

---

### How It Works

macOS cross-compilation requires:

- Clang/LLVM (cross-compilation supported by default)
- A macOS SDK

This branch of OSXCross uses [cctools-port](https://github.com/tpoechtrager/cctools-port).
`cctools-port` provides toolchain tools such as `ar`, `lipo`, `otool`, and `ld64` as linker.

OSXCross provides a set of scripts for preparing the SDK, building `cctools-port` (along with its dependencies), and compiling the compiler wrapper.

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
clang cmake git patch python libssl-dev lzma-dev libxml2-dev xz bzip2 cpio libbz2 zlib1g-dev bash
```
_Optional:_

-   `llvm-dev`: For Link Time Optimization support and `ld64` `-bitcode_bundle` support
-   `uuid-dev`: For ld64  `-random_uuid` support

You can run `sudo tools/get_dependencies.sh` to get these (and the optional packages) automatically. (outdated)

#### Build Clang (Optional - if you need an up-to-date version of LLVM/Clang)

```sh
./build_clang.sh                           # Builds mainline Clang
./build_apple_clang.sh                     # Builds Apple's Clang
INSTALLPREFIX=/opt/clang ./build_clang.sh  # Custom install path
```

#### Build OSXCross

By default, this installs the osxcross toolchain into `<current-directory>/target`.  
To specify a different installation path or run the build unattended,  
set the `TARGET_DIR` and/or `UNATTENDED` environment variables accordingly.  
You can use `ENABLE_ARCHS` to restrict the build to a specific set of supported architectures  
(e.g. `"arm64 x86_64"`).

```sh
./build.sh 
[TARGET_DIR=/usr/local/osxcross] [OSX_VERSION_MIN=XX.X] [ENABLE_ARCHS="<ARCHS>"] [UNATTENDED=1] ./build.sh 
```

Add `<target>/bin` to your `PATH` after installation.

#### Build GCC (Optional)

```sh
./build_gcc.sh
GCC_VERSION=5.2.0 ENABLE_FORTRAN=1 ./build_gcc.sh
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

1. [Download Command Line Tools](https://developer.apple.com/download/all/?q=Command%20Line%20Tools%20for%20Xcode)
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

- i386: `i386-apple-darwinXX-clang++ test.cpp -O3 -o test` (if your SDK supports `i386`)
- x86_64: `x86_64-apple-darwinXX-clang++ test.cpp -O3 -o test`
- arm64: `arm64-apple-darwinXX-clang++ test.cpp -O3 -o test`
- arm64e: `arm64e-apple-darwinXX-clang++ test.cpp -O3 -o test`

Or by using xcrun:

- i386: `xcrun clang++ -arch x86_64 test.cpp -O3 -o test`
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

### License

- `scripts/wrapper`: GPLv2
- `cctools/ld64`: APSL 2.0 (legacy)
- `xar`: New BSD
