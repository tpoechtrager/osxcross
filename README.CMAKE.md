## OSXCross CMake Integration

OSXCross installs architecture-specific CMake launchers which configure CMake for cross-compiling to macOS.  

The launchers select the target architecture, compiler family and C++ standard library, load the matching  
OSXCross configuration, and use the installed `toolchain.cmake` file automatically.

### Requirements

- A completed OSXCross installation (`./build.sh`)
- `<OSXCROSS-TARGET>/bin` in `PATH`
- CMake 3.21 or newer
- A build backend supported by CMake, such as Make or Ninja

CMake 3.21 is required because the launchers provide the toolchain file through the `CMAKE_TOOLCHAIN_FILE`  
environment variable. Replace `darwinXX` in the examples below with the target reported by `osxcross-conf`,  
for example `darwin23`.

### Installed CMake Launchers

`./build.sh` installs these launchers for every architecture selected through
`ENABLE_ARCHS`:

| Launcher suffix | C compiler | C++ compiler / standard library |
| --- | --- | --- |
| `-cmake` | Clang | Clang++ with the default (normally libc++) |
| `-cmake-clang` | Clang | Clang++ with the default (normally libc++) |
| `-cmake-clang-libc++` | Clang | Clang++ explicitly with libc++ |
| `-cmake-clang-gstdc++` | Clang | Clang++ with the GCC installation's libstdc++ |

For example, a build with `ENABLE_ARCHS="arm64 x86_64"` provides launchers such
as:

```text
arm64-apple-darwinXX-cmake
arm64-apple-darwinXX-cmake-clang
arm64-apple-darwinXX-cmake-clang-libc++
x86_64-apple-darwinXX-cmake
x86_64-apple-darwinXX-cmake-clang-gstdc++
```

The unsuffixed `-cmake` launcher is the recommended default. The `-cmake-clang` form is an explicit alias for the same compiler selection.  
`-cmake-clang-libc++` selects the `clang++-libc++` wrapper and therefore explicitly uses libc++, independently of the OSXCross default.  
`-cmake-clang-gstdc++` selects the `clang++-gstdc++` wrapper and uses the libstdc++ headers and libraries installed by `./build_gcc.sh`.  
A GCC build for the selected target architecture must therefore be available.

After `./build_gcc.sh`, OSXCross additionally installs GCC launchers for each
GCC target architecture built:

| Launcher suffix | C compiler | C++ compiler / standard library |
| --- | --- | --- |
| `-cmake-gcc` | GCC | G++ with libstdc++ |
| `-cmake-gcc-libc++` | GCC | G++ with libc++ |

The regular GCC build targets x86_64. The experimental ARM64 GCC build also
installs `aarch64-apple-darwinXX-*` launchers and equivalent
`arm64-apple-darwinXX-*` CMake aliases.

Do not run `osxcross-cmake` directly and do not use the installed `toolchain.cmake` by itself.  
The architecture-specific launcher supplies the OSXCross configuration and compiler selection required by the toolchain file.

### Quick Start

Given this small C++ project:

```text
hello/
|-- CMakeLists.txt
`-- main.cpp
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.21)
project(hello LANGUAGES CXX)

add_executable(hello main.cpp)
target_compile_features(hello PRIVATE cxx_std_17)
```

`main.cpp`:

```cpp
#include <iostream>

int main()
{
    std::cout << "Hello from macOS!\n";
}
```

Configure and build for Apple Silicon:

```sh
arm64-apple-darwinXX-cmake -S hello -B build-arm64 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm64 --parallel
file build-arm64/hello
```

To build for x86_64 instead, change the launcher and use a separate build
directory:

```sh
x86_64-apple-darwinXX-cmake -S hello -B build-x86_64 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-x86_64 --parallel
file build-x86_64/hello
```

### Compiler and Standard Library Selection

Use a fresh build directory whenever the architecture, compiler family or C++
standard library changes. CMake stores all of these choices in `CMakeCache.txt`
on the first configure run.

```sh
# Clang and libc++ (recommended)
arm64-apple-darwinXX-cmake -S . -B build-clang

# Clang and explicitly selected libc++
arm64-apple-darwinXX-cmake-clang-libc++ -S . -B build-clang-libcxx

# Clang with the GCC installation's libstdc++ (requires ./build_gcc.sh)
x86_64-apple-darwinXX-cmake-clang-gstdc++ -S . -B build-clang-libstdcxx

