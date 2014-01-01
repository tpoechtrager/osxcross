## OS X Cross toolchain for Linux and FreeBSD ##

### WHAT IS THE GOAL OF OSXCROSS? ###

The goal of OSXCross is to provide a well working OS X cross toolchain for Linux and FreeBSD.

### HOW DOES IT WORK? ###

[Clang/LLVM is a cross compiler by default](http://clang.llvm.org/docs/CrossCompilation.html) and is now available on nearly every Linux distribution,  
so we just need a proper
[port](https://github.com/tpoechtrager/cctools-port)
of the [cctools](http://www.opensource.apple.com/tarballs/cctools) (ld, lipo, ...) and the OS X SDK.

If you want, then you can build an up-to-date vanilla GCC as well.

### WHAT IS NOT WORKING (YET)? ###

* GCC itself [doesn't build with GCC](https://github.com/tpoechtrager/osxcross/commit/12f5dcdde4bc1000180d25ffda229f0a13cf723d),
      but builds fine when clang is used to build GCC.

### WHAT CAN I BUILD WITH IT? ###

Basically everything you can build on OS X with clang/gcc should build with this cross toolchain as well.

### INSTALLATION: ###

Download the SDK version (links below) you want to the tarball/ (important!) directory.

Then ensure you have the following installed on your Linux/FreeBSD box:

`Clang 3.2+`, `llvm-devel`, `automake`, `autogen`, `libtool`,  
`libxml2-devel` (<=10.5 only), `uuid-devel`, `openssl-devel` and the `bash shell`.

Hint: On Ubuntu 12.04 LTS you can use [llvm.org/apt](http://llvm.org/apt) to get a newer version of clang.

Now edit the `SDK_VERSION` in `build.sh`, so it matches the version you have downloaded before.

Then run `./build.sh` to build the cross toolchain (it will build in its own directory).

**Don't forget** to add the printed `` `<path>/osxcross-env` `` to your `~/.profile` or `~/.bashrc`.  
Then either run `source ~/.profile` or restart your shell session.

That's it. See usage examples below.

##### Building libc++: #####

If you want to build libc++ for modern C++11 with clang, then you can do this by running `./build_libcxx.sh`.  
The resulting library will be linked statically into the applications to avoid troubles with different  
libc++.dylib versions on OS X.

See below in how to use libc++ as the standard library.

##### Building GCC: #####

If you want to build GCC as well, then you can do this by running `./build_gcc.sh`.  
But before you do this, make sure you have got the GCC build depedencies installed on your system,  
on debian like systems you can run `apt-get install mpc-dev mpfr-dev gmp-dev` to install them.

### SDK DOWNLOAD LINKS: ###

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

*I'll continue from now on with `o32-clang`, but remember, you can simply replace it with `o32-gcc` or `i386-apple-darwin...`.*

##### Building Makefile based projects: #####

  * `make CC=o32-clang CXX=o32-clang++`

##### Building automake based projects: #####

  * `CC=o32-clang CXX=o32-clang++ ./configure --host=i386-apple-darwinXX`

##### Building test.cpp with libc++: #####

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


### LICENSE: ####
  * bash scripts: GPLv2
  * cctools: APSL 2.0
  * xar: New BSD


### CREDITS: ####
 * [cjacker for the cctools linux port](https://code.google.com/p/ios-toolchain-based-on-clang-for-linux/source/browse/#svn%2Ftrunk%2Fcctools-porting%2Fpatches)
