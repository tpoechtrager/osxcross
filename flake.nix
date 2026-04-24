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

        isMacosSdkRef = value:
          builtins.isAttrs value
          && value ? _type
          && value._type == "osxcross-macos-sdk"
          && value ? sdk
          && value ? sdkRoot
          && value ? sdkVersion;

        mkMacosSdkRef = {
          sdk,
          sdkVersion,
          sdkRoot ? "${sdk}/MacOSX${sdkVersion}.sdk",
        }:
          assert sdkVersion != null && builtins.isString sdkVersion
            || throw "osxcross: mkMacosSdkRef 'sdkVersion' must be a string";
          assert builtins.stringLength sdkVersion > 0
            || throw "osxcross: mkMacosSdkRef 'sdkVersion' must not be empty";
          {
            _type = "osxcross-macos-sdk";
            inherit sdk sdkVersion;
            sdkRoot = toString sdkRoot;
          };

        mkMacosSdk = {
          sdkArchive,
          sdkVersion ? null,
          outputHash ? null,
        }: let
          detectedSdkVersion =
            if sdkVersion != null
            then sdkVersion
            else osxcrossLib.detectSdkVersion (builtins.baseNameOf (toString sdkArchive));
          sdk = pkgs.callPackage ./nix/sdk.nix {
            sdkTarball = sdkArchive;
            sdkVersion = detectedSdkVersion;
            inherit outputHash;
          };
        in
          mkMacosSdkRef {
            inherit sdk;
            sdkVersion = detectedSdkVersion;
          };

        normalizeMacosSdk = macosSdk:
          if isMacosSdkRef macosSdk
          then macosSdk
          else throw "osxcross: expected a macOS SDK ref from mkMacosSdk or mkMacosSdkRef";

        # Function to build osxcross with configuration
        mkOsxcross = {
          macosSdk ? null,
          sdkPath ? null,
          sdkVersion ? null,
          osxVersionMin ? null,
          enableArchs ? null,
          enableLTO ? true,
        }: let
          # macosSdk is preferred. Legacy sdkPath takes precedence over env var.
          # Convert env var string to a Nix path so the file gets copied into the store.
          effectiveSdkPath =
            if sdkPath != null
            then sdkPath
            else if envSdkPath != ""
            then /. + envSdkPath
            else null;

          effectiveMacosSdk =
            if macosSdk != null
            then normalizeMacosSdk macosSdk
            else if effectiveSdkPath != null
            then
              mkMacosSdk {
                sdkArchive = effectiveSdkPath;
                inherit sdkVersion;
              }
            else throw "SDK required: pass macosSdk, pass sdkPath, or set MACOS_SDK (legacy impure fallback)";
        in
          pkgs.callPackage ./nix/osxcross.nix {
            inherit osxcrossLib sdkVersion osxVersionMin enableArchs enableLTO;
            macosSdk = effectiveMacosSdk;
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
            echo "SDK: not found (pass macosSdk, pass sdkPath, or set MACOS_SDK for legacy impure fallback)"
          fi
        '';

        realizeMacosSdk = import ./nix/realize-macos-sdk.nix {
          inherit pkgs;
          osxcross = self;
        };

        fakeSdkArchive = pkgs.runCommand "MacOSX26.1.sdk.tar" {
          nativeBuildInputs = [pkgs.gnutar];
        } ''
          mkdir -p MacOSX26.1.sdk/usr/include/c++/v1
          cat > MacOSX26.1.sdk/SDKSettings.json <<'JSON'
          {"Version":"26.1"}
          JSON
          tar cf "$out" MacOSX26.1.sdk
        '';

        fakeMacosSdk = mkMacosSdk {
          sdkArchive = fakeSdkArchive;
          sdkVersion = "26.1";
        };

        fakeToolchain = mkOsxcross {
          macosSdk = fakeMacosSdk;
          enableArchs = ["x86_64"];
          enableLTO = false;
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
            Prefer a shared macOS SDK ref. Legacy sdkPath and MACOS_SDK are still supported.

            One-time SDK realization:
            -------------------------
            nix run .#realize-macos-sdk -- /path/to/MacOSX14.5.sdk.tar.xz 14.5

            Usage in a flake:
            -----------------
            {
              inputs.osxcross.url = "github:tpoechtrager/osxcross";
              inputs.macos-sdk-archive = {
                url = "file:///path/to/MacOSX14.5.sdk.tar.xz";
                flake = false;
              };

              outputs = { osxcross, macos-sdk-archive, ... }: {
                packages.x86_64-linux.myApp = let
                  macosSdk = osxcross.lib.x86_64-linux.mkMacosSdk {
                    sdkArchive = macos-sdk-archive;
                    sdkVersion = "14.5";
                    # outputHash = "sha256-...";
                  };
                  toolchain = osxcross.lib.x86_64-linux.mkOsxcross {
                    inherit macosSdk;
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
            - macosSdk     (preferred) SDK ref from mkMacosSdk or mkMacosSdkRef
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

          # Realize a stable macOS SDK store path from a local archive
          realize-macos-sdk = realizeMacosSdk;
        };

        apps.realize-macos-sdk = {
          type = "app";
          program = "${realizeMacosSdk}/bin/realize-macos-sdk";
        };

        # Development shell for working on osxcross itself
        devShells.default = import ./nix/devshell.nix {inherit pkgs sdkShellHook;};

        checks =
          {
            macos-sdk-no-aliases = pkgs.runCommand "check-macos-sdk-no-aliases" {} ''
              test -d "${fakeMacosSdk.sdkRoot}"
              for path in \
                "${fakeMacosSdk.sdk}/MacOSX.sdk" \
                "${fakeMacosSdk.sdk}/MacOSX26.sdk" \
                "${fakeMacosSdk.sdk}/default"
              do
                test ! -e "$path"
                test ! -L "$path"
              done
              touch "$out"
            '';

            realize-macos-sdk-help = pkgs.runCommand "check-realize-macos-sdk-help" {} ''
              "${realizeMacosSdk}/bin/realize-macos-sdk" --help > help
              grep 'realize-macos-sdk \[--env\]' help
              grep 'Recursive hash' help
              grep -- '--env' help
              touch "$out"
            '';
          }
          // pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
            osxcross-direct-sdk-root = pkgs.runCommand "check-osxcross-direct-sdk-root" {} ''
              test ! -e "${fakeToolchain}/SDK"
              test ! -L "${fakeToolchain}/SDK"
              test "$("${fakeToolchain}/bin/xcrun" -show-sdk-path)" = "${fakeMacosSdk.sdkRoot}"
              "${fakeToolchain}/bin/osxcross-conf" | grep 'export OSXCROSS_SDK="${fakeMacosSdk.sdkRoot}"'
              touch "$out"
            '';
          };

        # Library outputs for use in other flakes
        lib = {
          inherit
            mkMacosSdk
            mkMacosSdkRef
            isMacosSdkRef
            mkOsxcross
            mkRustHelpers
            osxcrossLib
            sdkShellHook
            ;

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
        osxcrossLib = import ./nix/lib.nix {inherit (final) lib;};

        isMacosSdkRef = value:
          builtins.isAttrs value
          && value ? _type
          && value._type == "osxcross-macos-sdk"
          && value ? sdk
          && value ? sdkRoot
          && value ? sdkVersion;

        mkMacosSdkRef = {
          sdk,
          sdkVersion,
          sdkRoot ? "${sdk}/MacOSX${sdkVersion}.sdk",
        }:
          assert sdkVersion != null && builtins.isString sdkVersion
            || throw "osxcross: mkMacosSdkRef 'sdkVersion' must be a string";
          assert builtins.stringLength sdkVersion > 0
            || throw "osxcross: mkMacosSdkRef 'sdkVersion' must not be empty";
          {
            _type = "osxcross-macos-sdk";
            inherit sdk sdkVersion;
            sdkRoot = toString sdkRoot;
          };

        mkMacosSdk = {
          sdkArchive,
          sdkVersion ? null,
          outputHash ? null,
        }: let
          detectedSdkVersion =
            if sdkVersion != null
            then sdkVersion
            else osxcrossLib.detectSdkVersion (builtins.baseNameOf (toString sdkArchive));
          sdk = final.callPackage ./nix/sdk.nix {
            sdkTarball = sdkArchive;
            sdkVersion = detectedSdkVersion;
            inherit outputHash;
          };
        in
          mkMacosSdkRef {
            inherit sdk;
            sdkVersion = detectedSdkVersion;
          };

        normalizeMacosSdk = macosSdk:
          if isMacosSdkRef macosSdk
          then macosSdk
          else throw "osxcross: expected a macOS SDK ref from mkMacosSdk or mkMacosSdkRef";
      in {
        osxcross = {
          inherit mkMacosSdk mkMacosSdkRef isMacosSdkRef;

          mkOsxcross = {
            macosSdk ? null,
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
              else null;

            effectiveMacosSdk =
              if macosSdk != null
              then normalizeMacosSdk macosSdk
              else if effectiveSdkPath != null
              then
                mkMacosSdk {
                  sdkArchive = effectiveSdkPath;
                  inherit sdkVersion;
                }
              else throw "SDK required: pass macosSdk, pass sdkPath, or set MACOS_SDK (legacy impure fallback)";
          in
            final.callPackage ./nix/osxcross.nix {
              inherit osxcrossLib;
              inherit sdkVersion osxVersionMin enableArchs enableLTO;
              macosSdk = effectiveMacosSdk;
              src = self;
            };

          mkRustHelpers = osxcross:
            import ./nix/rust.nix {
              inherit (final) lib;
              pkgs = final;
              inherit osxcross;
            };

          lib = osxcrossLib;
        };
      };
    };
}
