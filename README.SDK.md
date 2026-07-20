# Packaging the SDK

**[Please ensure you have read and understood the Xcode license terms before continuing.](https://www.apple.com/legal/sla/docs/xcode.pdf)**

SDKs can be extracted either from the full Xcode or from the Xcode Command Line Tools.

## On macOS

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

## On Linux (and others)

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
