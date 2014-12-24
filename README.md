## OS X Cross toolchain for Linux, FreeBSD and NetBSD ##

### WHAT IS THE GOAL OF OSXCROSS? ###

The goal of OSXCross is to provide a well working OS X cross toolchain for Linux, FreeBSD and NetBSD.

### HOW DOES IT WORK? ###

[Clang/LLVM is a cross compiler by default](http://clang.llvm.org/docs/CrossCompilation.html)
and is now available on nearly every Linux distribution,  
so we just need a proper
[port](https://github.com/tpoechtrager/cctools-port)
of the [cctools](http://www.opensource.apple.com/tarballs/cctools) (ld, lipo, ...) and the OS X SDK.

If you want, then you can build an up-to-date vanilla GCC as well.

### WHAT CAN I BUILD WITH IT? ###

Basically everything you can build on OS X with clang/gcc should build with this cross toolchain as well.

### INSTALLATION: ###

Move your packaged SDK to the tarballs/ directory.

Then ensure you have the following installed on your Linux/BSD box:

`Clang 3.2+`, `llvm-devel`, `patch`, `libxml2-devel` (<=10.5 only),  
`uuid-devel`,   `openssl-devel` and the `bash shell`.

\--  
You can run 'sudo tools/get\_dependencies.sh' to get these automatically.  

'[INSTALLPREFIX=...] ./build_clang.sh' can be used to build a recent clang version  
from source (requires gcc and g++).

On debian like systems you can also use [llvm.org/apt](http://llvm.org/apt) to get a newer version of clang.  
But be careful, that repository is known to cause [troubles](https://github.com/tpoechtrager/osxcross/issues/16).  
\--

Then run `./build.sh` to build the cross toolchain.  
(It will search 'tarballs' for your SDK and then build in its own directory.)

**Don't forget** to add the printed `` `<path>/osxcross-env` `` to your `~/.profile` or `~/.bashrc`.  
Then either run `source ~/.profile` or restart your shell session.

That's it. See usage examples below.

##### Building GCC: #####

If you want to build GCC as well, then you can do this by running:  
`[GCC_VERSION=4.9.1] [ENABLE_FORTRAN=1] ./build_gcc.sh`.  

\[A gfortran usage example can be found [here](https://github.com/tpoechtrager/osxcross/issues/28#issuecomment-67047134)]

But before you do this, make sure you have got the GCC build depedencies installed on your system.  

On debian like systems you can run:

`[sudo] apt-get install gcc g++ zlib1g-dev libmpc-dev libmpfr-dev libgmp-dev`  

to install them.

ATTENTION:

OSXCross links libgcc and libstdc++ statically by default (this affects `-oc-use-gcc-libs` too).  
You can turn this behavior off with `OSXCROSS_GCC_NO_STATIC_RUNTIME=1` (env).

### PACKAGING THE SDK: ###

If you need a recent SDK, then you must do the SDK packaging on OS X.  
Recent Xcode images are compressed, but the Linux kernel does not  
support HFS+ compression.

##### Packaging the SDK on an OS X machine: #####

1. [Download [Xcode](https://developer.apple.com/downloads/index.action?name=Xcode%205.1.1) \*\*]
2. [Mount Xcode.dmg (Open With -> DiskImageMounter) \*\*\*]
3. Run: ./tools/gen\_sdk\_package.sh (from the OSXCross package)
4. Copy the packaged SDK (\*.tar.\* or \*.pkg) on a USB Stick
5. (On Linux/BSD) Copy or move the SDK into the tarballs/ directory of OSXCross

\*\* Xcode 4.6, 5.0+, 6.0, and the 6.1 Betas are known to work.  
\*\*\* If you get a dialog with a crossed circle, ignore it, you don't need to install Xcode.

Step 1. and 2. can be skipped if you have Xcode installed.

##### Packing the SDK on a Linux machine (does *NOT* work with Xcode 4.3 or later!): #####

1. Download
   [Xcode 4.2](https://startpage.com/do/search?q=stackoverflow+xcode+4.2+download+snow+leopard)
   for Snow Leopard \*\*
2. Ensure you are downloading the "Snow Leopard" version
3. Install `dmg2img`
4. Run (as root): ./tools/mount\_xcode\_image.sh /path/to/xcode.dmg
5. Follow the instructions printed by ./tools/mount\_xcode\_image.sh
6. Copy or move the SDK into the tarballs/ directory

\*\* SHA1 Sum: 1a06882638996dfbff65ea6b4c6625842903ead3.

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
2. Build your source code with GCC or with clang and '-oc-use-gcc-libs'

Usage Examples:

* Clang:

  * C++98: `o32-clang++ -stdlib=libc++ test.cpp -o test`
  * C++11: `o32-clang++ -stdlib=libc++ -std=c++11 tes1.cpp -o test`
  * C++1y: `o32-clang++ -stdlib=libc++ -std=c++1y test1.cpp -o test`  

* Clang (shortcut):

  * C++98: `o32-clang++-libc++ test.cpp -o test`
  * C++11: `o32-clang++-libc++ -std=c++11 test.cpp -o test`
  * C++1y: `o32-clang++-libc++ -std=c++1y test.cpp  -o test`

* GCC (defaults to C++11 with libc++)

  * C++11: `o32-g++-libc++ test.cpp`
  * C++1y: `o32-g++-libc++ -std=c++1y test.cpp -o test`

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

### LICENSE: ####
  * scripts/wrapper: GPLv2
  * cctools/ld64: APSL 2.0
  * xar: New BSD

### CREDITS: ####
 * [cjacker for the cctools linux port](https://code.google.com/p/ios-toolchain-based-on-clang-for-linux/source/browse/#svn%2Ftrunk%2Fcctools-porting%2Fpatches)
