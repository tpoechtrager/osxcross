# cctools-port - Apple's cctools and ld64 for Linux
# Provides: ar, as, ld, lipo, nm, otool, ranlib, strip, etc.
{
  lib,
  stdenv,
  fetchFromGitHub,
  autoreconfHook,
  pkg-config,
  llvmPackages,
  libuuid,
  xar,
  apple-libtapi ? null,
  darwinTarget,
  primaryArch,
  enableLTO ? true,
}:
stdenv.mkDerivation rec {
  pname = "cctools-port";
  version = "1010.6-ld64-951.9";

  src = fetchFromGitHub {
    owner = "tpoechtrager";
    repo = "cctools-port";
    rev = "e79d784d667816e4b15a0abd78828f9abb0a0b99";
    hash = "sha256-5VCNJrRQH5zAsYZQ/PEa82HfK/twULGucF/wHQKQDJM=";
  };

  sourceRoot = "${src.name}/cctools";

  nativeBuildInputs = [
    autoreconfHook
    pkg-config
    llvmPackages.clang
  ];

  buildInputs =
    [
      llvmPackages.llvm
      llvmPackages.libclang
      libuuid
      xar
    ]
    ++ lib.optional (apple-libtapi != null) apple-libtapi;

  # Patches for compatibility with modern LLVM and libtapi 1600+
  postPatch = ''
        # Newer libtool no longer infers a tag for Objective-C sources here.
        # cctools builds them with clang as C-family objects, so tag them as CC.
        substituteInPlace libobjc2/Makefile.am \
          --replace-fail \
            'libobjc_la_CPPFLAGS=' \
            'AM_LIBTOOLFLAGS = --tag=CC

    libobjc_la_CPPFLAGS='

        # Add clone() method to BlobCore class (missing in newer LLVM)
        substituteInPlace ld64/src/ld/code-sign-blobs/blob.h \
          --replace-fail \
            'static BlobCore *readBlob(int fd)			{ return readBlob(fd, 0, 0, 0); }' \
            'static BlobCore *readBlob(int fd)			{ return readBlob(fd, 0, 0, 0); }

    	BlobCore *clone() const {
    		size_t len = length();
    		BlobCore *copy = (BlobCore *)malloc(len);
    		memcpy(copy, this, len);
    		return copy;
    	}'

        # Fix tapi::Platform compatibility for libtapi 1600+
        # The new API uses getPlatformSet() returning uint32_t values instead of tapi::Platform enum
        # Remove the old mapPlatform function and the getPlatform() fallback code
        # The new code path already handles this with getPlatformSet()

        # Remove the mapPlatform function entirely (not needed with new API)
        sed -i '/^static ld::VersionSet mapPlatform/,/^}$/d' ld64/src/ld/parsers/textstub_dylib_file.cpp

        # Remove the TAPI version check and always use the new getPlatformSet() path
        # Replace the conditional block with just the new API code
        substituteInPlace ld64/src/ld/parsers/textstub_dylib_file.cpp \
          --replace-fail \
    '#if ((TAPI_API_VERSION_MAJOR == 1 &&  TAPI_API_VERSION_MINOR >= 6) || (TAPI_API_VERSION_MAJOR > 1))
    	if (tapi::APIVersion::isAtLeast(1, 6)) {
    		for (const auto &platform : file->getPlatformSet())
    			lcPlatforms.insert((ld::Platform)platform);
    	} else
    #endif
    	{
    		lcPlatforms = mapPlatform(file->getPlatform(), useSimulatorVariant());
    	}' \
          'for (const auto &platform : file->getPlatformSet())
    		lcPlatforms.insert((ld::Platform)platform);'
  '';

  configureFlags =
    [
      "--target=${primaryArch}-apple-${darwinTarget}"
      "--with-llvm-config=${llvmPackages.llvm.dev}/bin/llvm-config"
      "--with-libxar=${xar}"
    ]
    ++ lib.optional (apple-libtapi != null) "--with-libtapi=${apple-libtapi}"
    ++ lib.optional (!enableLTO) "--disable-lto-support";

  # Ensure we use clang (required by cctools-port)
  preConfigure = ''
    export CC="${llvmPackages.clang}/bin/clang"
    export CXX="${llvmPackages.clang}/bin/clang++"
    export CFLAGS="-std=gnu17"
  '';

  # Ensure we can find LLVM headers
  env = {
    NIX_CFLAGS_COMPILE = toString [
      "-I${llvmPackages.llvm.dev}/include"
      "-I${llvmPackages.libclang.dev}/include"
    ];
    NIX_LDFLAGS = toString [
      "-L${llvmPackages.llvm.lib}/lib"
    ];
  };

  # Don't strip - we need debug symbols for some tools
  dontStrip = true;

  # Tools to create unprefixed symlinks for
  passthru.commonTools = ["ar" "as" "nm" "otool" "ranlib" "strip" "libtool" "install_name_tool" "lipo"];

  postInstall = let
    # Tools to symlink without arch prefix
    commonTools = ["ar" "as" "nm" "otool" "ranlib" "strip" "libtool" "install_name_tool" "lipo"];
    # Generate symlink commands
    symlinkCommands =
      lib.concatMapStringsSep "\n" (tool: ''
        if [ -f "$out/bin/${primaryArch}-apple-${darwinTarget}-${tool}" ]; then
          ln -sf "${primaryArch}-apple-${darwinTarget}-${tool}" "$out/bin/${tool}"
        fi
      '')
      commonTools;
  in ''
    # Create convenience symlinks for common tools without arch prefix
    ${symlinkCommands}
  '';

  meta = with lib; {
    description = "Apple cctools and ld64 for Linux (and other platforms)";
    homepage = "https://github.com/tpoechtrager/cctools-port";
    license = licenses.apsl20;
    platforms = platforms.unix;
    maintainers = [];
  };
}
