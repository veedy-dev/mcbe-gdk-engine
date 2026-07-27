#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir -p \
  "$work/base/GDK-Proton10-32/files/lib/wine/x86_64-windows" \
  "$work/base/GDK-Proton10-32/files/lib/wine/i386-windows" \
  "$work/base/GDK-Proton10-32/files/lib/wine/dxvk/x86_64-windows" \
  "$work/base/GDK-Proton10-32/files/lib/wine/dxvk/i386-windows" \
  "$work/base/GDK-Proton10-32/files/lib/wine/vkd3d-proton/x86_64-windows" \
  "$work/base/GDK-Proton10-32/files/lib/wine/vkd3d-proton/i386-windows" \
  "$work/prefix/bin" "$work/prefix/lib/wine/x86_64-windows" \
  "$work/prefix/lib/wine/i386-windows" \
  "$work/dxvk/dxvk-3.0.1/x64" "$work/dxvk/dxvk-3.0.1/x32" \
  "$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/x64" \
  "$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/x86" \
  "$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/provenance"
printf '#!/bin/sh\necho wine-test\n' >"$work/base/GDK-Proton10-32/proton"
chmod +x "$work/base/GDK-Proton10-32/proton"
printf base >"$work/base/GDK-Proton10-32/files/lib/wine/x86_64-windows/xgameruntime.dll"
printf base >"$work/base/GDK-Proton10-32/files/lib/wine/i386-windows/xgameruntime.dll"
printf threading >"$work/base/GDK-Proton10-32/files/lib/wine/x86_64-windows/xgameruntime.dll.threading"
printf wine >"$work/prefix/bin/wine"
printf server >"$work/prefix/bin/wineserver"
printf xuser >"$work/prefix/lib/wine/x86_64-windows/xgameruntime.dll"
printf xuser >"$work/prefix/lib/wine/i386-windows/xgameruntime.dll"
printf ntdll >"$work/prefix/lib/wine/x86_64-windows/ntdll.dll"
for arch in x64 x32; do
  for dll in d3d8 d3d9 d3d10core d3d11 dxgi; do
    printf dxvk >"$work/dxvk/dxvk-3.0.1/$arch/$dll.dll"
  done
done
for arch in x64 x86; do
  printf vkd3d >"$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/$arch/d3d12.dll"
  printf vkd3d >"$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/$arch/d3d12core.dll"
done
printf licence >"$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/provenance/COPYING.LGPL-2.1"
cat >"$work/prefix/.mcbe-build-env" <<EOF
source_commit=$(git -C "$ROOT" rev-parse HEAD)
source_date_epoch=0
debian_suite=bullseye
debian_snapshot=20260701T000000Z
glibc_ceiling=2.31
package_versions_sha256=test
EOF
tar -czf "$work/base.tar.gz" -C "$work/base" GDK-Proton10-32
tar -czf "$work/dxvk.tar.gz" -C "$work/dxvk" dxvk-3.0.1
tar -czf "$work/vkd3d.tar.gz" -C "$work/vkd3d" vkd3d-proton-3.0.1-nv-dgc
base_sha="$(sha256sum "$work/base.tar.gz" | cut -d' ' -f1)"
dxvk_sha="$(sha256sum "$work/dxvk.tar.gz" | cut -d' ' -f1)"
vkd3d_sha="$(sha256sum "$work/vkd3d.tar.gz" | cut -d' ' -f1)"
commit="$(git -C "$ROOT" rev-parse HEAD)"
"$ROOT/.engine/package-engine.sh" \
  v0.0.0 "$commit" \
  "$work/prefix" "$work/base.tar.gz" \
  "$work/dxvk.tar.gz" "$work/vkd3d.tar.gz" "$work/dist" \
  "$base_sha" "$dxvk_sha" "$vkd3d_sha"
"$ROOT/.engine/package-engine.sh" \
  v0.0.0 "$commit" \
  "$work/prefix" "$work/base.tar.gz" \
  "$work/dxvk.tar.gz" "$work/vkd3d.tar.gz" "$work/dist2" \
  "$base_sha" "$dxvk_sha" "$vkd3d_sha"
cmp \
  "$work/dist/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" \
  "$work/dist2/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz"
"$ROOT/.engine/verify-engine.py" \
  "$work/dist/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" v0.0.0
(cd "$work/dist" && sha256sum -c GDK-Proton-mcbe-gdk-v0.0.0.tar.gz.sha256)
[[ "$(
  tar -xOf "$work/dist/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" \
    GDK-Proton-mcbe-gdk/files/lib/wine/x86_64-windows/xgameruntime.dll.threading
)" == threading ]]
