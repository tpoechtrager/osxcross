# OSXCross Nix Flake

A Nix flake for building macOS cross-compilation toolchains on Linux.

## Quick Start

### Building the Toolchain

```nix
# flake.nix in your project
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    osxcross.url = "github:tpoechtrager/osxcross";
  };

  outputs = { self, nixpkgs, osxcross }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};

    # Build the toolchain with your SDK
    toolchain = osxcross.lib.${system}.mkOsxcross {
      sdkPath = ./path/to/MacOSX14.5.sdk.tar.xz;
      # Optional: explicitly set version if filename doesn't include it
      # sdkVersion = "14.5";
    };
  in {
    # Use the toolchain in your builds
    packages.${system}.myApp = pkgs.stdenv.mkDerivation {
      # ...
      nativeBuildInputs = [ toolchain ];
    };
  };
}
```

### Command Line Usage

```bash
# Build with explicit SDK path and version
nix build --impure --expr '
  let
    flake = builtins.getFlake (toString ./.);
    toolchain = flake.lib.x86_64-linux.mkOsxcross {
      sdkPath = /path/to/MacOSX14.5.sdk.tar.xz;
      sdkVersion = "14.5";
    };
  in toolchain
'

# Use the toolchain
./result/bin/arm64-apple-darwin23-clang -o hello hello.c
./result/bin/x86_64-apple-darwin23-clang -o hello_x86 hello.c
```

## Configuration Options

