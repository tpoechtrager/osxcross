# OSXCross Nix Library
# Helper functions for SDK version detection, architecture mapping, and toolchain configuration
{lib}: let
  # SDK version to darwin target mapping (from build.sh)
  # Maps SDK major.minor version to darwin target info
  sdkVersionMap = {
    # Legacy SDKs (i386 support)
    "10.6" = {
      target = "darwin10";
      archs = ["i386" "x86_64"];
      needsTapi = false;
      minVersion = "10.6";
    };
    "10.7" = {
      target = "darwin11";
      archs = ["i386" "x86_64"];
      needsTapi = false;
      minVersion = "10.6";
    };
    "10.8" = {
      target = "darwin12";
      archs = ["i386" "x86_64" "x86_64h"];
      needsTapi = false;
      minVersion = "10.6";
    };
    "10.9" = {
      target = "darwin13";
      archs = ["i386" "x86_64" "x86_64h"];
      needsTapi = false;
      minVersion = "10.6";
    };
    "10.10" = {
      target = "darwin14";
      archs = ["i386" "x86_64" "x86_64h"];
      needsTapi = false;
      minVersion = "10.6";
    };
    "10.11" = {
      target = "darwin15";
      archs = ["i386" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.6";
    };
    "10.12" = {
      target = "darwin16";
      archs = ["i386" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.6";
    };
    "10.13" = {
      target = "darwin17";
      archs = ["i386" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.6";
    };

    # Modern SDKs (no i386, TAPI required)
    "10.14" = {
      target = "darwin18";
      archs = ["x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "10.15" = {
      target = "darwin19";
      archs = ["x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };

    # Big Sur+ (arm64 support)
    "11" = {
      target = "darwin20.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "11.0" = {
      target = "darwin20.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "11.1" = {
      target = "darwin20.2";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "11.2" = {
      target = "darwin20.3";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "11.3" = {
      target = "darwin20.4";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };

    # Monterey
    "12" = {
      target = "darwin21.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "12.0" = {
      target = "darwin21.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "12.1" = {
      target = "darwin21.2";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "12.2" = {
      target = "darwin21.3";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "12.3" = {
      target = "darwin21.4";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };

    # Ventura
    "13" = {
      target = "darwin22.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "13.0" = {
      target = "darwin22.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "13.1" = {
      target = "darwin22.2";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "13.2" = {
      target = "darwin22.3";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };
    "13.3" = {
      target = "darwin22.4";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.9";
    };

    # Sonoma
    "14" = {
      target = "darwin23";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "14.0" = {
      target = "darwin23";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "14.1" = {
      target = "darwin23.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "14.2" = {
      target = "darwin23.2";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "14.3" = {
      target = "darwin23.3";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "14.4" = {
      target = "darwin23.4";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "14.5" = {
      target = "darwin23.5";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };

    # Sequoia
    "15" = {
      target = "darwin24";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "15.0" = {
      target = "darwin24";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "15.1" = {
      target = "darwin24.1";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "15.2" = {
      target = "darwin24.2";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };

    # macOS 26 (future)
    "26" = {
      target = "darwin25";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
    "26.0" = {
      target = "darwin25";
      archs = ["arm64" "arm64e" "x86_64" "x86_64h"];
      needsTapi = true;
      minVersion = "10.13";
    };
  };

  # Detect SDK version from tarball filename
  # E.g., "MacOSX14.5.sdk.tar.xz" -> "14.5"
  detectSdkVersion = filename: let
    # Remove common extensions
    cleaned = lib.pipe filename [
      (lib.removeSuffix ".tar.xz")
      (lib.removeSuffix ".tar.gz")
      (lib.removeSuffix ".tar.bz2")
      (lib.removeSuffix ".tar")
      (lib.removeSuffix ".sdk")
    ];
    # Match MacOSX followed by version number
    match = builtins.match ".*MacOSX([0-9]+\\.?[0-9]*).*" cleaned;
  in
    if match != null
    then builtins.head match
    else throw "Cannot detect SDK version from filename: ${filename}. Expected format: MacOSX<version>.sdk.tar.xz";

  # Get target info for an SDK version
  getTargetInfo = sdkVersion: let
    # Try exact match first
    exact = sdkVersionMap.${sdkVersion} or null;
    # Try major version only
    majorVersion = builtins.head (lib.splitString "." sdkVersion);
    major = sdkVersionMap.${majorVersion} or null;
  in
    if exact != null
    then exact
    else if major != null
    then major
    else throw "Unsupported SDK version: ${sdkVersion}. Supported versions: ${builtins.concatStringsSep ", " (builtins.attrNames sdkVersionMap)}";

  # Validate architecture list against SDK-supported architectures
  validateArchs = {
    enableArchs,
    sdkVersion,
  }: let
    targetInfo = getTargetInfo sdkVersion;
    supportedArchs = targetInfo.archs;
    invalid = lib.filter (a: !(lib.elem a supportedArchs)) enableArchs;
  in
    if invalid == []
    then enableArchs
    else throw "Unsupported architectures for SDK ${sdkVersion}: ${lib.concatStringsSep ", " invalid}. Supported: ${lib.concatStringsSep ", " supportedArchs}";

  # Map osxcross arch names to Rust target triples
  archToRustTarget = arch:
    if arch == "arm64" || arch == "arm64e"
    then "aarch64-apple-darwin"
    else if arch == "x86_64" || arch == "x86_64h"
    then "x86_64-apple-darwin"
    else if arch == "i386"
    then "i686-apple-darwin"
    else throw "No Rust target for architecture: ${arch}";

  # Map Rust target to osxcross arch
  rustTargetToArch = target:
    if lib.hasPrefix "aarch64" target
    then "aarch64"
    else if lib.hasPrefix "x86_64" target
    then "x86_64"
    else if lib.hasPrefix "i686" target || lib.hasPrefix "i386" target
    then "i386"
    else throw "Unknown Rust target: ${target}";

  # Compiler wrapper tools (linked to main wrapper)
  wrapperTools = [
    "clang"
    "clang++"
    "clang++-libc++"
    "clang++-stdc++"
    "clang++-gstdc++"
    "cc"
    "c++"
  ];

  # Shortcut prefixes for architecture shortcuts
  shortcutPrefixes = {
    "x86_64" = "o64";
    "i386" = "o32";
    "arm64" = "oa64";
    "arm64e" = "oa64e";
    "aarch64" = "oa64";
  };

  # OSXCross utility tools that link to the wrapper (get unprefixed AND arch-prefixed symlinks)
  utilityTools = [
    "osxcross"
    "osxcross-conf"
    "osxcross-cmp"
    "osxcross-man"
    "sw_vers"
    "xcrun"
    "xcodebuild"
    "dsymutil"
  ];

  # Tools that only get arch-prefixed symlinks (no unprefixed version to avoid shadowing system tools)
  # pkg-config is here to avoid interfering with native Linux builds
  archOnlyTools = [
    "pkg-config"
  ];

  # cctools binaries that exist in cctools-port
  cctoolsBinaries = [
    "ar"
    "as"
    "bitcode_strip"
    "check_dylib"
    "checksyms"
    "cmpdylib"
    "codesign_allocate"
    "ctf_insert"
    "dyldinfo"
    "inout"
    "install_name_tool"
    "ld"
    "libtool"
    "lipo"
    "machocheck"
    "makerelocs"
    "mtoc"
    "mtor"
    "nm"
    "nmedit"
    "ObjectDump"
    "otool"
    "pagestuff"
    "ranlib"
    "redo_prebinding"
    "seg_addr_table"
    "seg_hack"
    "segedit"
    "size"
    "strings"
    "strip"
    "unwinddump"
    "vtool"
  ];

  # Generate wrapper symlinks as an attrset: { linkName = targetName; }
  # This returns pure data, not shell commands
  getWrapperSymlinks = {
    supportedArchs,
    darwinTarget,
  }: let
    primaryArch = builtins.head supportedArchs;
    wrapperBin = "${primaryArch}-apple-${darwinTarget}-wrapper";

    # Arch-prefixed symlinks for wrapper tools (clang, clang++, etc.)
    archWrapperLinks = lib.listToAttrs (
      lib.concatMap (
        arch:
          map (tool: {
            name = "${arch}-apple-${darwinTarget}-${tool}";
            value = wrapperBin;
          })
          wrapperTools
      )
      supportedArchs
    );

    # Shortcut symlinks (o64-clang, oa64-clang, etc.)
    shortcutLinks = lib.listToAttrs (
      lib.concatMap (
        arch: let
          prefix = shortcutPrefixes.${arch} or null;
        in
          if prefix != null
          then
            map (tool: {
              name = "${prefix}-${tool}";
              value = wrapperBin;
            })
            wrapperTools
          else []
      )
      supportedArchs
    );

    # Utility symlinks (unprefixed + arch-prefixed)
    utilityLinks = lib.listToAttrs (
      map (tool: {
        name = tool;
        value = wrapperBin;
      })
      utilityTools
    );

    # Arch-prefixed symlinks for utility tools
    archUtilityLinks = lib.listToAttrs (
      lib.concatMap (
        arch:
          map (tool: {
            name = "${arch}-apple-${darwinTarget}-${tool}";
            value = wrapperBin;
          })
          utilityTools
      )
      supportedArchs
    );

    # Arch-only tools (only arch-prefixed, no unprefixed to avoid shadowing system tools)
    archOnlyLinks = lib.listToAttrs (
      lib.concatMap (
        arch:
          map (tool: {
            name = "${arch}-apple-${darwinTarget}-${tool}";
            value = wrapperBin;
          })
          archOnlyTools
      )
      supportedArchs
    );
  in
    archWrapperLinks // shortcutLinks // utilityLinks // archUtilityLinks // archOnlyLinks;

  # Generate cctools symlinks as an attrset: { linkName = targetName; }
  getCctoolsSymlinks = {
    supportedArchs,
    darwinTarget,
    primaryArch,
  }:
    lib.listToAttrs (
      lib.concatMap (
        arch:
          if arch == primaryArch
          then []
          else
            map (tool: {
              name = "${arch}-apple-${darwinTarget}-${tool}";
              value = "${primaryArch}-apple-${darwinTarget}-${tool}";
            })
            cctoolsBinaries
      )
      supportedArchs
    );

  # Convert symlink attrset to shell commands (for backward compatibility during transition)
  symlinkAttrToShell = binDir: symlinks:
    lib.concatStringsSep "\n" (
      lib.mapAttrsToList (name: target: "ln -sf ${target} ${binDir}/${name}") symlinks
    );

  # Legacy functions for backward compatibility
  mkWrapperSymlinks = {
    supportedArchs,
    darwinTarget,
    binDir,
  }:
    symlinkAttrToShell binDir (getWrapperSymlinks {inherit supportedArchs darwinTarget;});

  mkCctoolsSymlinks = {
    supportedArchs,
    darwinTarget,
    binDir,
    primaryArch,
  }:
    symlinkAttrToShell binDir (getCctoolsSymlinks {inherit supportedArchs darwinTarget primaryArch;});
in {
  inherit
    sdkVersionMap
    detectSdkVersion
    getTargetInfo
    validateArchs
    archToRustTarget
    rustTargetToArch
    # New pure-data functions
    wrapperTools
    shortcutPrefixes
    utilityTools
    archOnlyTools
    cctoolsBinaries
    getWrapperSymlinks
    getCctoolsSymlinks
    symlinkAttrToShell
    # Legacy shell-generating functions (still available)
    mkWrapperSymlinks
    mkCctoolsSymlinks
    ;

  # Convenience: get supported Rust targets for an SDK
  getRustTargets = sdkVersion: let
    info = getTargetInfo sdkVersion;
    # Deduplicate (arm64 and arm64e map to same Rust target)
    targets = lib.unique (map archToRustTarget info.archs);
  in
    targets;

  # Get the primary architecture (first in list) for an SDK
  getPrimaryArch = sdkVersion: let
    info = getTargetInfo sdkVersion;
  in
    builtins.head info.archs;
}
