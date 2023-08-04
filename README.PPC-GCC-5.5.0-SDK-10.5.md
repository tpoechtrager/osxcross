## OS X Cross toolchain GCC-5.5.0 and MacOS SDK-10.5 targeting PowerPC and PowerPC64 ##


### Introduction: ###

These detailed instructions on how I was able to use the `ppc-test` branch of the
[osxcross](https://github.com/tpoechtrager/osxcross) project to build a toolchain
on an Ubuntu16 build host to target MacOS-10.5 PowerPC and PowerPC64 using the
mainline GCC-5.5.0. This toolchain supports C (C89, C90, C11), C++ (up to C++14),
and GNU Fortran (F77 and F90).

From my testing so far, the toolchain is working flawlessly. I have provided some
scripts to test the toolchain. See the toolchain test section below.

**NOTE:** MakcOS-10.5 does not have native support for C++11 or any recent C++
standards. With OS X Cross, we can use the GCC-5.5.0 and the GNU STDC++ runtime
library provided by that toolchain to build and run C++11 and C++14 code on
MacOS-10.5 PowerPC targets.

**NOTE:** I had to make slight modifications to the `build_gcc.sh` script in
order for it to retrieve the mainline GCC-5.5.0 source and to download the
build prerequisites.

My journey to creating this toolchain started
[here](https://github.com/tpoechtrager/osxcross/issues/50).

---


### What is Working: ###


#### PowerPC32: ####

C and C++ compilers are able to build code that runs on my iBook G4.
C++03, C++11, C++14 programs can be built and run well on the iBook G4.

```
Darwin ibook-g4.local 9.8.0 Darwin Kernel Version 9.8.0: Wed Jul 15 16:57:01 PDT 2009; root:xnu-1228.15.4~1/RELEASE_PPC Power Macintosh
```

The GNU STDC++ is statically linked so that we do not have to hastle with it.
I am able to build and run C99, C11, C++03, C++11, and C++14 code.


#### PowerPC364 ####

C, C++, and Fortran compilers are able to build code. I do not have a PowerPC64
Mac to test on at the moment.

The GNU STDC++ is statically linked so that we do not have to hastle with it.
I am able to build Fortran77, Fortran90, C99, C11, C++03, C++11, and C++14 code.

---


### What is Not Working: ###


#### PowerPC32: ####

The PPC32 Fortran compiler is not being staged (or built?). I dont know why this
is happening.


#### PowerPC364 ####

The PPC64 compiler reports warning that it cannot find apple gcc intrinsic
headers. The toolchain seems to work regardless. I dont know how to resolve this
issue or it it even matters.

```
osxcross: warning: cannot find apple gcc intrinsic headers; please report this issue to the OSXCross project
```

---


### Prepare the Build Host: ####

I am using an Ubuntu16 VM:

```
lsb_release -a
   LSB Version:    core-9.20160110ubuntu0.2-amd64:..
   Distributor ID: Ubuntu
   Description:    Ubuntu 16.04.7 LTS
   Release:        16.04
   Codename:       xenial

```

The following packages are installed:

```
sudo apt install -y ubuntu-standard
sudo apt install -y build-essential
sudo apt install -y clang
sudo apt install -y perl
sudo apt install -y python
sudo apt install -y wget
sudo apt install -y libxml2-dev
sudo apt install -y uuid-dev
```

The following package versions are installed:

```
BASH: v4.3.48
GLIBC: v2.23
GNU Make: v4.1
Binutils: v2.26.1
GCC: (Ubuntu 5.4.0-6ubuntu1~16.04.12) 5.4.0 20160609
Clang: v3.8.0-2ubuntu4
Perl: v5.22.1
Python: v2.7.12
WGet: v1.17.1
libxml2-dev: v2.9.3
uuid-dev: v2.27.1

```

---


### Build the Toolchain: ####


#### Clone my OS X Cross Branch: ####

I have had to make a few minor changes to the `build_gcc.sh`, some scripts that
can be used to test the Toolchain, and detailed instructions on how I built this
toolchain. This is currently in an unmerged branch.

```
# Clone the unmerged branch:
git clone \
   -b ppc-test-202308 \
   https://github.com/jlsantiago0/osxcross.git \
   osxcross-ppc-202308

# Change directories:
cd osxcross-ppc-202308

# Add target/bin to the beginning of PATH:
export export PATH="$(pwd)/target/bin:${PATH}"
```


#### Obtain the MacOS-10.5 SDK: ####

Obtain and repackage the MacOS-10.5 SDK.

I obtained the SDK from [here](https://github.com/phracker/MacOSX-SDKs/releases).
Other options are discussed
[here](https://github.com/tpoechtrager/osxcross/blob/master/README.md).

**NOTE:** We need to repackage the SDK as .tar.gz so that the scripts can find
it.

```
# Obtain the SDK:
wget https://github.com/phracker/MacOSX-SDKs/releases/download/11.3/MacOSX10.5.sdk.tar.xz

# Extract it:
tar xf ./MacOSX10.5.sdk.tar.xz

# Repackage the SDK as .tar.gz so that the scripts can find it:
tar czf ./tarballs/MacOSX10.5.sdk.tar.gz MacOSX10.5.sdk
```


#### Build OS X Cross: ####

Build OS X Cross. They will be staged in `$(pwd)/target/bin` thich needs to be
at the beginning of PATH:

```
CDEBUG=1 UNATTENDED=1 SDK_VERSION=10.5 ./build.sh
```

Ensure that `xcrun` is working:

```
which xcrun
xcrun -f ar
xcrun -f otool
xcrun otool -arch all --version
xcrun --show-sdk-version
xcrun --show-sdk-path
```

Ensure that the tools run:

```
which powerpc-apple-darwin9-sw_vers
powerpc-apple-darwin9-sw_vers

which powerpc64-apple-darwin9-sw_vers
powerpc64-apple-darwin9-sw_vers
```

**NOTE:** dsymutil does not seem to support PowerPC targets. So not building it.


#### Build GCC-5.5.0: ####

This builds the mainline GCC-5.5.0 to target MacOS-10.5 PowerPC and PowerPC64.

**NOTE:** The SDK C++ headers break the GCC build and the resulting C++ compiler
To work around this issue, we move them to hide them from GCC. Since we will only
be using GCC with this toolchain from now on and not Clang, this seems fine. We
had to wait to move them, until after the OS X Cross tools are built with Clang.
Because Cland does needs these headers. After the OS X Cross tools are built and
are usable, we no longer need Clang or these headers to build anything else.

```
# **NOTE:** the `SDK/include/c++/4.0.0` directory causes issues building GCC and
#  breaks the resulting C++ compiler for PowerPC64. Moving the directory will
#  hide them from GCC.
mv ./target/SDK/MacOSX10.5.sdk/usr/include/c++/4.0.0 \
   ./target/SDK/MacOSX10.5.sdk/usr/include/c++/4.0.0.dontuse
```
Build mainline GCC-5.5.0 to target MacOS-10.5 PowerPC:

```
# Build GCC for POWERPC targets
DEBUG=1 UNATTENDED=1 GCC_VERSION=5.5.0 ENABLE_FORTRAN=1 POWERPC=1 \
   ./build_gcc.sh

# Make sure it runs:
which powerpc-apple-darwin9-gcc
powerpc-apple-darwin9-gcc --version
which powerpc64-apple-darwin9-gcc
powerpc64-apple-darwin9-gcc --version
```

---


### Test the Toolchain: ####

Some scripts to test the toolchain:

`test_simple.sh`: Builds some perhaps trivial C, C++, and Fortran programs from
single source files.

`test_autotools.sh`: Builds a number of non-trivial Autotools Configured projects
from recent versions of theie mainline source. This includes OpenSSL, WGet
and CURL.


#### Build Some Simple Programs: ####

The script `test_simple.sh` generates single source file programs and compiles
them using this toolchain. Coverage includes: C (C89 and C11),
C++ (C++03, C++11, C++14), and Fortran (Fortran77 and Fortran90).

Requires the OSX Cross stage directory `target/bin` at the beginning of PATH and
`xcrun -f cc` must provide the C compiler program information.

**NOTE:** Environment variables that effect the test script operations:

```
# Set the test architecture to use:
OSXCROSS_TEST_ARCH=powerpc|powerpc64
```

The following programs are built:

   - FFPROG01: Fortran77 Helloworld Program:
   - FFPROG02: Fortran90 Helloworld Program:
   - CCPROG01: C89 Helloworld Program:
   - CCPROG02: C11 Program Using PThreads:
   - CXPROG01: C++03 Helloworld Program:
   - CXPROG02: C++11 Program Using ::std::thread:
   - CXPROG03: C++11 Program Using Lamdas:
   - CXPROG04: C++14 Program Using Auto Lamda Parameter:
   - CXPROG05: C++14 Program Using ::std::shared_timed_mutex:

**NOTE:**: `::std::shared_timed_mutex:` was introduced in MacOS-10.12. Since we
are using the GNU STDC++ library, we can now use these C++14 features in
programs running on MacOS-10.5 and PowerPC targets.

Run the test:

```
# Target PowerPC:
# Results will be in ./test/powerpc/:
OSXCROSS_TEST_ARCH=powerpc ./test_simple.sh

# Target PowerPC64:
# Results will be in ./test/powerpc64/:
OSXCROSS_TEST_ARCH=powerpc64 ./test_simple.sh
```


#### Build Some Non-Trivial Autotools Configured Projects: ####

The script `test_autotools.sh` downloads the mainline source for recent versions
of OpenSSL, WGet and CURL and builds them using this toolchain.

Requires the OSX Cross stage directory `target/bin` at the beginning of PATH and
`xcrun -f cc` must provide the C compiler program information.

**NOTE:** Environment variables that effect the test script operations:

```
# Set the test architecture to use:
OSXCROSS_TEST_ARCH=powerpc|powerpc64

# Trigger a rebuild of everything:
OSXCROSS_TEST_REBUILD=0|1

# Project Versions:
OSXCROSS_TEST_OPENSSL_VERSION=3.1.2
OSXCROSS_TEST_WGET_VERSION=1.21.4
OSXCROSS_TEST_CURL_VERSION=8.2.1
```

Run the test:

```
# Target PowerPC:
# Results will be in ./test/powerpc/:
OSXCROSS_TEST_ARCH=powerpc ./test_autotools.sh

# Target PowerPC64:
# Results will be in ./test/powerpc64/:
OSXCROSS_TEST_ARCH=powerpc64 ./test_autotools.sh
```

---


### TODO: ###

1. Test the PowerPC64 builds. I do not have access to one at this time.

2. Automatically statically link the libgcc_s and libatomic. That way noone has
to hastle with them. Maybe the wrapper can do it in a similar fashion as the
GNU STDC++ library is handled by the toolchain.

3. Create a test for a non-trivial CMake project. Perhaps auto generate a CMake
Toolchain file to use with the OS X Cross Toolchain.

4. Try to replicate this with a newer GCC Toolchain. Perhaps GCC-10.5.0
or GCC-13.x.
