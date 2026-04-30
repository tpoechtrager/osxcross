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
          sdkVersion ? null,
          osxVersionMin ? null,
          enableArchs ? null,
          enableLTO ? true,
        }: let
          effectiveMacosSdk =
            if macosSdk != null
            then normalizeMacosSdk macosSdk
            else throw "osxcross: mkOsxcross requires macosSdk; SDK discovery belongs in a higher-level policy layer";
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
          if [ -n "''${OSXCROSS_SDKROOT:-}" ]; then
            echo "SDK: $OSXCROSS_SDKROOT (from OSXCROSS_SDKROOT)"
          else
            echo "SDK: not configured (pass macosSdk to mkOsxcross)"
          fi
        '';

        realizeMacosSdk = import ./nix/realize-macos-sdk.nix {
          inherit pkgs;
          osxcross = self;
        };

        fakeSdkRoot = pkgs.runCommand "fake-MacOSX26.1.sdk" {} ''
          mkdir -p "$out/usr/include/c++/v1"
          cat > "$out/SDKSettings.json" <<'JSON'
          {"Version":"26.1"}
          JSON
        '';

        fakeSdkArchive = pkgs.runCommand "fake-MacOSX26.1.sdk.tar" {
          nativeBuildInputs = [pkgs.gnutar];
        } ''
          mkdir -p MacOSX26.1.sdk/usr/include/c++/v1
          cat > MacOSX26.1.sdk/SDKSettings.json <<'JSON'
          {"Version":"26.1"}
          JSON
          tar cf "$out" MacOSX26.1.sdk
        '';

        fakeMacosSdk = mkMacosSdkRef {
          sdk = fakeSdkRoot;
          sdkRoot = fakeSdkRoot;
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

            OSXCross requires a macOS SDK due to Apple licensing.
            Pass an explicit macOS SDK ref to mkOsxcross.

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

            Available options for mkOsxcross:
            ---------------------------------
            - macosSdk     (required) SDK ref from mkMacosSdk or mkMacosSdkRef
            - sdkVersion   (optional) SDK version override
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
              test ! -e "${fakeMacosSdk.sdk}/MacOSX.sdk"
              test ! -L "${fakeMacosSdk.sdk}/MacOSX.sdk"
              test ! -e "${fakeMacosSdk.sdk}/MacOSX26.sdk"
              test ! -L "${fakeMacosSdk.sdk}/MacOSX26.sdk"
              test ! -e "${fakeMacosSdk.sdk}/default"
              test ! -L "${fakeMacosSdk.sdk}/default"
              touch "$out"
            '';

            macos-sdk-ref-direct-root =
              assert fakeMacosSdk.sdkRoot == toString fakeSdkRoot;
                pkgs.runCommand "check-macos-sdk-ref-direct-root" {} "touch $out";

            mkosxcross-requires-macos-sdk = let
              missingSdk = builtins.tryEval ((mkOsxcross {
                  enableArchs = ["x86_64"];
                  enableLTO = false;
                })
                .drvPath);
            in
              assert missingSdk.success == false;
                pkgs.runCommand "check-mkosxcross-requires-macos-sdk" {} "touch $out";

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
            sdkVersion ? null,
            osxVersionMin ? null,
            enableArchs ? null,
            enableLTO ? true,
          }: let
            effectiveMacosSdk =
              if macosSdk != null
              then normalizeMacosSdk macosSdk
              else throw "osxcross: mkOsxcross requires macosSdk; SDK discovery belongs in a higher-level policy layer";
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
