# Development shell for OSXCross local development
#
# This file provides a Nix development environment with all tools needed
# to work on OSXCross locally.
#
# Usage:
#   nix develop            # Enter the dev shell (uses flake.nix devShells.default)
#
# Formatting:
#   This project uses Alejandra for Nix code formatting.
#   Alejandra is an opinionated Nix formatter that enforces consistent style.
#
#   To format all Nix files:
#     alejandra .
#
#   To format specific files:
#     alejandra flake.nix nix/devshell.nix
#
#   To check formatting without modifying (useful for CI):
#     alejandra --check .
#
#   Alejandra docs: https://github.com/kamadorueda/alejandra
#
{
  pkgs,
  sdkShellHook ? "",
}:
pkgs.mkShell {
  name = "osxcross-dev";

  packages = with pkgs; [
    # Build tools
    clang
    llvmPackages.llvm
    cmake
    gnumake
    autoconf
    automake
    libtool
    pkg-config

    # Required dependencies
    git
    gnupatch
    python3
    openssl
    xz
    libxml2
    bzip2
    cpio
    zlib
    bash
    libuuid

    # Utilities
    curl
    wget

    # Nix formatting - Alejandra is an opinionated Nix formatter
    # Usage: alejandra .          (format all nix files)
    #        alejandra --check .  (check without modifying)
    alejandra

    # Testing (bats for wrapper unit tests)
    bats
  ];

  shellHook = ''
    echo "OSXCross Development Environment"
    echo ""
    ${sdkShellHook}
    echo ""
    echo "Build commands:"
    echo "  ./build.sh              Build toolchain (SDK required in ./tarballs/)"
    echo "  make -C wrapper/        Build the compiler wrapper"
    echo "  make -C wrapper/ clean  Clean wrapper build artifacts"
    echo ""
    echo "Testing:"
    echo "  cd wrapper/unittests && ./run.bats"
    echo ""
    echo "Nix formatting (Alejandra):"
    echo "  alejandra .             Format all Nix files"
    echo "  alejandra --check .     Check formatting without changes"
  '';
}
