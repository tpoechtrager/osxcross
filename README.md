# OSXCross: OS X cross toolchain for Linux #

### WHAT IS THE GOAL OF OSXCROSS? ###

The goal of OSXCross is to deliever you a high quality cross toolchain targeting OS X on Linux.

### HOW DOES IT WORK? ###

[Clang/LLVM is a cross compiler by default](http://clang.llvm.org/docs/CrossCompilation.html) and is now available on nearly every Linux distrubtion.  
Therefore we "just" need a proper
[port](https://code.google.com/p/ios-toolchain-based-on-clang-for-linux/source/browse/#svn%2Ftrunk%2Fcctools-porting%2Fpatches)
of the [cctools](http://www.opensource.apple.com/tarballs/cctools) (ld, lipo, ...) for Linux, and the OS X SDK.

If you want, then you can build an up-to-date vanilla GCC as well.

### WHAT IS NOT WORKING (YET)? ###

* Clang:
    * using [libc++](http://libcxx.llvm.org/) (`-stdlib=libc++`) doesn't work yet (missing headers, besides that it should work)
* GCC:
    * debug info is weak, because of the [missing](https://github.com/tpoechtrager/osxcross/blob/master/patches/gcc-dsymutil.patch)
      [`dsymutil`](http://www.manpagez.com/man/1/dsymutil) (shows only function names, no line numbers)
    * GCC itself [doesn't build with GCC](https://github.com/tpoechtrager/osxcross/commit/12f5dcdde4bc1000180d25ffda229f0a13cf723d),
but builds fine when clang is used to build GCC

Everything else besides that should work.

### WHAT CAN I BUILD WITH IT? ###

Basically everything you can build on OS X with clang/gcc should build with this cross toolchain as well.

### INSTALLATION: ###

Download the SDK version you want to the tarball/ (important!) directory.

Then assure you have the following installed on your Linux box:

`Clang 3.2+`, `llvm-devel`, `automake`, `autogen`, `libtool`,  
`libxml2-devel`, `libuuid-devel`, `openssl-devel` and the `bash shell`.

Now edit the `SDK_VERSION` in `build.sh`, so it matches the version you have downloaded before.

Then run `./build.sh` to build the cross toolchain (It will build in it's own directory).

**Don't forget** to add the printed `export PATH=...` to your `~/.profile` or `~/.bashrc`.  
Then either run `source ~/.profile` or restart your shell session.

That's it. See Usage Examples below.

##### Building GCC: #####

If you want to build GCC as well, then you can do this by running `./build_gcc.sh`.  
But before you do this, make sure you have got the gcc build depedencies installed on your system,  
on debian like systems you can run `apt-get build-dep gcc` to install them.

### SDK DOWNLOAD LINKS: ###

  * [10.4 (Tiger)](http://www.mediafire.com/?zo9xuv5lsnucazy) (Note: Set SDK_VERSION to 10.4u)
  * [10.5 (Leopard)](http://www.mediafire.com/?y5gqvy02jr6g8t0)
  * 10.6 (Snow Leopard)
  * [10.7 (Lion)](http://www.mediafire.com/?jrprt27obohlrwe)
  * [10.8 (Mountain Lion)](http://www.mediafire.com/?pf99jk7u18e3kk8) **\*\*recommended\*\***
  * [10.9 (Mavericks)](http://www.mediafire.com/?pf99jk7u18e3kk8)

### Usage Examples: ###

##### Let's say you want to compile a file called test.cpp, then you can do this by running: #####

* Clang:

  * 32 bit:  `o32-clang test.cpp -O3 -o test`   OR   `i386-apple-darwinXX-clang test.cpp -O3 -o test`
  * 64 bit:  `o64-clang test.cpp -O3 -o test`   OR   `x86_64-apple-darwinXX-clang test.cpp -O3 -o test`

* GCC:

  * 32 bit:  `o32-gcc test.cpp -O3 -o test`  OR   `i386-apple-darwinXX-gcc test.cpp -O3 -o test`
  * 64 bit:  `o64-gcc test.cpp -O3 -o test`   OR   `x86_64-apple-darwinXX-gcc test.cpp -O3 -o test`

XX= the target version, you can find it out by running  `osxcross-conf`  and then see TARGET.

You can use the shorting `o32-...` or `i386-apple-darwin...` what ever you like more.

*I'll continue from now on with `o32-clang`, but remember you can simply replace it with `o32-gcc` or `i386-apple-darwin...`*

##### Building Makefile based projects: #####

  `make CC=o32-clang CXX=o32-clang++`

##### Building automake based projects: #####

  `CC=o32-clang CXX=o32-clang++ ./configure --host=i386-apple-darwinXX`

##### Building test1.cpp and test2.cpp with LTO (Link Time Optimization): #####

  * build the first object file: `o32-clang++ test1.cpp -O3 -flto -c`
  * build the second object file: `o32-clang++ test2.cpp -O3 -flto -c`
  * link them with LTO: `o32-clang++ -O3 -flto test1.o test2.o -o test`

##### Building a universal binary: #####

clang:

  `o64-clang++ test.cpp -O3 -arch i386 -arch x86_64 -o test`

GCC:

  * build the 32 bit binary: `o32-g++ test.cpp -O3 -o test.i386`
  * build the 64 bit binary: `o64-g++ test.cpp -O3 -o test.x86_64`
  * use lipo to generate the universal binary: `x86_64-apple darwinXX-lipo -create test.i386 test.x86_64 -output test`


### LICENSE: ####
  * bash scripts: GPLv2
  * cctools: APSL 2.0
  * xar: New BSD


### CREDITS: ####
 * [cjacker for the cctools linux port](https://code.google.com/p/ios-toolchain-based-on-clang-for-linux/source/browse/#svn%2Ftrunk%2Fcctools-porting%2Fpatches)
