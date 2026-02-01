# macOS SDK extraction
# Extracts and prepares the macOS SDK for use with osxcross
{
  lib,
  stdenv,
  gnutar,
  xz,
  gzip,
  bzip2,
  cpio,
  jq,
  pbzx ? null, # Optional: for .xip extraction
  sdkTarball,
  sdkVersion,
}: let
  # Determine archive type and extraction command based on file extension
  tarballName = builtins.baseNameOf (toString sdkTarball);

  archiveInfo =
    if lib.hasSuffix ".tar.xz" tarballName
    then {
      type = "tar.xz";
      extractCmd = "${xz}/bin/xz -dc ${sdkTarball} | ${gnutar}/bin/tar xf -";
    }
    else if lib.hasSuffix ".tar.gz" tarballName || lib.hasSuffix ".tgz" tarballName
    then {
      type = "tar.gz";
      extractCmd = "${gzip}/bin/gzip -dc ${sdkTarball} | ${gnutar}/bin/tar xf -";
    }
    else if lib.hasSuffix ".tar.bz2" tarballName || lib.hasSuffix ".tbz2" tarballName
    then {
      type = "tar.bz2";
      extractCmd = "${bzip2}/bin/bzip2 -dc ${sdkTarball} | ${gnutar}/bin/tar xf -";
    }
    else if lib.hasSuffix ".tar" tarballName
    then {
      type = "tar";
      extractCmd = "${gnutar}/bin/tar xf ${sdkTarball}";
    }
    else throw "Unknown archive format for SDK tarball: ${tarballName}. Supported formats: .tar.xz, .tar.gz, .tgz, .tar.bz2, .tbz2, .tar";

  # SDK directory name based on version (what we expect to find after extraction)
  expectedSdkName = "MacOSX${sdkVersion}.sdk";

  # Possible locations where the SDK might be found after extraction
  # Listed in order of preference
  sdkSearchPaths = [
    "MacOSX*.sdk"
    "SDKs/MacOSX*.sdk"
    "Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX*.sdk"
    "Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk"
  ];
in
  stdenv.mkDerivation {
    pname = "macosx-sdk";
    version = sdkVersion;

    src = sdkTarball;

    nativeBuildInputs =
      [
        gnutar
        xz
        gzip
        bzip2
        cpio
        jq
      ]
      ++ lib.optional (pbzx != null) pbzx;

    # Don't try to unpack automatically
    dontUnpack = true;

    buildPhase = ''
      runHook preBuild
      echo "Extracting ${archiveInfo.type} archive..."
      ${archiveInfo.extractCmd}
      runHook postBuild
    '';

    installPhase = let
      # Generate the search logic for SDK directory
      searchScript =
        lib.concatMapStringsSep "\n" (pattern: ''
          if [ -z "$sdkSource" ]; then
            for dir in ${pattern}; do
              if [ -d "$dir" ]; then
                sdkSource="$dir"
                break
              fi
            done
          fi
        '')
        sdkSearchPaths;
    in ''
      runHook preInstall

      mkdir -p $out

      # Find the SDK directory
      sdkSource=""
      ${searchScript}

      if [ -z "$sdkSource" ]; then
        echo "ERROR: Cannot find SDK directory"
        echo "Contents of extraction:"
        find . -maxdepth 3 -type d
        exit 1
      fi

      echo "Found SDK at: $sdkSource"
      sdkName=$(basename "$sdkSource")

      # Validate SDK version from SDKSettings.json
      settingsFile="$sdkSource/SDKSettings.json"
      if [ -f "$settingsFile" ]; then
        actualVersion=$(jq -r '.Version' "$settingsFile")
        expectedVersion="${sdkVersion}"

        if [ "$actualVersion" != "$expectedVersion" ]; then
          echo "ERROR: SDK version mismatch!"
          echo "  Expected (from filename): $expectedVersion"
          echo "  Actual (from SDKSettings.json): $actualVersion"
          echo ""
          echo "If the tarball was renamed, pass the correct version:"
          echo "  mkOsxcross { sdkPath = ...; sdkVersion = \"$actualVersion\"; }"
          exit 1
        fi
        echo "SDK version verified: $actualVersion"
      else
        echo "WARNING: SDKSettings.json not found, cannot verify SDK version"
        actualVersion="${sdkVersion}"
      fi

      # Move SDK to output
      mv "$sdkSource" "$out/$sdkName"

      # Write the verified version to output for reference
      echo "$actualVersion" > "$out/sdk-version"

      # Apply SDK fixups (from build.sh)
      # Remove problematic libcxx.imp file that can cause build issues
      rm -f "$out/$sdkName/usr/include/c++/v1/libcxx.imp" || true

      # Fix permissions
      chmod -R u+w "$out/$sdkName" || true

      # Create version symlinks for convenience
      ln -sf "$sdkName" "$out/${expectedSdkName}" || true
      ln -sf "$sdkName" "$out/MacOSX.sdk"
      ln -sf "$sdkName" "$out/default"

      runHook postInstall
    '';

    # Don't run fixup - SDK should remain as-is
    dontFixup = true;

    meta = with lib; {
      description = "macOS ${sdkVersion} SDK";
      license = licenses.unfree; # Apple SDK license
      platforms = platforms.all;
      maintainers = [];
    };
  }
