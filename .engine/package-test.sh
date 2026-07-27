#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir -p \
  "$work/base/GDK-Proton10-32/files/lib/wine/x86_64-windows" \
  "$work/base/GDK-Proton10-32/files/lib/wine/i386-windows" \
  "$work/prefix/bin" "$work/prefix/lib/wine/x86_64-windows" \
  "$work/prefix/lib/wine/i386-windows"
printf '#!/bin/sh\necho wine-test\n' >"$work/base/GDK-Proton10-32/proton"
chmod +x "$work/base/GDK-Proton10-32/proton"
printf base >"$work/base/GDK-Proton10-32/files/lib/wine/x86_64-windows/xgameruntime.dll"
printf base >"$work/base/GDK-Proton10-32/files/lib/wine/i386-windows/xgameruntime.dll"
printf wine >"$work/prefix/bin/wine"
printf server >"$work/prefix/bin/wineserver"
printf xuser >"$work/prefix/lib/wine/x86_64-windows/xgameruntime.dll"
printf xuser >"$work/prefix/lib/wine/i386-windows/xgameruntime.dll"
printf ntdll >"$work/prefix/lib/wine/x86_64-windows/ntdll.dll"
cat >"$work/prefix/.mcbe-build-env" <<EOF
source_commit=$(git -C "$ROOT" rev-parse HEAD)
source_date_epoch=0
debian_suite=bullseye
debian_snapshot=20260701T000000Z
glibc_ceiling=2.31
package_versions_sha256=test
EOF
tar -czf "$work/base.tar.gz" -C "$work/base" GDK-Proton10-32
base_sha="$(sha256sum "$work/base.tar.gz" | cut -d' ' -f1)"
commit="$(git -C "$ROOT" rev-parse HEAD)"
"$ROOT/.engine/package-engine.sh" \
  v0.0.0 "$commit" \
  "$work/prefix" "$work/base.tar.gz" "$work/dist" "$base_sha"
"$ROOT/.engine/verify-engine.py" \
  "$work/dist/GDK-Proton-mcbe-gdk-v0.0.0.tar.gz" v0.0.0
(cd "$work/dist" && sha256sum -c GDK-Proton-mcbe-gdk-v0.0.0.tar.gz.sha256)
