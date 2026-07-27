#!/usr/bin/env bash
set -Eeuo pipefail
export LC_ALL=C LANG=C TZ=UTC
umask 022

SOURCE="${1:?usage: build-winegdk-container.sh SOURCE PREFIX COMMIT}"
PREFIX="${2:?usage: build-winegdk-container.sh SOURCE PREFIX COMMIT}"
COMMIT="${3:?usage: build-winegdk-container.sh SOURCE PREFIX COMMIT}"
BUILD=/winegdk/build
readonly GLIBC_CEILING=2.31

[[ "$(id -u)" == 0 ]] || {
  echo "This script must run as root in the Bullseye container." >&2
  exit 1
}
git config --global --add safe.directory "$SOURCE"
[[ "$(git -C "$SOURCE" rev-parse HEAD)" == "$COMMIT" ]] || {
  echo "Source checkout does not match $COMMIT." >&2
  exit 1
}

"$SOURCE/.engine/pin-apt-snapshot.sh" bullseye
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends \
  build-essential ca-certificates bison flex gettext git pkg-config \
  python3-minimal gcc-mingw-w64-i686 gcc-mingw-w64-x86-64 \
  libasound2-dev libdbus-1-dev libegl1-mesa-dev libfontconfig1-dev \
  libfreetype6-dev libgl1-mesa-dev libgnutls28-dev libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev libkrb5-dev libpcap-dev libpulse-dev \
  libsdl2-dev libudev-dev libunwind-dev libusb-1.0-0-dev libvulkan-dev \
  libwayland-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxfixes-dev libxft-dev libxi-dev libxinerama-dev libxkbcommon-dev \
  libxrandr-dev libxrender-dev libxxf86vm-dev wayland-protocols >/dev/null

mkdir -p "$BUILD" "$PREFIX"
SOURCE_DATE_EPOCH="$(git -C "$SOURCE" show -s --format=%ct "$COMMIT")"
export SOURCE_DATE_EPOCH
(
  cd "$BUILD"
  "$SOURCE/configure" \
    --enable-archs=i386,x86_64 \
    --disable-tests \
    --prefix="$PREFIX"
  make -j"$(nproc)"
  make install
)
rm -rf "$PREFIX/include"

libunwind="$(readlink -f /usr/lib/x86_64-linux-gnu/libunwind.so.8)"
mkdir -p "$PREFIX/lib/x86_64-linux-gnu"
cp -a "$libunwind" "$PREFIX/lib/x86_64-linux-gnu/$(basename "$libunwind")"
ln -sfn "$(basename "$libunwind")" \
  "$PREFIX/lib/x86_64-linux-gnu/libunwind.so.8"

failures=0
while IFS= read -r -d '' file; do
  readelf -h "$file" >/dev/null 2>&1 || continue
  maximum="$(
    readelf --version-info "$file" 2>/dev/null |
      grep -oE 'GLIBC_[0-9]+([.][0-9]+)*' |
      sed 's/GLIBC_//' | sort -Vu | tail -1 || true
  )"
  if [[ -n "$maximum" && "$(
    printf '%s\n%s\n' "$maximum" "$GLIBC_CEILING" | sort -V | tail -1
  )" == "$maximum" && "$maximum" != "$GLIBC_CEILING" ]]; then
    echo "$file requires GLIBC_$maximum" >&2
    failures=$((failures + 1))
  fi
done < <(find "$PREFIX" -type f -print0)
[[ "$failures" == 0 ]] || exit 1

dpkg-query -W -f='${binary:Package}\t${Version}\n' \
  >"$PREFIX/.mcbe-package-versions.tsv"
cat >"$PREFIX/.mcbe-build-env" <<EOF
source_commit=$COMMIT
source_date_epoch=$SOURCE_DATE_EPOCH
debian_suite=bullseye
debian_snapshot=20260701T000000Z
glibc_ceiling=$GLIBC_CEILING
package_versions_sha256=$(sha256sum "$PREFIX/.mcbe-package-versions.tsv" | cut -d' ' -f1)
EOF
