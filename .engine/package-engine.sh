#!/usr/bin/env bash
set -Eeuo pipefail
export LC_ALL=C LANG=C TZ=UTC
umask 022

VERSION="${1:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DIST BASE_SHA}"
COMMIT="${2:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DIST BASE_SHA}"
PREFIX="${3:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DIST BASE_SHA}"
BASE="${4:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DIST BASE_SHA}"
DIST="${5:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DIST BASE_SHA}"
BASE_SHA="${6:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DIST BASE_SHA}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"

[[ "$VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
  echo "Version must use vX.Y.Z." >&2
  exit 2
}
[[ "$COMMIT" =~ ^[0-9a-f]{40}$ ]] || {
  echo "Source commit must be a full SHA." >&2
  exit 2
}
echo "$BASE_SHA  $BASE" | sha256sum -c -
[[ -f "$PREFIX/.mcbe-build-env" ]] || {
  echo "WineGDK build environment metadata is missing." >&2
  exit 1
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/base" "$work/GDK-Proton-mcbe-gdk" "$DIST"
tar -xzf "$BASE" -C "$work/base"
mapfile -t base_roots < <(find "$work/base" -mindepth 1 -maxdepth 1 -type d)
[[ "${#base_roots[@]}" == 1 ]] || {
  echo "GDK-Proton archive must contain one root directory." >&2
  exit 1
}
engine="$work/GDK-Proton-mcbe-gdk"
cp -a "${base_roots[0]}/." "$engine/"

for arch in x86_64-windows i386-windows; do
  dll="$engine/files/lib/wine/$arch/xgameruntime.dll"
  [[ ! -f "$dll" ]] || cp -a "$dll" "$dll.threading"
done
cp -a "$PREFIX/." "$engine/files/"
cp "$ROOT/LICENSE" "$engine/WineGDK-LICENSE"
cp "$ROOT/COPYING.LIB" "$engine/COPYING.LIB"
cp "$ROOT/ATTRIBUTION.md" "$engine/ATTRIBUTION.md"

python3 "$ROOT/.engine/write-manifest.py" \
  "$engine" "$VERSION" "$COMMIT" "$BASE_SHA" \
  "$ROOT/SOURCE-SHA256SUMS" "$ROOT/DEPENDENCIES.lock"

epoch="$(git -C "$ROOT" show -s --format=%ct "$COMMIT")"
asset="GDK-Proton-mcbe-gdk-$VERSION.tar.gz"
tar --sort=name --format=gnu --mtime="@$epoch" \
  --owner=0 --group=0 --numeric-owner \
  -C "$work" -cf - GDK-Proton-mcbe-gdk |
  gzip -n -6 >"$DIST/$asset"
(cd "$DIST" && sha256sum "$asset" >"$asset.sha256")
