#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

base="$work/base/GDK-Proton10-32"
prefix="$work/prefix"
modules=(
  combase.dll
  kernel32.dll
  microsoft.windowsappruntime.bootstrap.dll
  windows.storage.applicationdata.dll
  windows.storage.dll
  winex11.drv
  xgameruntime.dll
)
mkdir -p \
  "$base/files/bin" "$base/files/bin-wow64" "$base/files/include" \
  "$base/files/lib/wine/x86_64-windows" \
  "$base/files/lib/wine/i386-windows" \
  "$base/files/lib/wine/x86_64-unix" \
  "$base/files/lib/wine/i386-unix" \
  "$base/files/lib/wine/dxvk/x86_64-windows" \
  "$base/files/lib/wine/dxvk/i386-windows" \
  "$base/files/lib/wine/vkd3d-proton/x86_64-windows" \
  "$base/files/lib/wine/vkd3d-proton/i386-windows" \
  "$base/files/share/default_pfx/drive_c/windows/system32" \
  "$base/files/share/default_pfx/drive_c/windows/syswow64" \
  "$prefix/bin" "$prefix/bin-wow64" "$prefix/include" \
  "$prefix/lib/wine/x86_64-windows" \
  "$prefix/lib/wine/i386-windows" \
  "$prefix/lib/wine/x86_64-unix" \
  "$prefix/lib/wine/i386-unix" \
  "$prefix/share/default_pfx/drive_c/windows/system32" \
  "$prefix/share/default_pfx/drive_c/windows/syswow64" \
  "$work/dxvk/dxvk-3.0.1/x64" "$work/dxvk/dxvk-3.0.1/x32" \
  "$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/x64" \
  "$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/x86" \
  "$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/provenance"

printf '#!/bin/sh\necho wine-test\n' >"$base/proton"
chmod +x "$base/proton"
printf base-wine >"$base/files/bin/wine"
printf base-server >"$base/files/bin/wineserver"
printf base-wow64 >"$base/files/bin-wow64/wine"
printf base-include >"$base/files/include/wine.h"
printf winegdk-wine >"$prefix/bin/wine"
printf winegdk-server >"$prefix/bin/wineserver"
printf winegdk-wow64 >"$prefix/bin-wow64/wine"
printf winegdk-include >"$prefix/include/wine.h"

for pair in i386-windows:syswow64 x86_64-windows:system32; do
  arch="${pair%%:*}"
  system="${pair##*:}"
  printf 'base-ntdll-%s' "$arch" >"$base/files/lib/wine/$arch/ntdll.dll"
  printf 'base-unix-%s' "$arch" >"$base/files/lib/wine/${arch%-windows}-unix/ntdll.so"
  printf 'winegdk-ntdll-%s' "$arch" >"$prefix/lib/wine/$arch/ntdll.dll"
  printf 'winegdk-unix-%s' "$arch" >"$prefix/lib/wine/${arch%-windows}-unix/ntdll.so"
  for module in "${modules[@]}"; do
    printf 'base-%s-%s' "$arch" "$module" \
      >"$base/files/lib/wine/$arch/$module"
    printf 'winegdk-%s-%s' "$arch" "$module" \
      >"$prefix/lib/wine/$arch/$module"
    printf 'base-pfx-%s-%s' "$arch" "$module" \
      >"$base/files/share/default_pfx/drive_c/windows/$system/$module"
    printf 'winegdk-pfx-%s-%s' "$arch" "$module" \
      >"$prefix/share/default_pfx/drive_c/windows/$system/$module"
  done
done
printf threading \
  >"$base/files/lib/wine/x86_64-windows/xgameruntime.dll.threading"

for arch in x64 x32; do
  for dll in d3d8 d3d9 d3d10core d3d11 dxgi; do
    printf dxvk >"$work/dxvk/dxvk-3.0.1/$arch/$dll.dll"
  done
done
for arch in x64 x86; do
  printf 'vkd3d-%s-d3d12' "$arch" \
    >"$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/$arch/d3d12.dll"
  printf 'vkd3d-%s-d3d12core' "$arch" \
    >"$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc/$arch/d3d12core.dll"
done
vkd3d_fixture="$work/vkd3d/vkd3d-proton-3.0.1-nv-dgc"
(
  cd "$vkd3d_fixture"
  sha256sum \
    x64/d3d12.dll x64/d3d12core.dll \
    x86/d3d12.dll x86/d3d12core.dll \
    >provenance/OUTPUT-SHA256SUMS
)
printf patch >"$vkd3d_fixture/provenance/fix-occluded-frame-latency.patch"
printf licence >"$vkd3d_fixture/provenance/COPYING.LGPL-2.1"
cat >"$prefix/.mcbe-build-env" <<EOF
source_commit=$(git -C "$ROOT" rev-parse HEAD)
source_date_epoch=0
debian_suite=bullseye
debian_snapshot=20260701T000000Z
glibc_ceiling=2.31
ntsync_enabled=1
ntsync_uapi_version=linux-v6.14
ntsync_uapi_sha256=006437ee52a3e04f921df77081eb5c21c44c71f598b10ac534c6ef9e78296262
wine_cflags=-O2 -march=nocona -mtune=core-avx2 -mfpmath=sse
package_versions_sha256=test
EOF
printf 'package\tversion\n' >"$prefix/.mcbe-package-versions.tsv"

