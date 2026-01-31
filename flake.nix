{
  description = "OSXCross - macOS cross-compilation toolchain for Linux";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };

        # Import the helper library
        osxcrossLib = import ./nix/lib.nix {inherit (pkgs) lib;};

        # Function to build osxcross with configuration
        mkOsxcross = {
          sdkPath,
          sdkVersion ? null,
          osxVersionMin ? null,
          enableArchs ? null,
          enableLTO ? true,
        }:
          pkgs.callPackage ./nix/osxcross.nix {
            inherit osxcrossLib sdkPath sdkVersion osxVersionMin enableArchs enableLTO;
            src = self;
          };

        # Rust helpers factory (requires a built osxcross)
        mkRustHelpers = osxcross:
          import ./nix/rust.nix {
            inherit (pkgs) lib;
            inherit pkgs osxcross;
          };
      in {
        # Package outputs
        packages = {
          # Default package - provides usage instructions
          default = pkgs.writeShellScriptBin "osxcross-help" ''
            cat << 'EOF'
            OSXCross Nix Flake
            ==================

            OSXCross requires a macOS SDK tarball due to Apple licensing.
            You must provide the SDK path when building.

            Usage in a flake:
            -----------------
            {
              inputs.osxcross.url = "github:tpoechtrager/osxcross";

              outputs = { osxcross, ... }: {
                packages.x86_64-linux.myApp = let
                  toolchain = osxcross.lib.x86_64-linux.mkOsxcross {
                    sdkPath = ./MacOSX14.5.sdk.tar.xz;
                  };
                in ...;
              };
            }

            Available options for mkOsxcross:
            ---------------------------------
            - sdkPath      (required) Path to macOS SDK tarball
            - sdkVersion   (optional) SDK version, auto-detected from filename
            - osxVersionMin(optional) Minimum deployment target
            - enableArchs  (optional) List of architectures: ["arm64" "x86_64"]
            - enableLTO    (optional) Enable LTO support (default: true)

            For Rust cross-compilation:
            ---------------------------
            rustHelpers = osxcross.lib.x86_64-linux.mkRustHelpers toolchain;

            See README for more details.
            EOF
          '';

          # Standalone XAR tool
          xar = pkgs.callPackage ./nix/xar.nix {};

          # Standalone Apple TAPI library
          apple-libtapi = pkgs.callPackage ./nix/apple-libtapi.nix {};
        };

        # Development shell for working on osxcross itself
        devShells.default = pkgs.mkShell {
          name = "osxcross-dev";

          buildInputs = with pkgs; [
            # Build tools
            clang
            llvmPackages.llvm
            cmake
            gnumake
            autoconf
            automake
            libtool
            pkg-config

            # Required dependencies
            git
            gnupatch
            python3
            openssl
            xz
            libxml2
            bzip2
            cpio
            zlib
            bash
            libuuid

            # Optional but recommended
            curl
            wget
          ];

          shellHook = ''
            echo "OSXCross development environment"
            echo ""
            echo "To build osxcross manually:"
            echo "  1. Place SDK tarball in ./tarballs/"
            echo "  2. Run: ./build.sh"
            echo ""
            echo "To use Nix flake:"
            echo "  nix build .#mkOsxcross --impure"
            echo ""
          '';
        };

        # Library outputs for use in other flakes
        lib = {
          inherit mkOsxcross mkRustHelpers osxcrossLib;

          # Convenience: get Rust targets for an SDK version
          getRustTargets = osxcrossLib.getRustTargets;

          # Convenience: get target info for an SDK version
          getTargetInfo = osxcrossLib.getTargetInfo;

          # Convenience: detect SDK version from filename
          detectSdkVersion = osxcrossLib.detectSdkVersion;
        };
      }
    )
    // {
      # Overlay for use with nixpkgs overlays
      overlays.default = final: prev: {
        osxcross = {
          mkOsxcross = {
            sdkPath,
            sdkVersion ? null,
            osxVersionMin ? null,
            enableArchs ? null,
            enableLTO ? true,
          }:
            final.callPackage ./nix/osxcross.nix {
              osxcrossLib = import ./nix/lib.nix {inherit (final) lib;};
              inherit sdkPath sdkVersion osxVersionMin enableArchs enableLTO;
              src = self;
            };

          mkRustHelpers = osxcross:
            import ./nix/rust.nix {
              inherit (final) lib;
              pkgs = final;
              inherit osxcross;
            };

          lib = import ./nix/lib.nix {inherit (final) lib;};
        };
      };
    };
}
