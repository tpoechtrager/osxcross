# Building LLVM/Clang

Building the host LLVM/Clang toolchain requires a native C/C++ compiler and the listed build utilities.  
It does not require Clang to be installed already.

## Dependencies

Debian/Ubuntu:

```sh
sudo apt install bash gcc g++ cmake curl unzip make patch sed gzip python3
```

## Build current upstream LLVM/Clang

```sh
./build_clang.sh
```

This builds mainline LLVM/Clang together with LLD.

## Build Apple Clang

```sh
./build_apple_clang.sh
```

This builds Apple LLVM/Clang together with LLD.

## Custom installation path

Set `INSTALLPREFIX` to install into a directory other than the script default:

```sh
INSTALLPREFIX=/opt/clang ./build_clang.sh
```
