## OS X Cross toolchain for Linux, *BSD and Cygwin ##

### WHAT IS THE GOAL OF OSXCROSS? ###

The goal of OSXCross is to provide a well working OS X cross toolchain for Linux, *BSD and Cygwin.

### HOW DOES IT WORK? ###

[Clang/LLVM is a cross compiler by default](http://clang.llvm.org/docs/CrossCompilation.html)
and is now available on nearly every Linux distribution,  
so we just need a proper
[port](https://github.com/tpoechtrager/cctools-port)
of the [cctools](http://www.opensource.apple.com/tarballs/cctools) (ld, lipo, ...) and the OS X SDK.

If you want, then you can build an up-to-date vanilla GCC as well.

### WHAT CAN I BUILD WITH IT? ###

Basically everything you can build on OS X with clang/gcc should build with this cross toolchain as well.

### PACKET MANAGERS ###

OSXCross comes with a minimalistic MacPorts Packet Manager.  
Please see [README.MACPORTS](README.MACPORTS.md) for more.

### INSTALLATION: ###

*Windows/Cygwin users should follow [README.CYGWIN](README.CYGWIN.md).*

Move your [packaged SDK](https://github.com/tpoechtrager/osxcross#packaging-the-sdk) to the tarballs/ directory.

Then ensure you have the following installed on your system:

`Clang 3.2+`, `patch`, `libxml2-devel` (<=10.6 only) and the `bash shell`.

Optional:

`llvm-devel`: For Link Time Optimization support  
`uuid-devel`: For ld64 `-random_uuid` support

\--  
You can run 'sudo tools/get\_dependencies.sh' to get these automatically.  

'[INSTALLPREFIX=...] ./build_clang.sh' can be used to build a recent clang version  
from source (requires gcc and g++).

On debian like systems you can also use [llvm.org/apt](http://llvm.org/apt) to get a newer version of clang.  
But be careful, that repository is known to cause [troubles](https://github.com/tpoechtrager/osxcross/issues/16).  
\--

Then run `[UNATTENDED=1] ./build.sh` to build the cross toolchain.  
(It will search 'tarballs' for your SDK and then build in its own directory.)

**Do not forget** to add `<path>/target/bin` to your PATH variable.

That's it. See usage examples below.

##### Building GCC: #####

If you want to build GCC as well, then you can do this by running:  
`[GCC_VERSION=5.2.0] [ENABLE_FORTRAN=1] ./build_gcc.sh`.  

\[A gfortran usage example can be found [here](https://github.com/tpoechtrager/osxcross/issues/28#issuecomment-67047134)]

But before you do this, make sure you have got the GCC build depedencies installed on your system.  

On debian like systems you can run:

`[sudo] apt-get install gcc g++ zlib1g-dev libmpc-dev libmpfr-dev libgmp-dev`  

to install them.

ATTENTION:

OSXCross links libgcc and libstdc++ statically by default (this affects `-foc-use-gcc-libstdc++` too).  
You can turn this behavior off with `OSXCROSS_GCC_NO_STATIC_RUNTIME=1` (env).

### PACKAGING THE SDK: ###

**[Please ensure you have read and understood the Xcode license
   terms before continuing.](https://www.apple.com/legal/sla/docs/xcode.pdf)**

##### Packaging the SDK on Mac OS X: #####

1. [Download [Xcode](https://developer.apple.com/downloads/index.action?name=Xcode%205.1.1) \*\*]
2. [Mount Xcode.dmg (Open With -> DiskImageMounter) \*\*\*]
3. Run: `./tools/gen_sdk_package.sh` (from the OSXCross package)
4. Copy the packaged SDK (\*.tar.\* or \*.pkg) on a USB Stick
5. (On Linux/BSD) Copy or move the SDK into the tarballs/ directory of OSXCross

\*\* Xcode up to 6.3.x is known to work; 7.x is not working (yet).  
\*\*\* If you get a dialog with a crossed circle, ignore it, you don't need to install Xcode.

Step 1. and 2. can be skipped if you have Xcode installed.

##### Packing the SDK on Linux, Cygwin (and others), Method 1 (works with Xcode >= 4.3): #####

1. Download Xcode like described in 'Packaging the SDK on Mac OS X'
2. Ensure you have `clang` and `make` installed
3. Run `./gen_sdk_package_p7zip.sh <xcode>.dmg`
4. Copy or move the SDK into the tarballs/ directory

##### Packing the SDK on Linux, Method 2 (works with Xcode >= 4.3): #####

1. Download Xcode like described in 'Packaging the SDK on Mac OS X'
2. Install `cmake`, `libxml2-dev` and `fuse`
3. Run `./gen_sdk_package_darling_dmg.sh <xcode>.dmg`
4. Copy or move the SDK into the tarballs/ directory

##### Packing the SDK on Linux, Method 3 (does *NOT* work with Xcode 4.3 or later!): #####

1. Download Xcode 4.2 for Snow Leopard
2. Ensure you are downloading the "Snow Leopard" version
3. Install `dmg2img`
4. Run (as root): `./tools/mount_xcode_image.sh /path/to/xcode.dmg`
5. Follow the instructions printed by `./tools/mount_xcode_image.sh`
6. Copy or move the SDK into the tarballs/ directory


### USAGE EXAMPLES: ###

##### Let's say you want to compile a file called test.cpp, then you can do this by running: #####

* Clang:

  * 32 bit:  `o32-clang++ test.cpp -O3 -o test`   OR   `i386-apple-darwinXX-clang++ test.cpp -O3 -o test`
  * 64 bit:  `o64-clang++ test.cpp -O3 -o test`   OR   `x86_64-apple-darwinXX-clang++ test.cpp -O3 -o test`

* GCC:

  * 32 bit:  `o32-g++ test.cpp -O3 -o test`  OR   `i386-apple-darwinXX-g++ test.cpp -O3 -o test`
  * 64 bit:  `o64-g++ test.cpp -O3 -o test`   OR   `x86_64-apple-darwinXX-g++ test.cpp -O3 -o test`

XX= the target version, you can find it out by running  `osxcross-conf`  and then see `TARGET`.

You can use the shortcut `o32-...` or `i386-apple-darwin...` what ever you like more.

*I'll continue from now on with `o32-clang`, but remember,
 you can simply replace it with `o32-gcc` or `i386-apple-darwin...`.*

##### Building Makefile based projects: #####

  * `make CC=o32-clang CXX=o32-clang++`

##### Building automake based projects: #####

  * `CC=o32-clang CXX=o32-clang++ ./configure --host=i386-apple-darwinXX`

##### Building test.cpp with libc++: #####

Note: libc++ requires Mac OS X 10.7 or newer! If you really need C++11 for  
an older OS X version, then you can do the following:

1. Build GCC so you have an up-to-date libstdc++
2. Build your source code with GCC or `clang++-gstdc++` / `clang++ -foc-use-gcc-libstdc++`

Usage Examples:

* Clang:

  * C++98: `o32-clang++ -stdlib=libc++ test.cpp -o test`
  * C++11: `o32-clang++ -stdlib=libc++ -std=c++11 test1.cpp -o test`
  * C++14: `o32-clang++ -stdlib=libc++ -std=c++14 test1.cpp -o test`
  * C++1z: `o32-clang++ -stdlib=libc++ -std=c++1z test1.cpp -o test`

* Clang (shortcut):

  * C++98: `o32-clang++-libc++ test.cpp -o test`
  * C++11: `o32-clang++-libc++ -std=c++11 test.cpp -o test`
  * C++14: `o32-clang++-libc++ -std=c++14 test.cpp  -o test`
  * C++1z: `o32-clang++-libc++ -std=c++1z test.cpp  -o test`

* GCC

  * C++11: `o32-g++-libc++ -std=c++11 test.cpp`
  * C++14: `o32-g++-libc++ -std=c++14 test.cpp -o test`
  * C++1z: `o32-g++-libc++ -std=c++1z test.cpp -o test`

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
  * use lipo to generate the universal binary: `x86_64-apple darwinXX-lipo -create test.i386 test.x86_64 -output test`

### DEPLOYMENT TARGET: ###

The default deployment target is `Mac OS X 10.5`.

However, there are several ways to override the default value:

1. by passing `OSX_VERSION_MIN=10.x` to `./build.sh`
2. by passing `-mmacosx-version-min=10.x` to the compiler
3. by setting the `MACOSX_DEPLOYMENT_TARGET` environment variable

\>= 10.9 also defaults to `libc++` instead of `libstdc++`, this behavior  
can be overriden by explicitly passing `-stdlib=libstdc++` to clang.

x86\_64h defaults to `Mac OS X 10.8` and requires clang 3.5+.  
x86\_64h = x86\_64 with optimizations for the Intel Haswell Architecture.

### BUILDING OSXCROSS WITH GCC: ###

You can build OSXCross with GCC this way:

`CC=gcc CXX=g++ ./build.sh`

You will need gcc/g++/gcc-objc 4.6+.

### LICENSE: ####
  * scripts/wrapper: GPLv2
  * cctools/ld64: APSL 2.0
  * xar: New BSD

### CREDITS: ####
 * [cjacker for the initial cctools port](https://code.google.com/p/ios-toolchain-based-on-clang-for-linux/source/browse/#svn%2Ftrunk%2Fcctools-porting%2Fpatches)
