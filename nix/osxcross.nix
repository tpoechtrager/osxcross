# OSXCross - Main package derivation
# Combines all components into a complete macOS cross-compilation toolchain
{
  lib,
  stdenv,
  callPackage,
  makeWrapper,
  llvmPackages,
  src, # osxcross source
  osxcrossLib,
  macosSdk,
  sdkVersion ? null,
  osxVersionMin ? null,
  enableArchs ? null,
  enableLTO ? true,
}: let
  macosSdkVersion =
    macosSdk.sdkVersion or (throw "osxcross: macosSdk.sdkVersion is required");

  # Auto-detect SDK version if not provided
  detectedSdkVersion =
    if sdkVersion != null
    then
      if sdkVersion == macosSdkVersion
      then sdkVersion
      else throw "osxcross: sdkVersion (${sdkVersion}) does not match macosSdk.sdkVersion (${macosSdkVersion})"
    else macosSdkVersion;

  sdk = macosSdk.sdk or (throw "osxcross: macosSdk.sdk is required");
  sdkRoot = macosSdk.sdkRoot or "${sdk}/MacOSX${detectedSdkVersion}.sdk";

  # Get target info from SDK version
  targetInfo = osxcrossLib.getTargetInfo detectedSdkVersion;

  # Determine architectures to build
  supportedArchs =
    if enableArchs != null
    then
      osxcrossLib.validateArchs {
        inherit enableArchs;
        sdkVersion = detectedSdkVersion;
      }
    else targetInfo.archs;

  # Primary architecture (first in list)
  primaryArch = builtins.head supportedArchs;

  # Darwin target string
  darwinTarget = targetInfo.target;

  # Minimum OS version
  effectiveOsxVersionMin =
    if osxVersionMin != null
    then osxVersionMin
    else targetInfo.minVersion;

  # LTO library path
  libltoPath =
    if enableLTO
    then "${llvmPackages.llvm.lib}/lib"
    else "";

  # Build XAR
  xar = callPackage ./xar.nix {};

  # Build Apple TAPI (if needed)
  apple-libtapi =
    if targetInfo.needsTapi
    then callPackage ./apple-libtapi.nix {}
    else null;

  # Build cctools-port
  cctools-port = callPackage ./cctools-port.nix {
    inherit xar darwinTarget primaryArch enableLTO;
    apple-libtapi = apple-libtapi;
  };

  # Build wrapper
  wrapper = callPackage ./wrapper.nix {
    inherit src darwinTarget supportedArchs libltoPath;
    osxVersionMin = effectiveOsxVersionMin;
  };

  # Get symlink definitions from lib
  wrapperSymlinks = osxcrossLib.getWrapperSymlinks {inherit supportedArchs darwinTarget;};
  cctoolsSymlinks = osxcrossLib.getCctoolsSymlinks {inherit supportedArchs darwinTarget primaryArch;};
  allSymlinks = wrapperSymlinks // cctoolsSymlinks;

  # Generate symlink creation commands
  symlinkCommands = lib.concatStringsSep "\n" (
    lib.mapAttrsToList (name: target: "ln -sf \"${target}\" \"$out/bin/${name}\"") allSymlinks
  );

  # Script contents defined in Nix
  osxcrossCmakeScript = ''
    #!/usr/bin/env bash
    # OSXCross CMake wrapper
    OSXCROSS_TARGET_DIR="$(cd "$(dirname "$0")/.." && pwd)"
    OSXCROSS_HOST="${primaryArch}-apple-${darwinTarget}"
    OSXCROSS_TARGET="${darwinTarget}"
    OSXCROSS_SDK="${sdkRoot}"
    OSXCROSS_SDKROOT="${sdkRoot}"

    export OSXCROSS_TARGET_DIR OSXCROSS_HOST OSXCROSS_TARGET OSXCROSS_SDK OSXCROSS_SDKROOT

    exec cmake \
      -DCMAKE_TOOLCHAIN_FILE="$OSXCROSS_TARGET_DIR/share/osxcross/toolchain.cmake" \
      "$@"
  '';

  archCmakeScript = arch: ''
    #!/usr/bin/env bash
    OSXCROSS_HOST="${arch}-apple-${darwinTarget}"
    export OSXCROSS_HOST
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    exec "$SCRIPT_DIR/osxcross-cmake" "$@"
  '';

  osxcrossEnvScript = ''
    #!/usr/bin/env bash
    # Source this file to set up osxcross environment
    OSXCROSS_TARGET_DIR="$(cd "$(dirname "''${BASH_SOURCE[0]}")/.." && pwd)"
    export PATH="$OSXCROSS_TARGET_DIR/bin:$PATH"
    export OSXCROSS_TARGET_DIR
    export OSXCROSS_SDK="${sdkRoot}"
    export OSXCROSS_SDKROOT="${sdkRoot}"
    export MACOSX_DEPLOYMENT_TARGET="${effectiveOsxVersionMin}"
  '';

  configFileContent = ''
    OSXCROSS_VERSION=1.5
    SDK_VERSION=${detectedSdkVersion}
    DARWIN_TARGET=${darwinTarget}
    OSX_VERSION_MIN=${effectiveOsxVersionMin}
    SUPPORTED_ARCHS=${lib.concatStringsSep " " supportedArchs}
    PRIMARY_ARCH=${primaryArch}
    LTO_ENABLED=${lib.boolToString enableLTO}
  '';

  # Libraries to copy (as a list for clarity)
  libSources =
    [
      {
        src = cctools-port;
        hasLib = true;
      }
      {
        src = xar;
        hasLib = true;
      }
    ]
    ++ lib.optional (apple-libtapi != null) {
      src = apple-libtapi;
      hasLib = true;
      hasInclude = true;
    };

  # Generate library copy commands
  libCopyCommands =
    lib.concatMapStringsSep "\n" (
      entry:
        (lib.optionalString entry.hasLib ''
          if [ -d "${entry.src}/lib" ]; then
            cp -r "${entry.src}"/lib/* "$out/lib/" 2>/dev/null || true
          fi
        '')
        + (lib.optionalString (entry.hasInclude or false) ''
          if [ -d "${entry.src}/include" ]; then
            mkdir -p "$out/include"
            cp -r "${entry.src}"/include/* "$out/include/"
          fi
        '')
    )
    libSources;

  # Generate arch-specific cmake wrapper install commands
  archCmakeInstallCommands =
    lib.concatMapStringsSep "\n" (arch: ''
      cat > "$out/bin/${arch}-apple-${darwinTarget}-cmake" << 'ARCHCMAKE'
      ${archCmakeScript arch}
      ARCHCMAKE
      chmod +x "$out/bin/${arch}-apple-${darwinTarget}-cmake"
    '')
    supportedArchs;
in
  stdenv.mkDerivation {
    pname = "osxcross";
    version = "1.5-sdk${detectedSdkVersion}";

    inherit src;

    nativeBuildInputs = [
      makeWrapper
    ];

    dontBuild = true;
    dontConfigure = true;

    installPhase = ''
      runHook preInstall

      # Create directory structure
      mkdir -p "$out"/{bin,lib,share/osxcross}

      # Copy cctools binaries
      cp -r "${cctools-port}"/bin/* "$out/bin/"

      # Copy libraries
      ${libCopyCommands}

      # Copy and wrap the wrapper binary to include unwrapped clang in PATH
      # We use clang-unwrapped to avoid Nix's cc-wrapper intercepting linker calls
      for wrapperBin in "${wrapper}"/bin/*-wrapper; do
        binName=$(basename "$wrapperBin")
        cp "$wrapperBin" "$out/bin/$binName"
        wrapProgram "$out/bin/$binName" \
          --set-default OSXCROSS_SDK "${sdkRoot}" \
          --set-default OSXCROSS_SDKROOT "${sdkRoot}" \
          --prefix PATH : "${llvmPackages.clang-unwrapped}/bin:$out/bin"
      done

      # Create tool symlinks
      ${symlinkCommands}

      # Install CMake toolchain file
      cp "$src/tools/toolchain.cmake" "$out/share/osxcross/"
      ln -sf "$out/share/osxcross/toolchain.cmake" "$out/toolchain.cmake"

      # Install macports script
      cp "$src/tools/osxcross-macports" "$out/bin/"
      chmod +x "$out/bin/osxcross-macports"
      ln -sf osxcross-macports "$out/bin/osxcross-mp"
      ln -sf osxcross-macports "$out/bin/omp"

      # Install osxcross-cmake wrapper
      cat > "$out/bin/osxcross-cmake" << 'CMAKEWRAPPER'
      ${osxcrossCmakeScript}
      CMAKEWRAPPER
      chmod +x "$out/bin/osxcross-cmake"

      # Install architecture-specific cmake wrappers
      ${archCmakeInstallCommands}

      # Install environment setup script
      cat > "$out/bin/osxcross-env" << 'ENVSCRIPT'
      ${osxcrossEnvScript}
      ENVSCRIPT
      chmod +x "$out/bin/osxcross-env"

      # Store configuration for reference
      cat > "$out/share/osxcross/config" << 'CONFIG'
      ${configFileContent}
      CONFIG

      runHook postInstall
    '';

    # Expose attributes for downstream use
    passthru = {
      inherit
        detectedSdkVersion
        darwinTarget
        supportedArchs
        primaryArch
        effectiveOsxVersionMin
        sdk
        sdkRoot
        macosSdk
        cctools-port
        wrapper
        xar
        apple-libtapi
        ;

      # Helper to get compiler for a specific arch
      getCompiler = arch: "${placeholder "out"}/bin/${arch}-apple-${darwinTarget}-clang";
      getCompilerCxx = arch: "${placeholder "out"}/bin/${arch}-apple-${darwinTarget}-clang++";
      getAr = arch: "${placeholder "out"}/bin/${arch}-apple-${darwinTarget}-ar";
      getLd = arch: "${placeholder "out"}/bin/${arch}-apple-${darwinTarget}-ld";
    };

    meta = with lib; {
      description = "macOS cross-compilation toolchain for Linux";
      longDescription = ''
        OSXCross is a toolchain that allows you to compile macOS binaries
        on Linux. This package includes:
        - Apple cctools (ar, as, ld, lipo, nm, otool, etc.)
        - ld64 linker
        - Compiler wrappers for clang
        - macOS SDK ${detectedSdkVersion}
      '';
      homepage = "https://github.com/tpoechtrager/osxcross";
      license = licenses.gpl2Plus;
      platforms = platforms.linux;
      maintainers = [];
    };
  }
