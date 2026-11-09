# OSXCross compiler wrapper
# C++ program that wraps clang/gcc for cross-compilation
{
  lib,
  stdenv,
  llvmPackages,
  src, # osxcross source
  osxcrossVersion ? "1.5",
  darwinTarget,
  osxVersionMin,
  linkerVersion ? "951.9",
  supportedArchs,
  libltoPath ? "",
}: let
  primaryArch = builtins.head supportedArchs;
  wrapperBinaryName = "${primaryArch}-apple-${darwinTarget}-wrapper";
in
  stdenv.mkDerivation {
    pname = "osxcross-wrapper";
    version = osxcrossVersion;

    inherit src;
    sourceRoot = "source/wrapper";

    unpackPhase = ''
      mkdir source
      cp -r "$src"/* source/
      chmod -R u+w source
    '';

    nativeBuildInputs = [
      llvmPackages.clang
    ];

    # Build-time constants compiled into the wrapper
    makeFlags = [
      "CXX=${llvmPackages.clang}/bin/clang++"
      "VERSION=${osxcrossVersion}"
      "TARGET=${darwinTarget}"
      "OSX_VERSION_MIN=${osxVersionMin}"
      "LINKER_VERSION=${linkerVersion}"
      "BUILD_DIR="
      "LIBLTO_PATH=${libltoPath}"
      "PLATFORM=Linux"
    ];

    # SUPPORTED_ARCHS contains spaces, so pass via environment
    env = {
      OPTIMIZE = "2";
      LTO = "0"; # Don't use LTO for wrapper itself
      SUPPORTED_ARCHS = lib.concatStringsSep " " supportedArchs;
    };

    installPhase = ''
      runHook preInstall
      mkdir -p "$out/bin"
      cp wrapper "$out/bin/${wrapperBinaryName}"
      runHook postInstall
    '';

    meta = with lib; {
      description = "OSXCross compiler wrapper";
      homepage = "https://github.com/tpoechtrager/osxcross";
      license = licenses.gpl2Plus;
      platforms = platforms.unix;
      maintainers = [];
    };
  }
