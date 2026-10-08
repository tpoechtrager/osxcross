{
  pkgs,
  osxcross,
}:
pkgs.writeShellApplication {
  name = "realize-macos-sdk";
  runtimeInputs = [
    pkgs.coreutils
    pkgs.nix
  ];
  text = ''
    set -euo pipefail

    usage() {
      cat <<'USAGE'
    Usage:
      realize-macos-sdk [--env] /path/to/MacOSX26.1.sdk.tar.xz 26.1

    Realize a local macOS SDK archive into a stable Nix store output.

    Options:
      --env      Print shell assignments for scripting
      -h, --help Show this help

    Output includes:
      Store path
      SDK root
      SDK version
      Recursive hash
    USAGE
    }

    emit_env() {
      printf 'STORE_PATH=%q\n' "$1"
      printf 'SDK_ROOT=%q\n' "$2"
      printf 'SDK_VERSION=%q\n' "$3"
      printf 'RECURSIVE_HASH=%q\n' "$4"
    }

    mode="human"
    case "''${1:-}" in
      --help|-h)
        usage
        exit 0
        ;;
      --env)
        mode="env"
        shift
        ;;
    esac

    if [ "$#" -ne 2 ]; then
      usage >&2
      exit 64
    fi

    archive="$1"
    sdk_version="$2"

    if [ "''${archive#/}" = "$archive" ]; then
      archive="$(realpath "$archive")"
    fi

    if [ ! -f "$archive" ]; then
      echo "error: SDK archive does not exist: $archive" >&2
      exit 66
    fi

    if [ -z "$sdk_version" ]; then
      echo "error: SDK version must not be empty" >&2
      exit 64
    fi

    sdk_expr="$(cat <<'NIX_EXPR'
    let
      osxcrossFlake = builtins.getFlake (builtins.getEnv "OSXCROSS_FLAKE_PATH");
      system = builtins.currentSystem;
      sdkArchivePath = builtins.getEnv "OSXCROSS_SDK_ARCHIVE";
      sdkVersion = builtins.getEnv "OSXCROSS_SDK_VERSION";
      outputHashValue = builtins.getEnv "OSXCROSS_SDK_OUTPUT_HASH";
      sdkArgs =
        {
          sdkArchive = /. + sdkArchivePath;
          inherit sdkVersion;
        }
        // (
          if outputHashValue == ""
          then {}
          else {outputHash = outputHashValue;}
        );
      osxcrossLib = builtins.getAttr system osxcrossFlake.lib;
      macosSdk =
        if osxcrossLib ? mkMacosSdk
        then osxcrossLib.mkMacosSdk sdkArgs
        else throw "realize-macos-sdk requires an osxcross input that exposes lib.<system>.mkMacosSdk";
    in
      macosSdk.sdk
    NIX_EXPR
    )"

    nix_cmd=(nix --extra-experimental-features "nix-command flakes")

    export OSXCROSS_FLAKE_PATH="${osxcross.outPath}"
    export OSXCROSS_SDK_ARCHIVE="$archive"
    export OSXCROSS_SDK_VERSION="$sdk_version"
    export OSXCROSS_SDK_OUTPUT_HASH=""

    echo "Building bootstrap SDK from local archive..." >&2
    bootstrap_out="$(
      "''${nix_cmd[@]}" build --impure --no-link --print-out-paths --expr "$sdk_expr"
    )"

    recursive_hash="$("''${nix_cmd[@]}" hash path "$bootstrap_out")"

    export OSXCROSS_SDK_OUTPUT_HASH="$recursive_hash"
    echo "Rebuilding fixed-output SDK with recursive hash..." >&2
    final_out="$(
      "''${nix_cmd[@]}" build --impure --no-link --print-out-paths --expr "$sdk_expr"
    )"

    sdk_root="$final_out/MacOSX$sdk_version.sdk"

    if [ ! -d "$sdk_root" ]; then
      echo "error: expected SDK root was not produced: $sdk_root" >&2
      exit 70
    fi

    if [ "$mode" = "env" ]; then
      emit_env "$final_out" "$sdk_root" "$sdk_version" "$recursive_hash"
      exit 0
    fi

    cat <<EOF
    macOS SDK realized.

    Store path:
    $final_out

    SDK root:
    $sdk_root

    SDK version:
    $sdk_version

    Recursive hash:
    $recursive_hash
    EOF
  '';
}
