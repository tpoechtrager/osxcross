
## macOS Cross toolchain for Linux and *BSD ##

### WHAT IS THE GOAL OF OSXCROSS? ###

The goal of OSXCross is to provide a well working macOS cross toolchain for  
`Linux`, `FreeBSD`, `OpenBSD`, and `Android (Termux)`.

OSXCross works **on** `x86`, `x86_64`, `arm` and `AArch64`/`arm64`,  
and is able to **target** `arm64`, `arm64e`, `x86_64`, `x86_64h` and `i386`.

`arm64` requires macOS 11.0 SDK (or later).  
`arm64e` [requires a recent Apple clang compiler.](https://github.com/apple/llvm-project)

[There is also a `ppc` test branch that has recently seen some daylight.](https://github.com/tpoechtrager/osxcross/blob/ppc-test/README.PPC-GCC-5.5.0-SDK-10.5.md)

### HOW DOES IT WORK? ###

For cross-compiling for macOS you need
* the Clang/LLVM compiler
* the cctools (lipo, otool, nm, ar, ...) and ld64
* the macOS SDK.

[Clang/LLVM is a cross compiler by default](http://clang.llvm.org/docs/CrossCompilation.html)
and is now available on nearly every Linux distribution, so we just
need a proper [port](https://github.com/tpoechtrager/cctools-port) of
the cctools/ld64 and the macOS SDK.

OSXCross includes a collection of scripts for preparing the SDK and
building the cctools/ld64.

It also includes scripts for optionally building
* Clang using gcc (for the case your distribution does not include it),
* an up-to-date vanilla GCC as a cross-compiler for target macOS,
* the "compiler-rt" runtime library, and
* the `llvm-dsymutil` tool required for debugging.

Note: The "compiler-rt" library can be needed to link code that uses the
`__builtin_available()` runtime version check.


### WHAT CAN BE BUILT WITH IT? ###

Basically everything you can build on macOS with clang/gcc should build with
this cross toolchain as well.

### PACKET MANAGERS ###

OSXCross comes with a minimalistic MacPorts Packet Manager.
See [README.MACPORTS](README.MACPORTS.md) for more.

### INSTALLATION: ###

Move your
[packaged SDK](https://github.com/tpoechtrager/osxcross#packaging-the-sdk)
to the tarballs/ directory.

Then ensure you have the following installed on your system:

`Clang 3.9+`, `cmake`, `git`, `patch`, `Python`, `libssl-dev` (openssl)
`lzma-dev`, `libxml2-dev`, `xz`, `bzip2`, `cpio`, `libbz2`, `zlib1g-dev`
and the `bash shell`.

You can run 'sudo tools/get\_dependencies.sh' to get these (and the
optional packages) automatically. (outdated)

*Optional:*

- `llvm-devel`: For Link Time Optimization support
- `llvm-devel`: For ld64 `-bitcode_bundle` support
- `uuid-devel`: For ld64 `-random_uuid` support

On Ubuntu trusty you must upgrade CMake to >= 3.2.3 first. Or do this:

```shell
    curl -sSL https://cmake.org/files/v3.14/cmake-3.14.5-Linux-x86_64.tar.gz | sudo tar -xzC /opt
    export PATH=/opt/cmake-3.14.5-Linux-x86_64/bin:$PATH
```

##### Building Clang #####

OSXCross uses `clang` as the default compiler for building its tools, and also
as a cross-compiler to create macOS binaries.

In `clang` there is no difference between cross-compilation and native
compilation, so OSXCross can use a normal `clang` install for both.  You can
use either a `clang` installation you already have, or build your own from
source.

To build and install your own `clang` from a recent source tree, using `gcc`,
run:

```shell
    ./build_clang.sh
```

This installs `clang` into `/usr/local`.  If you want to install somewhere
else, set the `INSTALLPREFIX` variable.  For example:

```shell
    INSTALLPREFIX=/opt/clang ./build_clang.sh
```

##### Building OSXCross #####

To build the cross toolchain (using `clang`), run:

```shell
    ./build.sh
```

This installs the osxcross toolchain into `<path>/target`. If you want to install somewhere
else, set the `TARGET_DIR` variable.  For example:

```shell
    TARGET_DIR=/usr/local/osxcross ./build.sh
```

And/Or, set variable `UNATTENDED` to `1` to skip the prompt and proceed straight to
the build:

```shell
    UNATTENDED=1 ./build.sh
```

(This will search 'tarballs' for your SDK and then build in its own directory.)

**Once this is done:** add `<path>/target/bin` to your PATH variable so that
you can invoke the cross-compiler.

That's it. See usage examples below.

##### Building GCC: #####

If you also want to build GCC as a cross-compiler, you can do that by running:

```shell
    ./build_gcc.sh
```

The script lets you select a GCC version by setting the variable `GCC_VERSION`.
 By default you get C and C++ compilers, but you can tell the script to build a
Fortran compiler as well:

```shell
    GCC_VERSION=5.2.0 ENABLE_FORTRAN=1 ./build_gcc.sh
```

\[A gfortran usage example can be found [here](https://github.com/tpoechtrager/osxcross/issues/28#issuecomment-67047134)]

Before you do this, make sure you have the GCC build depedencies installed on
your system.

On debian like systems you can install these using:

```shell
    sudo apt-get install gcc g++ zlib1g-dev libmpc-dev libmpfr-dev libgmp-dev
```

ATTENTION:

OSXCross does not enable `-Werror=implicit-function-declaration` by default.  
You can emulate Xcode 12's behavior by setting the environmental variable  
`OSXCROSS_ENABLE_WERROR_IMPLICIT_FUNCTION_DECLARATION` to 1.

OSXCross links libgcc and libstdc++ statically by default (this affects
`-foc-use-gcc-libstdc++` too).  You can turn this behavior off with
`OSXCROSS_GCC_NO_STATIC_RUNTIME=1` (env).

The build also creates aliases `*-g++-libc++` which link with the `clang`
implementation of the C++ standard library instead of the GCC version.  Don't
use these variants unless you know what you're doing.

### PACKAGING THE SDK: ###

**[Please ensure you have read and understood the Xcode license
   terms before continuing.](https://www.apple.com/legal/sla/docs/xcode.pdf)**

The SDKs can be extracted either from full Xcode or from Command Line
Tools for Xcode.

##### Packaging the SDK on recent macOS (Xcode): #####

1. [Download Xcode: https://developer.apple.com/download/all/?q=xcode] \*\*
2. [Mount Xcode.dmg (Open With -> DiskImageMounter) \*\*\*]
3. Run: `./tools/gen_sdk_package.sh` (from the OSXCross package)
4. Copy the packaged SDK (\*.tar.\* or \*.pkg) on a USB Stick
5. (On Linux/BSD) Copy or move the SDK into the tarballs/ directory of
   OSXCross.

\*\*  
-- Xcode up to 15 Beta 6 is known to work.  
-- Use Firefox if you have problems signing in.

\*\*\*  
-- If you get a dialog with a crossed circle, ignore it.  
-- You don't need to install Xcode.

Step 1. and 2. can be skipped if you have Xcode installed.

##### Packaging the Xcode 4.2 SDK on Snow Leopard: #####
1. Install a recent version of Bash from MacPorts or Tigerbrew
2. Download Xcode 4.2 for Snow Leopard
3. Mount the disk image with DiskImageMounter or by running
  `hdiutil attach <xcode>.dmg`
4. Run: `XCODEDIR=/Volumes/Xcode ./tools/gen_sdk_package.sh`
5. (On Linux/BSD) Copy or move the SDK into the tarballs/ directory of
   OSXCross.

##### Packing the SDK on Linux - Method 1 (Xcode > 8.0): #####

This method may require up to 45 GB of free disk space.  
An SSD is recommended for this method.

1. Download Xcode like described in 'Packaging the SDK on macOS'
2. Install `clang`, `make`, `libssl-devel`, `lzma-devel` and `libxml2-devel`
3. Run `./tools/gen_sdk_package_pbzx.sh <xcode>.xip`
4. Copy or move the SDK into the tarballs/ directory

##### Packing the SDK on Linux - Method 2 (works up to Xcode 7.3): #####

1. Download Xcode like described in 'Packaging the SDK on macOS'
2. Install `cmake`, `libxml2-dev` and `fuse`
3. Run `./tools/gen_sdk_package_darling_dmg.sh <xcode>.dmg`
4. Copy or move the SDK into the tarballs/ directory

##### Packing the SDK on Linux (and others) - Method 3 (works up to Xcode 7.2): #####

1. Download Xcode like described in 'Packaging the SDK on macOS'
2. Ensure you have `clang` and `make` installed
3. Run `./tools/gen_sdk_package_p7zip.sh <xcode>.dmg`
4. Copy or move the SDK into the tarballs/ directory

##### Packing the SDK on Linux - Method 4 (works up to Xcode 4.2): #####

1. Download Xcode 4.2 for Snow Leopard
2. Ensure you are downloading the "Snow Leopard" version
3. Install `dmg2img`
4. Run (as root): `./tools/mount_xcode_image.sh /path/to/xcode.dmg`
5. Follow the instructions printed by `./tools/mount_xcode_image.sh`
6. Copy or move the SDK into the tarballs/ directory


##### Packaging the SDK from Xcode Command Line Tools on macOS: #####

1. [Download Xcode Command Line Tools: https://developer.apple.com/download/more] \*\*\*\*
2. [Mount Command_Line_Tools_for_Xcode.dmg (Open With -> DiskImageMounter)]
3. [Install "Command Line Tools.pkg" (Open With -> Installer)]
3. Run: `./tools/gen_sdk_package_tools.sh` (from the OSXCross package)
4. Copy the packaged SDK (\*.tar.\* or \*.pkg) on a USB Stick
5. (On Linux/BSD) Copy or move the SDK into the tarballs/ directory of
   OSXCross.

\*\*\*\*
-- Xcode command line tools 12.x are known to work.

Steps 1. to 3. can be skipped if you have Xcode Command line tools
already installed (e.g., auto-installed by running `git` or `gcc`
command from command-line).

##### Packing the SDK from from Xcode Command Line Tools on Linux: #####

This method may require up to 45 GB of free disk space.
An SSD is recommended for this method.

1. Download Xcode Command Line Tools like described in 'Packaging the SDK from Xcode Command Line Tools on macOS'
2. Install `clang`, `make`, `libssl-devel`, `lzma-devel` and `libxml2-devel`
3. Run `./tools/gen_sdk_package_tools_dmg.sh <command_line_tools_for_xcode>.dmg`
4. Copy or move the SDK into the tarballs/ directory


### USAGE EXAMPLES: ###

##### Example.  To compile a file called test.cpp, you can run: #####

##### x86 #####

* Clang:

  * 32 bit: `o32-clang++ test.cpp -O3 -o test` OR
    `i386-apple-darwinXX-clang++ test.cpp -O3 -o test`
  * 64 bit: `o64-clang++ test.cpp -O3 -o test` OR
    `x86_64-apple-darwinXX-clang++ test.cpp -O3 -o test`

* GCC:

  * 32 bit:  `o32-g++ test.cpp -O3 -o test` OR
    `i386-apple-darwinXX-g++ test.cpp -O3 -o test`
  * 64 bit:  `o64-g++ test.cpp -O3 -o test` OR
    `x86_64-apple-darwinXX-g++ test.cpp -O3 -o test`

##### ARM #####

* Clang:

  * arm64: `oa64-clang++ test.cpp -O3 -o test` OR
    `arm64-apple-darwinXX-clang++ test.cpp -O3 -o test`
  * arm64e: `oa64e-clang++ test.cpp -O3 -o test` OR
    `arm64e-apple-darwinXX-clang++ test.cpp -O3 -o test`


XX= the target version, you can find it out by running  `osxcross-conf` and
then see `TARGET`.

You can use the shortcuts `o32-...` for `i386-apple-darwin...`, depending on
which you prefer.

*I'll continue from here on with `o32-clang`, but remember,
 you can simply replace it with `o32-gcc` or `i386-apple-darwin...`.*

##### Building Makefile based projects: #####

  * `make CC=o32-clang CXX=o32-clang++`

##### Building automake based projects: #####

  * `CC=o32-clang CXX=o32-clang++ ./configure --host=i386-apple-darwinXX`

##### Building test.cpp with libc++: #####

Note: libc++ requires macOS 10.7 or later! If you really need C++11 for
an older macOS version, then you can do the following:

1. Build GCC so you have an up-to-date libstdc++
2. Build your source code with GCC or
   `clang++-gstdc++` / `clang++ -foc-use-gcc-libstdc++`

Usage Examples:

* Clang:

  * C++98: `o32-clang++ -stdlib=libc++ -std=c++98 test.cpp -o test`
  * C++11: `o32-clang++ -stdlib=libc++ -std=c++11 test1.cpp -o test`
  * C++14: `o32-clang++ -stdlib=libc++ -std=c++14 test1.cpp -o test`
  * C++17: `o32-clang++ -stdlib=libc++ -std=c++17 test1.cpp -o test`
  * C++2a: `o32-clang++ -stdlib=libc++ -std=c++20 test1.cpp -o test`

* Clang (shortcut):

  * C++98: `o32-clang++-libc++ -std=c++98 test.cpp -o test`
  * C++11: `o32-clang++-libc++ -std=c++11 test.cpp -o test`
  * C++14: `o32-clang++-libc++ -std=c++14 test.cpp -o test`
  * C++17: `o32-clang++-libc++ -std=c++17 test.cpp -o test`
  * C++2a: `o32-clang++-libc++ -std=c++20 test.cpp -o test`

* GCC

  * C++11: `o32-g++-libc++ -std=c++11 test.cpp`
  * C++14: `o32-g++-libc++ -std=c++14 test.cpp -o test`
  * C++17: `o32-g++-libc++ -std=c++17 test.cpp -o test`
  * C++2a: `o32-g++-libc++ -std=c++20 test.cpp -o test`

##### Building test1.cpp and test2.cpp with LTO (Link Time Optimization): #####

  * build the first object file: `o32-clang++ test1.cpp -O3 -flto -c`
  * build the second object file: `o32-clang++ test2.cpp -O3 -flto -c`
  * link them with LTO: `o32-clang++ -O3 -flto test1.o test2.o -o test`

##### Building a universal binary: #####

* Clang:
  * `o64-clang++ test.cpp -O3 -arch i386 -arch x86_64 -o test`
* GCC:
  * build the 32 bit binary: `o32-g++ test.cpp -O3 -o test.i386`
  * build the 64 bit binary: `o64-g++ test.cpp -O3 -o test.x86_64`
  * use lipo to generate the universal binary:
    `x86_64-apple darwinXX-lipo -create test.i386 test.x86_64 -output test`

### DEPLOYMENT TARGET: ###

The default deployment target is:  

SDK <= 10.13: `macOS 10.6`  
SDK >= 10.14: `macOS 10.9`

However, there are several ways to override the default value:

1. by passing `OSX_VERSION_MIN=10.x` to `./build.sh`
2. by passing `-mmacosx-version-min=10.x` to the compiler
3. by setting the `MACOSX_DEPLOYMENT_TARGET` environment variable

\>= 10.9 also defaults to `libc++` instead of `libstdc++`,  
this behavior can be overriden by explicitly passing `-stdlib=libstdc++` to clang.

x86\_64h defaults to `macOS 10.8` and requires clang 3.5+.  
x86\_64h = x86\_64 with optimizations for the Intel Haswell Architecture.

### PROJECTS USING OSXCROSS: ###

* [multiarch/crossbuild](https://github.com/multiarch/crossbuild):  
  various cross-compilers  
  (**Systems**: Linux, macOS, Windows, **Archs**: x86\_64,i386, arm, ppc, mips)  
  in Docker. OSXCross powers the Darwin builds.
* [Smartmontools](https://www.smartmontools.org)

### LICENSE: ####
  * scripts/wrapper: GPLv2
  * cctools/ld64: APSL 2.0
  * xar: New BSD

### CREDITS: ####
 * [cjacker for the initial cctools port](https://code.google.com/p/ios-toolchain-based-on-clang-for-linux/source/browse/#svn%2Ftrunk%2Fcctools-porting%2Fpatches)
