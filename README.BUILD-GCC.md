# Building GCC

Building GCC is optional. OSXCross works out of the box with the Clang toolchain provided by `./build.sh`;  
GCC is only needed if you specifically want a GNU compiler for your macOS targets.

## Dependencies

Debian/Ubuntu:

```sh
sudo apt-get install gcc g++ zlib1g-dev libmpc-dev libmpfr-dev libgmp-dev
```

## Build the ARM64 Darwin GCC fork

Run `./build_gcc_with_arm64_support.sh` to build the ARM64 Darwin
GCC fork from [`iains`](https://github.com/iains).

```sh
./build_gcc_with_arm64_support.sh
ARM64_GCC_REPO="gcc-darwin-arm64" ./build_gcc_with_arm64_support.sh # Builds trunk
```

## Build vanilla GCC (x86_64, with i386 multilib where supported)

```sh
./build_gcc.sh
GCC_VERSION=15.1.0 ENABLE_FORTRAN=1 ./build_gcc.sh # Builds 15.1.0, and enables Fortran
```

This uses separate build directories to build both `arm64` (`aarch64`) and
`x86_64` from the same source checkout and installs the full-triplet compiler
families `arm64-apple-<darwin>-gcc` and `x86_64-apple-<darwin>-gcc`.

Set `ARM64_GCC_REPO` to build a different repository; it defaults to
[`gcc-16-branch`](https://github.com/iains/gcc-16-branch).

## Notes

- To enable `-Werror=implicit-function-declaration`, set
  `OSXCROSS_ENABLE_WERROR_IMPLICIT_FUNCTION_DECLARATION=1`
- To disable static linking: `OSXCROSS_GCC_NO_STATIC_RUNTIME=1`
- `*-g++-libc++` uses Clang's libc++ — only use if needed