### `mkOsxcross` Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `sdkPath` | path | Yes | Path to macOS SDK tarball (`.tar`, `.tar.xz`, `.tar.gz`, `.tar.bz2`) |
| `sdkVersion` | string | No | SDK version (auto-detected from filename if not provided) |
| `osxVersionMin` | string | No | Minimum macOS deployment target (default: SDK's minimum) |
| `enableArchs` | list | No | Architectures to enable (default: all supported by SDK) |
| `enableLTO` | bool | No | Enable LTO support (default: `true`) |

### Example Configurations

```nix
# Minimal - auto-detect everything from SDK filename
toolchain = mkOsxcross {
  sdkPath = ./MacOSX14.5.sdk.tar.xz;
};

# Explicit version (useful if filename doesn't include version)
toolchain = mkOsxcross {
  sdkPath = ./MacOSX.sdk.tar;
  sdkVersion = "14.5";
};

# Restrict to specific architectures
toolchain = mkOsxcross {
  sdkPath = ./MacOSX14.5.sdk.tar.xz;
  enableArchs = [ "arm64" "x86_64" ];  # Exclude arm64e, x86_64h
};

# Set minimum deployment target
toolchain = mkOsxcross {
  sdkPath = ./MacOSX14.5.sdk.tar.xz;
  osxVersionMin = "11.0";  # Require macOS 11+
};

# Disable LTO (faster builds, larger binaries)
toolchain = mkOsxcross {
  sdkPath = ./MacOSX14.5.sdk.tar.xz;
  enableLTO = false;
};
```

## Available Tools

The toolchain provides these binaries (prefixed with `<arch>-apple-darwin<version>-`):

### Compilers
- `clang`, `clang++` - C/C++ compilers
- `cc`, `c++` - Aliases for clang/clang++

### Linker & Object Tools
- `ld` - Apple's ld64 linker
- `ar` - Archive tool
- `ranlib` - Archive index generator
- `libtool` - Library tool
- `lipo` - Universal binary creator
- `nm` - Symbol table viewer
- `otool` - Object file viewer
- `strip` - Symbol stripper
- `install_name_tool` - Dynamic library path editor

### Utilities
- `dsymutil` - Debug symbol utility
- `codesign_allocate` - Code signing space allocator
- `vtool` - Version tool
- `xcrun` - Xcode tool runner (simulated)
- `sw_vers` - macOS version info (simulated)
- `pkg-config` - Package configuration tool (arch-prefixed only, see note below)

### Shortcut Prefixes
For convenience, short prefixes are available:
- `o64-clang` → `x86_64-apple-darwin*-clang`
- `oa64-clang` → `arm64-apple-darwin*-clang`
- `o32-clang` → `i386-apple-darwin*-clang` (SDK ≤10.13 only)

### Tool Categories

The flake organizes tools into categories that determine how symlinks are created:

| Category | Unprefixed | Arch-prefixed | Shortcuts | Example |
|----------|------------|---------------|-----------|---------|
| Compiler wrappers | No | Yes | Yes | `clang`, `clang++` |
| Utility tools | Yes | Yes | No | `osxcross`, `xcrun` |
| Arch-only tools | No | Yes | No | `pkg-config` |
| cctools binaries | Yes (common) | Yes | No | `ar`, `ld`, `lipo` |

**Note on pkg-config**: The `pkg-config` wrapper is only available with architecture prefixes (e.g., `arm64-apple-darwin23-pkg-config`) to avoid shadowing the system's `pkg-config` during native Linux builds. This prevents issues when building projects that require both native and cross-compiled dependencies.

## Rust Cross-Compilation

The flake includes helpers for Rust cross-compilation with [rust-overlay](https://github.com/oxalica/rust-overlay) and [crane](https://github.com/ipetkov/crane).

### Setup

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    osxcross.url = "github:tpoechtrager/osxcross";
    rust-overlay.url = "github:oxalica/rust-overlay";
    crane.url = "github:ipetkov/crane";
  };

  outputs = { self, nixpkgs, osxcross, rust-overlay, crane }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {
      inherit system;
      overlays = [ rust-overlay.overlays.default ];
    };

    # Build osxcross toolchain
    toolchain = osxcross.lib.${system}.mkOsxcross {
      sdkPath = ./MacOSX14.5.sdk.tar.xz;
    };

    # Get Rust helpers
    rustHelpers = osxcross.lib.${system}.mkRustHelpers toolchain;

    # Setup Rust with macOS targets
    rustToolchain = pkgs.rust-bin.stable.latest.default.override {
      targets = rustHelpers.rustTargets;  # ["aarch64-apple-darwin" "x86_64-apple-darwin"]
    };

    craneLib = (crane.mkLib pkgs).overrideToolchain rustToolchain;
  in {
    # ... see examples below
  };
}
```

### Rust Helper Functions

#### `rustTargets`
List of Rust target triples supported by the toolchain.
```nix
rustHelpers.rustTargets
# => ["aarch64-apple-darwin" "x86_64-apple-darwin"]
```

#### `cargoEnvFor`
Environment variables for a specific target.
```nix
rustHelpers.cargoEnvFor "aarch64-apple-darwin"
# => {
#   CARGO_TARGET_AARCH64_APPLE_DARWIN_LINKER = "/nix/store/.../bin/arm64-apple-darwin23-clang";
#   CARGO_TARGET_AARCH64_APPLE_DARWIN_AR = "/nix/store/.../bin/arm64-apple-darwin23-ar";
#   CC_aarch64_apple_darwin = "/nix/store/.../bin/arm64-apple-darwin23-clang";
#   CXX_aarch64_apple_darwin = "/nix/store/.../bin/arm64-apple-darwin23-clang++";
#   AR_aarch64_apple_darwin = "/nix/store/.../bin/arm64-apple-darwin23-ar";
# }
```

#### `cargoEnvAll`
Environment variables for all supported targets.

#### `commonEnv`
Common environment variables for cross-compilation.
```nix
rustHelpers.commonEnv
# => {
#   OSXCROSS_TARGET_DIR = "/nix/store/...";
#   OSXCROSS_SDK = "/nix/store/.../SDK/MacOSX.sdk";
#   MACOSX_DEPLOYMENT_TARGET = "10.13";
# }
```

#### `mkCargoConfig`
Generate `.cargo/config.toml` content.
```nix
rustHelpers.mkCargoConfig { }
# => "[target.aarch64-apple-darwin]\nlinker = \"...\"\n..."