# GCC and libstdc++ (requires ./build_gcc.sh)
x86_64-apple-darwinXX-cmake-gcc -S . -B build-gcc

# GCC and libc++ (requires ./build_gcc.sh)
x86_64-apple-darwinXX-cmake-gcc-libc++ -S . -B build-gcc-libcxx
```

To see the exact compilers selected by a configured build:

```sh
arm64-apple-darwinXX-cmake -S . -B build
arm64-apple-darwinXX-cmake -LA -N build | \
  grep -E '^CMAKE_(C|CXX)_COMPILER:'
```

### Deployment Target

The deployment target controls the oldest macOS version on which the generated
binaries may run. Set it through `CMAKE_OSX_DEPLOYMENT_TARGET` when configuring:

```sh
arm64-apple-darwinXX-cmake -S . -B build-arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

x86_64-apple-darwinXX-cmake -S . -B build-x86_64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
```

It must not be newer than the selected SDK. ARM64 requires macOS 11.0 or newer.
If this option is omitted, the deployment target configured when OSXCross was
built is used. `MACOSX_DEPLOYMENT_TARGET` can also be set in the environment,
but an explicit CMake cache option is easier to reproduce.

### Universal Binaries

Clang can produce a universal binary in one build when all requested
architectures were enabled in OSXCross. Pass a semicolon-separated architecture
list to CMake (quote it so the shell does not interpret the semicolon):

```sh
arm64-apple-darwinXX-cmake -S . -B build-universal \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-universal --parallel
xcrun lipo -info build-universal/hello
```

Use the default Clang/libc++ launcher for universal builds. Alternatively,
build each architecture in a separate directory and combine the final binaries
with `xcrun lipo -create`.

### Libraries, Packages and Frameworks

The OSXCross toolchain configures CMake so that:

- build tools and programs are found on the host;
- headers, libraries and CMake packages are found in the macOS SDK and the
  OSXCross MacPorts prefix;
- `ar`, `ranlib` and `install_name_tool` use their OSXCross wrappers;
- pkg-config searches the OSXCross MacPorts package directory.

This prevents accidental linking against Linux or BSD libraries. Install target
dependencies with `osxcross-macports` where possible:

```sh
osxcross-macports install zlib
arm64-apple-darwinXX-cmake -S . -B build
cmake --build build --parallel
```

A normal CMake package remains portable and does not need OSXCross-specific
compiler paths in its `CMakeLists.txt`:

```cmake
find_package(ZLIB REQUIRED)
target_link_libraries(hello PRIVATE ZLIB::ZLIB)
```

Apple frameworks can be resolved using standard CMake commands:

```cmake
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
target_link_libraries(hello PRIVATE "${COREFOUNDATION_FRAMEWORK}")
```

See [README.MACPORTS.md](README.MACPORTS.md) and
[README.PKG-CONFIG.md](README.PKG-CONFIG.md) for package-specific details.

### Installing and Testing

Install into a staging directory on the build host instead of using CMake's
usual `/usr/local` default:

```sh
arm64-apple-darwinXX-cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix "$PWD/stage"
```

CTest can build tests normally, but the resulting test executables cannot run
on the cross-compilation host. Run them after copying the staged files to a
compatible macOS system. Projects which execute target programs during
configuration may need a project-specific cross-compilation option or an
emulator; `try_run()` cannot run a Mach-O executable on Linux or BSD by itself.

### Troubleshooting

**CMake uses the host compiler or reports that the toolchain is missing**

Use an architecture-specific OSXCross launcher for the first configuration and
CMake 3.21 or newer. Remove the old build directory if it was previously
configured with plain `cmake`.

**Changing the launcher does not change the compiler**

CMake caches the compiler and toolchain. Use a separate build directory or
remove the complete old build tree before reconfiguring.

**A dependency is found in `/usr/lib` or `/usr/include`**

That dependency belongs to the host and must not be linked into a macOS target.
Install a macOS build of the dependency, for example through
`osxcross-macports`, and clear the affected CMake cache entries.

**CMake fails while trying to execute a test program**

The project is attempting to run a macOS executable on the build host. Check
the project for a documented cross-compilation switch. For checks which only
need to compile or link, the project may support
`-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`.

**The deployment target or architecture appears unchanged**

Both values are cached. Configure a new build directory and pass
`CMAKE_OSX_DEPLOYMENT_TARGET` or `CMAKE_OSX_ARCHITECTURES` on its first
configuration.
