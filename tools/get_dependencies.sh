#!/bin/sh
#
# auto-install dependency packages using the systems package manager.
# this assumes you are running as root or are using sudo
#

# Install the union of stable/latest, LLVM-flavor, and optional dependencies.
get_fedora_deps()
{
 yum install \
  bash clang cmake python3 git make patch sed tar gzip xz bzip2 cpio \
  libxml2-devel libuuid-devel openssl-devel zlib-devel xz-devel \
  bzip2-devel llvm llvm-devel lld libstdc++-static
}

get_freebsd_deps()
{
 pkg install \
  bash cmake python3 git gmake patch gsed gtar gzip bzip2 gcpio \
  libxml2 libuuid openssl llvm llvm-devel
}

get_netbsd_deps()
{
 pkgin install \
  bash clang cmake python313 git-base gmake patch gsed gtar-base gzip xz \
  bzip2 gcpio libxml2 libuuid openssl zlib llvm lld
}

get_opensuse_deps()
{
 zypper install \
  bash clang clang-devel cmake python3 git make patch sed tar gzip xz \
  bzip2 cpio libxml2-devel libuuid-devel openssl libopenssl-devel \
  zlib-devel xz-devel libbz2-devel llvm llvm-devel lld \
  llvm-clang llvm-clang-devel libclang13
}

get_mageia_deps()
{
 urpmi \
  ctags bash clang cmake python3 git make patch sed tar gzip xz bzip2 cpio \
  task-c-devel task-c++-devel libstdc++-devel libxml2-devel \
  libuuid-devel openssl libopenssl-devel zlib-devel xz-devel bzip2-devel \
  llvm lib64llvm-devel lld
}

get_debian_deps()
{
 apt-get install -y \
  bash clang cmake python3 git make patch sed tar gzip xz-utils bzip2 cpio \
  libxml2-dev uuid-dev libssl-dev zlib1g-dev liblzma-dev libbz2-dev \
  llvm llvm-dev lld
}

get_arch_deps()
{
 pacman -S \
  bash clang cmake python git make patch sed tar gzip xz bzip2 cpio \
  libxml2 openssl zlib util-linux-libs llvm lld
}

get_arch_uuid_deps()
{
 ARCH_UUID_BUILD_USER=${SUDO_USER-}

 if [ -z "$ARCH_UUID_BUILD_USER" ]; then
  ARCH_UUID_BUILD_USER=$(stat -c %U .) || return 1
 fi

 if [ "$ARCH_UUID_BUILD_USER" = root ]; then
  echo "Cannot build the AUR uuid package as root." >&2
  return 1
 fi

 (
  ARCH_UUID_BUILD_DIR=$(sudo -u "$ARCH_UUID_BUILD_USER" -- \
   mktemp -d "${TMPDIR:-/tmp}/osxcross-uuid.XXXXXXXXXX") || exit 1

  cleanup_arch_uuid_build()
  {
   case "$ARCH_UUID_BUILD_DIR" in
    ?*/osxcross-uuid.*)
     rm -rf -- "$ARCH_UUID_BUILD_DIR"
     ;;
   esac
  }

  trap cleanup_arch_uuid_build 0
  trap 'exit 1' 1 2 15

  sudo -u "$ARCH_UUID_BUILD_USER" -- sh -c '
   git clone https://aur.archlinux.org/uuid.git "$1" &&
   cd "$1" &&
   makepkg -srci
  ' sh "$ARCH_UUID_BUILD_DIR/uuid"
 )
}

get_all_arch_deps()
{
 echo "Running pacman to install dependencies..."
 get_arch_deps || return 1
 echo "Downloading and Installing uuid..."
 get_arch_uuid_deps
}

unknown()
{
 echo "Unknown system type. Please get dependencies by hand "
 echo "following README.md. Or update get_dependencies.sh and submit a patch."
 return 1
}

get_linux_deps_from_issue()
{
 if grep -E -i -q 'ubuntu|debian|raspbian|mint' /etc/issue; then
  get_debian_deps
 elif grep -i -q suse /etc/issue; then
  get_opensuse_deps
 elif grep -E -i -q 'fedora|red[[:space:]]*hat' /etc/issue; then
  get_fedora_deps
 elif grep -i -q mageia /etc/issue; then
  get_mageia_deps
 elif grep -i -q arch /etc/issue; then
  get_all_arch_deps
 else
  unknown
 fi
}

get_linux_deps()
{
 distro=

 if [ -r /etc/os-release ]; then
  ID=
  ID_LIKE=
  . /etc/os-release
  distro="${ID-} ${ID_LIKE-}"
 fi

 case "$distro" in
  *debian*|*ubuntu*|*raspbian*|*linuxmint*|*mint*)
   get_debian_deps
   ;;
  *suse*)
   get_opensuse_deps
   ;;
  *fedora*|*rhel*|*redhat*|*centos*|*rocky*|*almalinux*)
   get_fedora_deps
   ;;
  *mageia*)
   get_mageia_deps
   ;;
  *arch*)
   get_all_arch_deps
   ;;
  *)
   if [ -r /etc/issue ]; then
    get_linux_deps_from_issue
   else
    unknown
   fi
   ;;
 esac
}

case "$(uname -s)" in
 Linux)
  get_linux_deps
  ;;
 FreeBSD)
  get_freebsd_deps
  ;;
 NetBSD)
  get_netbsd_deps
  ;;
 *)
  unknown
  ;;
esac