# With custom deployment target
rustHelpers.mkCargoConfig { deploymentTarget = "11.0"; }
```

#### `writeCargoConfig`
Write cargo config to a file in the Nix store.
```nix
rustHelpers.writeCargoConfig { }
# => /nix/store/...-cargo-config.toml
```

#### `mkCrossBuilder`
Wrap crane for cross-compilation.
```nix
let
  darwinBuilder = rustHelpers.mkCrossBuilder {
    inherit craneLib;
    target = "aarch64-apple-darwin";
  };
in
  darwinBuilder.buildPackage {
    src = ./.;
    # ... other crane options
  }
```

#### `mkUniversalBinary`
Create universal (fat) binaries from arm64 + x86_64 builds.
```nix
rustHelpers.mkUniversalBinary {
  name = "myapp";
  arm64Drv = arm64Build;
  x86_64Drv = x86Build;
  binaries = [ "myapp" "myapp-cli" ];  # Optional, defaults to [name]
}
```

#### `mkUniversalLibrary`
Create universal libraries.
```nix
rustHelpers.mkUniversalLibrary {
  name = "mylib";
  arm64Drv = arm64Build;
  x86_64Drv = x86Build;
  libraries = [ "libmylib.a" "libmylib.dylib" ];  # Optional
}
```

#### `mkDevShellHook`
Shell hook for development environments.
```nix
devShells.default = pkgs.mkShell {
  shellHook = rustHelpers.mkDevShellHook { };
};
```

### Complete Rust Example

```nix
{
  packages.${system} = {
    # ARM64 build
    myapp-aarch64 = let
      builder = rustHelpers.mkCrossBuilder {
        inherit craneLib;
        target = "aarch64-apple-darwin";
      };
    in builder.buildPackage {
      src = ./.;
      cargoExtraArgs = "--release";
    };

    # x86_64 build
    myapp-x86_64 = let
      builder = rustHelpers.mkCrossBuilder {
        inherit craneLib;
        target = "x86_64-apple-darwin";
      };
    in builder.buildPackage {
      src = ./.;
      cargoExtraArgs = "--release";
    };

    # Universal binary
    myapp-universal = rustHelpers.mkUniversalBinary {
      name = "myapp";
      arm64Drv = self.packages.${system}.myapp-aarch64;
      x86_64Drv = self.packages.${system}.myapp-x86_64;
    };
  };

  devShells.${system}.default = pkgs.mkShell {
    nativeBuildInputs = [ rustToolchain toolchain ];
    shellHook = rustHelpers.mkDevShellHook { };
  };
}
```

## CMake Integration

The toolchain includes CMake integration via a toolchain file.

```bash
# Using the cmake wrapper
./result/bin/osxcross-cmake -B build -S .

# Or with architecture-specific wrapper
./result/bin/arm64-apple-darwin23-cmake -B build -S .

# Manual toolchain file usage
cmake -DCMAKE_TOOLCHAIN_FILE=./result/share/osxcross/toolchain.cmake ...
```

## Supported SDK Versions

| SDK Version | Darwin Target | Architectures | TAPI Required |
|-------------|---------------|---------------|---------------|
| 10.6 - 10.10 | darwin10-14 | i386, x86_64 | No |
| 10.11 - 10.13 | darwin15-17 | i386, x86_64, x86_64h | Yes |
| 10.14 - 10.15 | darwin18-19 | x86_64, x86_64h | Yes |
| 11.x | darwin20.x | arm64, arm64e, x86_64, x86_64h | Yes |
| 12.x | darwin21.x | arm64, arm64e, x86_64, x86_64h | Yes |
| 13.x | darwin22.x | arm64, arm64e, x86_64, x86_64h | Yes |
| 14.x | darwin23.x | arm64, arm64e, x86_64, x86_64h | Yes |
| 15.x | darwin24.x | arm64, arm64e, x86_64, x86_64h | Yes |
| 26.x | darwin25.x | arm64, arm64e, x86_64, x86_64h | Yes |

## Flake Outputs

### Packages
- `packages.<system>.xar` - XAR archive tool
- `packages.<system>.apple-libtapi` - Apple TAPI library
- `packages.<system>.default` - Help message (SDK required for full toolchain)

### Library Functions
- `lib.<system>.mkOsxcross` - Build complete toolchain
- `lib.<system>.mkRustHelpers` - Get Rust cross-compilation helpers
- `lib.<system>.osxcrossLib` - Low-level helper functions

### Dev Shells
- `devShells.<system>.default` - Development environment for working on osxcross

### Overlays
- `overlays.default` - Nixpkgs overlay adding `mkOsxcross` and `mkRustHelpers`

## Toolchain Passthru Attributes

The built toolchain exposes useful attributes via `passthru`:

```nix
toolchain.detectedSdkVersion  # "14.5"
toolchain.darwinTarget        # "darwin23.5"
toolchain.supportedArchs      # ["arm64" "arm64e" "x86_64" "x86_64h"]
toolchain.primaryArch         # "arm64"
toolchain.effectiveOsxVersionMin  # "10.13"

