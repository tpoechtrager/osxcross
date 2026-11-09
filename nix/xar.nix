# XAR archive tool (osxcross fork)
# Required for extracting macOS SDK packages
{
  lib,
  stdenv,
  fetchFromGitHub,
  autoconf,
  automake,
  libtool,
  pkg-config,
  openssl,
  libxml2,
  zlib,
  bzip2,
  xz,
}:
stdenv.mkDerivation rec {
  pname = "xar";
  version = "1.6.1-osxcross";

  src = fetchFromGitHub {
    owner = "tpoechtrager";
    repo = "xar";
    rev = "xar-1.6.1";
    hash = "sha256-vuHuRZhigVYWnpuCmk0ShlWwEEvnGMnQDoMG6/d8UAo=";
  };

  sourceRoot = "${src.name}/xar";

  nativeBuildInputs = [
    autoconf
    automake
    libtool
    pkg-config
  ];

  buildInputs = [
    openssl
    libxml2
    zlib
    bzip2
    xz
  ];

  # Patch configure.ac to work with OpenSSL 3.0+
  # OpenSSL_add_all_ciphers was removed in OpenSSL 3.0
  postPatch = ''
    substituteInPlace configure.ac \
      --replace-fail 'OpenSSL_add_all_ciphers' 'EVP_aes_256_cbc'
  '';

  preConfigure = ''
    ./autogen.sh --noconfigure
    export LDFLAGS="-L${openssl.out}/lib"
    export CPPFLAGS="-I${openssl.dev}/include"
    export LIBS="-lcrypto -lssl"
  '';

  configureFlags = [
    "--with-lzma=${xz.dev}"
    "--with-bzip2=${bzip2.dev}"
    "--with-xml2-config=${libxml2.dev}/bin/xml2-config"
  ];

  # Fix for newer OpenSSL and GCC - allow deprecated API calls and implicit declarations
  env = {
    NIX_CFLAGS_COMPILE = toString [
      "-Wno-error=deprecated-declarations"
      "-Wno-error=implicit-function-declaration"
      "-Wno-error=builtin-declaration-mismatch"
      "-I${openssl.dev}/include"
    ];
    NIX_LDFLAGS = "-L${openssl.out}/lib -lcrypto -lssl";
  };

  # Make warnings non-fatal
  makeFlags = ["CFLAGS=-Wno-error"];

  meta = with lib; {
    description = "Extensible Archiver (osxcross fork)";
    homepage = "https://github.com/tpoechtrager/xar";
    license = licenses.bsd3;
    platforms = platforms.unix;
    maintainers = [];
  };
}
