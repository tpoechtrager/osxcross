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

        # Get SDK path from env var as fallback
        envSdkPath = builtins.getEnv "MACOS_SDK";

        # Function to build osxcross with configuration
        mkOsxcross = {
          sdkPath ? null,
          sdkVersion ? null,
          osxVersionMin ? null,
          enableArchs ? null,
          enableLTO ? true,
        }: let
          # Manual config (sdkPath argument) takes precedence over env var
          # Convert env var string to a Nix path so the file gets copied into the store
          effectiveSdkPath =
            if sdkPath != null
            then sdkPath
            else if envSdkPath != ""
            then /. + envSdkPath
            else throw "SDK path required: either pass sdkPath argument or set MACOS_SDK environment variable (requires --impure flag)";
        in
          pkgs.callPackage ./nix/osxcross.nix {
            inherit osxcrossLib sdkVersion osxVersionMin enableArchs enableLTO;
            sdkPath = effectiveSdkPath;
            src = self;
          };

        # Rust helpers factory (requires a built osxcross)
        mkRustHelpers = osxcross:
          import ./nix/rust.nix {
            inherit (pkgs) lib;
            inherit pkgs osxcross;
          };

        # Shell hook snippet for SDK detection (reusable by consumers)
        sdkShellHook = ''
          if [ -n "''${MACOS_SDK:-}" ]; then
            echo "SDK: $MACOS_SDK (from MACOS_SDK env var)"
          else
            echo "SDK: not found (set MACOS_SDK env var or pass sdkPath to mkOsxcross)"
          fi
        '';
      in {
        # Package outputs
        packages = {
          # Default package - provides usage instructions
          default = pkgs.writeShellScriptBin "osxcross-help" ''
            cat << 'EOF'
            OSXCross Nix Flake
            ==================

            OSXCross requires a macOS SDK tarball due to Apple licensing.
            You can provide the SDK path via argument or environment variable.

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

            Using MACOS_SDK environment variable:
            -------------------------------------
            export MACOS_SDK=/path/to/MacOSX14.5.sdk.tar.xz
            nix build --impure  # --impure required for env var access

            Available options for mkOsxcross:
            ---------------------------------
            - sdkPath      (optional) Path to macOS SDK tarball
                           Falls back to MACOS_SDK env var if not provided
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
        devShells.default = import ./nix/devshell.nix {inherit pkgs sdkShellHook;};

        # Library outputs for use in other flakes
        lib = {
          inherit mkOsxcross mkRustHelpers osxcrossLib sdkShellHook;

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
      overlays.default = final: prev: let
        envSdkPath = builtins.getEnv "MACOS_SDK";
      in {
        osxcross = {
          mkOsxcross = {
            sdkPath ? null,
            sdkVersion ? null,
            osxVersionMin ? null,
            enableArchs ? null,
            enableLTO ? true,
          }: let
            effectiveSdkPath =
              if sdkPath != null
              then sdkPath
              else if envSdkPath != ""
              then /. + envSdkPath
              else throw "SDK path required: either pass sdkPath argument or set MACOS_SDK environment variable (requires --impure flag)";
          in
            final.callPackage ./nix/osxcross.nix {
              osxcrossLib = import ./nix/lib.nix {inherit (final) lib;};
              inherit sdkVersion osxVersionMin enableArchs enableLTO;
              sdkPath = effectiveSdkPath;
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
