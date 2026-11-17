# Apple TAPI library
# Required for SDK >= 10.11 to handle text-based API stubs (.tbd files)
{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  ninja,
  python3,
  bash,
  coreutils,
  gnugrep,
  gnused,
  llvmPackages,
}:
stdenv.mkDerivation rec {
  pname = "apple-libtapi";
  version = "1600.0.11.8";

  src = fetchFromGitHub {
    owner = "tpoechtrager";
    repo = "apple-libtapi";
    # Latest commit with stdint.h fixes for modern compilers
    rev = "aed9334283e3e290bba622ee980bde2322e4d516";
    hash = "sha256-+iuZ8hbH/2yWF+Km4ktXyjRcofxYMPxe43IGp8WdTog=";
  };

  nativeBuildInputs = [
    cmake
    ninja
    python3
    bash
    coreutils
    gnugrep
    gnused
    llvmPackages.clang
  ];

  buildInputs = [
    llvmPackages.llvm
    llvmPackages.libclang
  ];

  # Patch shebangs in all scripts
  postPatch = ''
    patchShebangs .
  '';

  # Use the project's build.sh but with proper environment
  dontUseCmakeConfigure = true;

  buildPhase = ''
    runHook preBuild

    export INSTALLPREFIX=$out
    export CC="${llvmPackages.clang}/bin/clang"
    export CXX="${llvmPackages.clang}/bin/clang++"

    # Run the project's build script
    bash ./build.sh

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    bash ./install.sh

    runHook postInstall
  '';

  meta = with lib; {
    description = "Apple's TAPI library for handling .tbd stub files";
    homepage = "https://github.com/tpoechtrager/apple-libtapi";
    license = licenses.asl20;
    platforms = platforms.unix;
    maintainers = [];
  };
}
