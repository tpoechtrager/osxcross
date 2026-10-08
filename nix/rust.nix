# OSXCross Rust cross-compilation support
# Helpers for integrating osxcross with rust-overlay and crane
{
  lib,
  pkgs,
  osxcross, # The built osxcross toolchain
}: let
  inherit (osxcross) darwinTarget supportedArchs;
  sdkRoot = osxcross.sdkRoot or "${osxcross.sdk}/MacOSX${osxcross.detectedSdkVersion}.sdk";

  # Map osxcross arch to Rust target triple
  archToRustTarget = arch:
    if arch == "arm64" || arch == "aarch64" || arch == "arm64e"
    then "aarch64-apple-darwin"
    else if arch == "x86_64" || arch == "x86_64h"
    then "x86_64-apple-darwin"
    else if arch == "i386"
    then "i686-apple-darwin"
    else throw "No Rust target for architecture: ${arch}";

  # Map Rust target to osxcross arch prefix (used in binary names)
  rustTargetToArch = target:
    if lib.hasPrefix "aarch64" target
    then "arm64" # osxcross uses arm64, not aarch64
    else if lib.hasPrefix "x86_64" target
    then "x86_64"
    else if lib.hasPrefix "i686" target || lib.hasPrefix "i386" target
    then "i386"
    else throw "Unknown Rust target: ${target}";

  # Get unique Rust targets supported by this osxcross build
  rustTargets = lib.unique (map archToRustTarget supportedArchs);

  # Convert Rust target triple to environment variable name format
  # e.g., "aarch64-apple-darwin" -> "AARCH64_APPLE_DARWIN"
  targetToEnvName = target:
    lib.toUpper (lib.replaceStrings ["-"] ["_"] target);

  # Generate Cargo environment variables for a specific Rust target
  cargoEnvFor = rustTarget: let
    arch = rustTargetToArch rustTarget;
    envName = targetToEnvName rustTarget;
    # cc-rs uses lowercase with underscores: aarch64_apple_darwin
    ccEnvName = lib.replaceStrings ["-"] ["_"] rustTarget;
  in {
    # Cargo linker/ar settings
    "CARGO_TARGET_${envName}_LINKER" = "${osxcross}/bin/${arch}-apple-${darwinTarget}-clang";
    "CARGO_TARGET_${envName}_AR" = "${osxcross}/bin/${arch}-apple-${darwinTarget}-ar";

    # cc-rs crate environment variables (uses underscores: CC_aarch64_apple_darwin)
    "CC_${ccEnvName}" = "${osxcross}/bin/${arch}-apple-${darwinTarget}-clang";
    "CXX_${ccEnvName}" = "${osxcross}/bin/${arch}-apple-${darwinTarget}-clang++";
    "AR_${ccEnvName}" = "${osxcross}/bin/${arch}-apple-${darwinTarget}-ar";
  };

  # Generate environment variables for all supported targets
  cargoEnvAll = lib.foldl' (acc: target: acc // (cargoEnvFor target)) {} rustTargets;

  # Common environment variables
  commonEnv = {
    OSXCROSS_TARGET_DIR = "${osxcross}";
    OSXCROSS_SDK = sdkRoot;
    OSXCROSS_SDKROOT = sdkRoot;
    MACOSX_DEPLOYMENT_TARGET = osxcross.effectiveOsxVersionMin;
  };

  # Generate .cargo/config.toml content for cross-compilation
  mkCargoConfig = {deploymentTarget ? osxcross.effectiveOsxVersionMin}: let
    mkTargetSection = rustTarget: let
      arch = rustTargetToArch rustTarget;
    in ''
      [target.${rustTarget}]
      linker = "${osxcross}/bin/${arch}-apple-${darwinTarget}-clang"
      ar = "${osxcross}/bin/${arch}-apple-${darwinTarget}-ar"
      rustflags = ["-C", "link-arg=-mmacosx-version-min=${deploymentTarget}"]
    '';
  in
    lib.concatMapStringsSep "\n" mkTargetSection rustTargets;

  # Write .cargo/config.toml to a file
  writeCargoConfig = {deploymentTarget ? osxcross.effectiveOsxVersionMin}:
    pkgs.writeText "cargo-config.toml" (mkCargoConfig {inherit deploymentTarget;});

  # Wrap a crane library for cross-compilation to a specific target
  mkCrossBuilder = {
    craneLib,
    target,
  }: let
    cargoEnv = cargoEnvFor target;
  in {
    # Build package with cross-compilation
    buildPackage = args:
      craneLib.buildPackage (args
        // cargoEnv
        // {
          cargoExtraArgs = (args.cargoExtraArgs or "") + " --target ${target}";
          nativeBuildInputs = (args.nativeBuildInputs or []) ++ [osxcross];
          # Ensure linker can find osxcross libs
          LD_LIBRARY_PATH = "${osxcross}/lib";
        });

    # Build dependencies only
    buildDepsOnly = args:
      craneLib.buildDepsOnly (args
        // cargoEnv
        // {
          cargoExtraArgs = (args.cargoExtraArgs or "") + " --target ${target}";
          nativeBuildInputs = (args.nativeBuildInputs or []) ++ [osxcross];
          LD_LIBRARY_PATH = "${osxcross}/lib";
        });

    # Clippy check
    cargoClippy = args:
      craneLib.cargoClippy (args
        // cargoEnv
        // {
          cargoClippyExtraArgs = (args.cargoClippyExtraArgs or "") + " --target ${target}";
          nativeBuildInputs = (args.nativeBuildInputs or []) ++ [osxcross];
        });

    # Format check (doesn't need cross-compilation)
    cargoFmt = args: craneLib.cargoFmt args;
  };

  # Create universal (fat) binary from arm64 + x86_64 builds
  mkUniversalBinary = {
    name,
    arm64Drv,
    x86_64Drv,
    binaries ? [name],
    outputPath ? "bin",
  }:
    pkgs.runCommand "${name}-universal" {
      nativeBuildInputs = [osxcross];
    } ''
      mkdir -p $out/${outputPath}

      ${lib.concatMapStringsSep "\n" (bin: ''
          echo "Creating universal binary: ${bin}"
          ${osxcross}/bin/lipo -create \
            -output $out/${outputPath}/${bin} \
            ${arm64Drv}/${outputPath}/${bin} \
            ${x86_64Drv}/${outputPath}/${bin}

          echo "Verifying universal binary:"
          file $out/${outputPath}/${bin}
        '')
        binaries}
    '';

  # Create universal binary from library builds
  mkUniversalLibrary = {
    name,
    arm64Drv,
    x86_64Drv,
    libraries ? ["lib${name}.a" "lib${name}.dylib"],
    outputPath ? "lib",
  }:
    pkgs.runCommand "${name}-universal-lib" {
      nativeBuildInputs = [osxcross];
    } ''
      mkdir -p $out/${outputPath}

      ${lib.concatMapStringsSep "\n" (libFile: ''
          arm64Path="${arm64Drv}/${outputPath}/${libFile}"
          x86Path="${x86_64Drv}/${outputPath}/${libFile}"

          if [ -f "$arm64Path" ] && [ -f "$x86Path" ]; then
            echo "Creating universal library: ${libFile}"
            ${osxcross}/bin/lipo -create \
              -output $out/${outputPath}/${libFile} \
              "$arm64Path" \
              "$x86Path"
          elif [ -f "$arm64Path" ]; then
            echo "Only arm64 available for ${libFile}, copying..."
            cp "$arm64Path" $out/${outputPath}/${libFile}
          elif [ -f "$x86Path" ]; then
            echo "Only x86_64 available for ${libFile}, copying..."
            cp "$x86Path" $out/${outputPath}/${libFile}
          else
            echo "Warning: ${libFile} not found in either build"
          fi
        '')
        libraries}
    '';

  # Development shell setup for Rust cross-compilation
  mkDevShellHook = {deploymentTarget ? osxcross.effectiveOsxVersionMin}: let
    envVars = commonEnv // cargoEnvAll;
    exports = lib.concatStringsSep "\n" (
      lib.mapAttrsToList (name: value: "export ${name}=\"${value}\"") envVars
    );
  in ''
    # OSXCross Rust cross-compilation environment
    export PATH="${osxcross}/bin:$PATH"
    ${exports}

    # Create .cargo/config.toml if it doesn't exist
    if [ ! -f .cargo/config.toml ]; then
      mkdir -p .cargo
      cat > .cargo/config.toml << 'CARGOCONFIG'
    ${mkCargoConfig {inherit deploymentTarget;}}
    CARGOCONFIG
      echo "Created .cargo/config.toml for cross-compilation"
    fi

    echo "OSXCross Rust environment configured"
    echo "  Targets: ${lib.concatStringsSep ", " rustTargets}"
    echo "  SDK: macOS ${osxcross.detectedSdkVersion}"
    echo "  Min deployment: ${deploymentTarget}"
  '';
in {
  # Expose all helpers
  inherit
    rustTargets
    cargoEnvFor
    cargoEnvAll
    commonEnv
    mkCargoConfig
    writeCargoConfig
    mkCrossBuilder
    mkUniversalBinary
    mkUniversalLibrary
    mkDevShellHook
    ;
}
