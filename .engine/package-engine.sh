#!/usr/bin/env bash
set -Eeuo pipefail
export LC_ALL=C LANG=C TZ=UTC
umask 022

VERSION="${1:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
COMMIT="${2:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
PREFIX="${3:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
BASE="${4:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
DXVK="${5:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
VKD3D="${6:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
DIST="${7:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
BASE_SHA="${8:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
DXVK_SHA="${9:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
VKD3D_SHA="${10:?usage: package-engine.sh VERSION COMMIT PREFIX BASE DXVK VKD3D DIST BASE_SHA DXVK_SHA VKD3D_SHA}"
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
"$ROOT/.engine/apply-graphics.sh" \
  "$engine" "$DXVK" "$VKD3D" "$DXVK_SHA" "$VKD3D_SHA"

threading="$engine/files/lib/wine/x86_64-windows/xgameruntime.dll.threading"
[[ -f "$threading" ]] || {
  echo "GDK-Proton base is missing the native XTaskQueue sidecar." >&2
  exit 1
}
threading_sha="$(sha256sum "$threading" | cut -d' ' -f1)"
cp -a --remove-destination "$PREFIX/." "$engine/files/"
echo "$threading_sha  $threading" | sha256sum -c -
for alias in wine64 wine-preloader wine64-preloader; do
  rm -f "$engine/files/bin/$alias"
  ln -s wine "$engine/files/bin/$alias"
done
rm -rf "$engine/files/lib/wine/i386-unix" "$engine/files/include"
rm -rf "$engine/files/bin-wow64"
install -d -m755 "$engine/files/bin-wow64"
cp -a "$engine/files/bin/wine" "$engine/files/bin-wow64/wine"
cp -a "$engine/files/bin/wineserver" "$engine/files/bin-wow64/wineserver"
ln -s wine "$engine/files/bin-wow64/wine-preloader"
ln -s wine "$engine/files/bin-wow64/msidb"
find "$engine" -type f -name '*.pyc' -delete
find "$engine" -depth -type d -name '__pycache__' -empty -delete
cp "$ROOT/LICENSE" "$engine/WineGDK-LICENSE"
cp "$ROOT/COPYING.LIB" "$engine/COPYING.LIB"
cp "$ROOT/ATTRIBUTION.md" "$engine/ATTRIBUTION.md"
install -d -m755 "$engine/LICENSES"
cp "$ROOT/LICENSES/GPL-2.0-only" "$engine/LICENSES/GPL-2.0-only"
cp "$ROOT/LICENSES/Linux-syscall-note" "$engine/LICENSES/Linux-syscall-note"

python3 "$ROOT/.engine/write-manifest.py" \
  "$engine" "$VERSION" "$COMMIT" "$BASE_SHA" "$DXVK_SHA" "$VKD3D_SHA" \
  "$ROOT/SOURCE-SHA256SUMS" "$ROOT/DEPENDENCIES.lock"

epoch="$(git -C "$ROOT" show -s --format=%ct "$COMMIT")"
asset="GDK-Proton-mcbe-gdk-$VERSION.tar.gz"
tar --sort=name --format=gnu --mtime="@$epoch" \
  --owner=0 --group=0 --numeric-owner \
  -C "$work" -cf - GDK-Proton-mcbe-gdk |
  gzip -n -6 >"$DIST/$asset"
(cd "$DIST" && sha256sum "$asset" >"$asset.sha256")
