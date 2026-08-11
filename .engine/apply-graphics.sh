#!/usr/bin/env bash
set -Eeuo pipefail

ENGINE="${1:?usage: apply-graphics.sh ENGINE DXVK VKD3D DXVK_SHA VKD3D_SHA}"
DXVK="${2:?usage: apply-graphics.sh ENGINE DXVK VKD3D DXVK_SHA VKD3D_SHA}"
VKD3D="${3:?usage: apply-graphics.sh ENGINE DXVK VKD3D DXVK_SHA VKD3D_SHA}"
DXVK_SHA="${4:?usage: apply-graphics.sh ENGINE DXVK VKD3D DXVK_SHA VKD3D_SHA}"
VKD3D_SHA="${5:?usage: apply-graphics.sh ENGINE DXVK VKD3D DXVK_SHA VKD3D_SHA}"

echo "$DXVK_SHA  $DXVK" | sha256sum -c -
echo "$VKD3D_SHA  $VKD3D" | sha256sum -c -
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
tar -xzf "$DXVK" -C "$work"
tar -xzf "$VKD3D" -C "$work"
dxvk="$work/dxvk-3.0.1"
vkd3d="$work/vkd3d-proton-3.0.1-nv-dgc"
(
  cd "$vkd3d"
  sha256sum -c provenance/OUTPUT-SHA256SUMS
)

for pair in x86_64-windows:x64 i386-windows:x32; do
  arch="${pair%%:*}"
  source_arch="${pair##*:}"
  target="$ENGINE/files/lib/wine/dxvk/$arch"
  [[ -d "$target" ]] || {
    echo "GDK-Proton base is missing $target." >&2
    exit 1
  }
  for dll in d3d8 d3d9 d3d10core d3d11 dxgi; do
    install -m644 "$dxvk/$source_arch/$dll.dll" "$target/$dll.dll"
  done
done
printf 'v3.0.1\n' >"$ENGINE/files/lib/wine/dxvk/version"

for pair in x86_64-windows:x64 i386-windows:x86; do
  arch="${pair%%:*}"
  source_arch="${pair##*:}"
  target="$ENGINE/files/lib/wine/vkd3d-proton/$arch"
  [[ -d "$target" ]] || {
    echo "GDK-Proton base is missing $target." >&2
    exit 1
  }
  install -m644 "$vkd3d/$source_arch/d3d12.dll" "$target/d3d12.dll"
  install -m644 "$vkd3d/$source_arch/d3d12core.dll" "$target/d3d12core.dll"
done
printf 'v3.0.1-nv-dgc\n' >"$ENGINE/files/lib/wine/vkd3d-proton/version"
mkdir -p "$ENGINE/files/share/mcbe-gdk-engine/vkd3d-proton"
cp -a "$vkd3d/provenance/." \
  "$ENGINE/files/share/mcbe-gdk-engine/vkd3d-proton/"
