# OSXCross Build Dependencies

This document lists the dependencies required to build the main OSXCross
toolchain with `build.sh`. Dependency names are distribution-independent;
`(dev)` denotes development headers and libraries.

## Stable and latest build flavor

Required dependencies:<br>
`Bash`, `Clang`, `CMake`, `Python 3`, `Git`, `GNU Make`, `patch`,<br>
`sed`, `tar`, `gzip`, `XZ Utils`, `bzip2`, `cpio`, `libxml2 (dev)`,<br>
`OpenSSL (dev)`, `zlib (dev)`, `liblzma (dev)`, `libbz2 (dev)`

### Debian/Ubuntu

```sh
sudo apt-get install \
  bash clang cmake python3 git make patch sed tar gzip xz-utils bzip2 cpio \
  libxml2-dev libssl-dev zlib1g-dev liblzma-dev libbz2-dev
```

### Optional dependencies

- `LLVM (dev)`: Enables Link Time Optimization support and ld64
  `-bitcode_bundle` support.
- `libuuid (dev)`: Enables ld64 `-random_uuid` support.

Debian/Ubuntu:

```sh
sudo apt-get install llvm-dev uuid-dev
```

## LLVM build flavor

Required dependencies:<br>
`Bash`, `Clang`, `LLVM`, `LLD`, `Git`, `GNU Make`, `patch`,<br>
`sed`, `tar`, `gzip`, `XZ Utils`, `bzip2`, `cpio`

### Debian/Ubuntu

```sh
sudo apt-get install \
  bash clang llvm lld git make patch sed tar gzip xz-utils bzip2 cpio
```

## Automatic dependency installation

On supported systems, the dependency helper installs the combined dependencies
for all build flavors, including the optional packages:

```sh
sudo tools/get_dependencies.sh
```

The following systems are detected automatically:

| System family | Detected systems | Package manager |
|---|---|---|
| Debian | Debian, Ubuntu, Raspbian, Linux Mint | `apt-get` |
| Fedora | Fedora, RHEL, CentOS, Rocky Linux, AlmaLinux | `yum` |
| SUSE | openSUSE and other SUSE-based systems | `zypper` |
| Mageia | Mageia | `urpmi` |
| Arch | Arch Linux | `pacman` and AUR |
| FreeBSD | FreeBSD | `pkg` |
| NetBSD | NetBSD | `pkgin` |

On Arch Linux, the helper additionally builds the `uuid` package from the AUR.
This step must run as a non-root build user.