# Sub-derivations
toolchain.sdk          # The extracted SDK
toolchain.cctools-port # Apple cctools
toolchain.wrapper      # Compiler wrapper
toolchain.xar          # XAR tool
toolchain.apple-libtapi  # TAPI library (if needed)

# Helper functions
toolchain.getCompiler "arm64"     # Path to arm64 clang
toolchain.getCompilerCxx "arm64"  # Path to arm64 clang++
toolchain.getAr "arm64"           # Path to arm64 ar
toolchain.getLd "arm64"           # Path to arm64 ld
```

## Obtaining an SDK

macOS SDKs can be extracted from Xcode using the included script:

```bash
# On a Mac with Xcode installed
./tools/gen_sdk_package.sh

# Or from an Xcode .xip file
./tools/gen_sdk_package_pbzx.sh /path/to/Xcode.xip
```

The SDK tarball will be created in the `tarballs/` directory.

## Troubleshooting

### "cannot find clang intrinsic headers"
This warning is normal when using the host system's clang. It doesn't affect functionality.

### "Your clang installation is outdated"
This warning appears with very new SDKs and older version parsing. It's cosmetic and doesn't affect functionality.

### Linker errors about "unrecognised emulation mode"
Ensure you're using the osxcross-provided clang wrappers, not the system clang directly.

### SDK not found
Make sure:
1. The SDK tarball exists at the specified path
2. If the filename doesn't include the version (e.g., `MacOSX.sdk.tar`), provide `sdkVersion` explicitly
3. The tarball contains a directory named `MacOSX.sdk` or `MacOSX<version>.sdk`

### pkg-config not finding packages during cross-compilation
Use the architecture-prefixed pkg-config:
```bash
# Instead of: pkg-config --libs mylib
arm64-apple-darwin23-pkg-config --libs mylib
```

Set `OSXCROSS_PKG_CONFIG_PATH` to point to your MacPorts or cross-compiled library `.pc` files.

### Native Linux builds fail with "wayland-client not found" or similar
If osxcross is in your PATH and native builds fail to find system libraries, this is likely because the system's `pkg-config` is being shadowed. The Nix flake avoids creating an unprefixed `pkg-config` symlink to prevent this. If you're using a manual osxcross installation, ensure you use architecture-prefixed pkg-config for cross-compilation only.

## Architecture

The Nix flake is organized into these components:

```
nix/
├── lib.nix          # Helper functions (SDK detection, symlink generation)
├── osxcross.nix     # Main derivation combining all components
├── sdk.nix          # SDK extraction
├── cctools-port.nix # Apple cctools (ar, ld, lipo, etc.)
├── wrapper.nix      # Compiler wrapper
├── xar.nix          # XAR archive tool
├── apple-libtapi.nix # TAPI library for .tbd files
└── rust.nix         # Rust cross-compilation helpers
```

## License

- OSXCross: GPL-2.0-or-later
- cctools-port: APSL-2.0
- Apple TAPI: Apache-2.0 with LLVM exception
