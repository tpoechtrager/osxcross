# SDK

OSXCross supports two types of SDKs:

| SDK | Made from | Use it for |
|---|---|---|
| [Apple's proprietary SDK (**recommended**)](#packaging-apples-sdk) | Xcode or the Command Line Tools | General use. It has every framework and library of macOS. |
| [OSXCross Open SDK](#osxcross-open-sdk) | Open source only | C, C++, GCC, and CI jobs that need no proprietary Apple SDK.<br>Not a full replacement. |

## Packaging Apple's SDK

**[Please ensure you have read and understood the Xcode license terms before continuing.](https://www.apple.com/legal/sla/docs/xcode.pdf)**

SDKs can be extracted either from the full Xcode or from the Xcode Command Line Tools.

### On macOS

**From Full Xcode**

1. [Download Xcode](https://developer.apple.com/download/all/?q=xcode)
2. Mount `Xcode.dmg` (Right-click → Open With → DiskImageMounter)
   - If you see a crossed-circle dialog when mounting, ignore it — installation of Xcode is not required
3. Run: `./tools/gen_sdk_package.sh` (from the OSXCross package)
4. Copy the resulting SDK (`*.tar.*` or `*.pkg`) to a USB stick
5. On Linux/BSD, move the SDK to the `tarballs/` directory of OSXCross

**From Command Line Tools**

1. [Download Command Line Tools](https://developer.apple.com/download/all/?q=Command%20Line%20Tools%20for%20Xcode)
2. Mount the `Command_Line_Tools_for_Xcode.dmg` (Open With → DiskImageMounter)
3. Install `Command Line Tools.pkg` (Open With → Installer)
4. Run: `./tools/gen_sdk_package_tools.sh`
5. Copy the resulting SDK (`*.tar.*` or `*.pkg`) to a USB stick
6. On Linux/BSD, move the SDK to the `tarballs/` directory of OSXCross

### On Linux (and others)

**Method 1 (Xcode > 8.0)**\
*Requires up to 45 GB free disk space. SSD strongly recommended.*

1. Download Xcode as described above
2. Install: `clang`, `make`, `libssl-devel`, `lzma-devel`, and `libxml2-devel`
3. Run: `./tools/gen_sdk_package_pbzx.sh <xcode>.xip`
4. Move the SDK to the `tarballs/` directory

**Method 2 (up to Xcode 7.3)**

1. Download Xcode as described above
2. Install: `cmake`, `libxml2-dev`, and `fuse`
3. Run: `./tools/gen_sdk_package_darling_dmg.sh <xcode>.dmg`
4. Move the SDK to the `tarballs/` directory

**Method 3 (up to Xcode 7.2)**

1. Download Xcode as described above
2. Ensure `clang` and `make` are installed
3. Run: `./tools/gen_sdk_package_p7zip.sh <xcode>.dmg`
4. Move the SDK to the `tarballs/` directory

**Method 4 (Xcode 4.2)**

1. Download Xcode 4.2 for Snow Leopard (ensure it's the correct version)
2. Install `dmg2img`
3. As root, run: `./tools/mount_xcode_image.sh /path/to/xcode.dmg`
4. Follow the on-screen instructions from the script
5. Move the SDK to the `tarballs/` directory

**From Xcode Command Line Tools**

1. Download Command Line Tools as described above
2. Install: `clang`, `make`, `libssl-devel`, `lzma-devel`, and `libxml2-devel`
3. Run: `./tools/gen_sdk_package_tools_dmg.sh <command_line_tools_for_xcode>.dmg`
4. Move the SDK to the `tarballs/` directory

## OSXCross Open SDK

The [OSXCross Open SDK](https://github.com/tpoechtrager/osxcross-open-sdk) is a macOS SDK built from open source only.

**What it can do**

- C, C++ (with `libc++`), Objective-C without `Foundation`, Swift without `Foundation` and Fortran (with GCC),  
  for x86_64 and arm64.
- Build GCC: `build_gcc.sh` builds completely with it, with C, C++ and Fortran for x86_64 and arm64.
- The C library, POSIX, Mach, `pthreads`, `libdispatch` and blocks.
- The libraries of `/usr/lib`: `zlib`, `bzip2`, `expat`, `libffi`, `libedit`, `libxml2`, `libxslt`, `sqlite3`,  
  `libcurl`, `libarchive`, `libpcap`, `OpenPAM`, `ncurses`, `libiconv`, `libresolv`, `CommonCrypto`, `dns_sd`  
  and the launchd API.
- `os_log`: the messages appear in the unified log of macOS.
- The frameworks `CoreFoundation`, `DiskArbitration`, `SystemConfiguration`, `Security` and `IOKit`  
  (without the device families).
- Open source projects such as `zlib`, `curl`, `git`, `cmake`, `sqlite`, `bash`, `coreutils`, `fmt` and  
  `redis` were built with it.

**What it cannot do**

- It is not a full replacement for Apple's SDKs.
- It has no `Foundation`, `AppKit` and none of the other frameworks without public source,  
  so it cannot build programs that need them, such as GUI applications.
- Swift with `Foundation`, `XPC`, `FSEvents`, `LaunchServices` and the `IOKit` device families are missing too.

**How to use it**

1. [Download the archive for the macOS release you need](https://github.com/tpoechtrager/osxcross-open-sdk):
   - `MacOSX<VER>-open-sdk-<hash>.tar.xz` is the full SDK with everything listed above. Use it by default.
   - `MacOSX<VER>-open-core-sdk-<hash>.tar.xz` is the core SDK. It has only the C library, C++ and POSIX,  
     for GCC with `libstdc++` and small CI jobs.
2. Move it to the `tarballs/` directory of OSXCross.
3. Build as usual.