tar -czf "$work/base.tar.gz" -C "$work/base" GDK-Proton10-32
tar -czf "$work/dxvk.tar.gz" -C "$work/dxvk" dxvk-3.0.1
tar -czf "$work/vkd3d.tar.gz" -C "$work/vkd3d" vkd3d-proton-3.0.1-nv-dgc
base_sha="$(sha256sum "$work/base.tar.gz" | cut -d' ' -f1)"
dxvk_sha="$(sha256sum "$work/dxvk.tar.gz" | cut -d' ' -f1)"
vkd3d_sha="$(sha256sum "$work/vkd3d.tar.gz" | cut -d' ' -f1)"
commit="$(git -C "$ROOT" rev-parse HEAD)"

"$ROOT/.engine/package-engine.sh" \
  v0.0.0 "$commit" \
  "$prefix" "$work/base.tar.gz" \
  "$work/dxvk.tar.gz" "$work/vkd3d.tar.gz" "$work/dist-bad" \
  "$base_sha" "$dxvk_sha" "$vkd3d_sha"
if "$ROOT/.engine/verify-engine.py" \
  "$work/dist-bad/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" v0.0.0; then
  echo "Verification accepted an engine without NTSync." >&2
  exit 1
fi
printf 'winegdk-server /dev/ntsync' >"$prefix/bin/wineserver"

for dist in dist dist2; do
  "$ROOT/.engine/package-engine.sh" \
    v0.0.0 "$commit" \
    "$prefix" "$work/base.tar.gz" \
    "$work/dxvk.tar.gz" "$work/vkd3d.tar.gz" "$work/$dist" \
    "$base_sha" "$dxvk_sha" "$vkd3d_sha"
done
cmp \
  "$work/dist/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" \
  "$work/dist2/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz"
"$ROOT/.engine/verify-engine.py" \
  "$work/dist/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" v0.0.0
(cd "$work/dist" && sha256sum -c GDK-Proton-mcbe-gdk-v0.0.0.tar.gz.sha256)

mkdir "$work/result"
tar -xzf "$work/dist/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" -C "$work/result"
result="$work/result/GDK-Proton-mcbe-gdk"
cmp "$prefix/bin/wine" "$result/files/bin/wine"
cmp "$prefix/bin/wineserver" "$result/files/bin/wineserver"
cmp "$prefix/bin/wine" "$result/files/bin-wow64/wine"
cmp "$prefix/bin/wineserver" "$result/files/bin-wow64/wineserver"
cmp "$prefix/lib/wine/x86_64-unix/ntdll.so" \
  "$result/files/lib/wine/x86_64-unix/ntdll.so"
for pair in i386-windows:syswow64 x86_64-windows:system32; do
  arch="${pair%%:*}"
  system="${pair##*:}"
  cmp "$prefix/lib/wine/$arch/ntdll.dll" \
    "$result/files/lib/wine/$arch/ntdll.dll"
  for module in "${modules[@]}"; do
    cmp "$prefix/lib/wine/$arch/$module" \
      "$result/files/lib/wine/$arch/$module"
    cmp "$prefix/share/default_pfx/drive_c/windows/$system/$module" \
      "$result/files/share/default_pfx/drive_c/windows/$system/$module"
  done
done
[[ "$(cat "$result/files/lib/wine/x86_64-windows/xgameruntime.dll.threading")" == threading ]]

for pair in x64:x86_64-windows x86:i386-windows; do
  source_arch="${pair%%:*}"
  target_arch="${pair##*:}"
  for dll in d3d12.dll d3d12core.dll; do
    cmp "$vkd3d_fixture/$source_arch/$dll" \
      "$result/files/lib/wine/vkd3d-proton/$target_arch/$dll"
  done
done
cmp "$vkd3d_fixture/provenance/OUTPUT-SHA256SUMS" \
  "$result/files/share/mcbe-gdk-engine/vkd3d-proton/OUTPUT-SHA256SUMS"
cmp "$vkd3d_fixture/provenance/fix-occluded-frame-latency.patch" \
  "$result/files/share/mcbe-gdk-engine/vkd3d-proton/fix-occluded-frame-latency.patch"