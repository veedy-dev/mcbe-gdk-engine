#!/usr/bin/env python3
import hashlib
import json
import sys
from pathlib import Path

engine, version, commit, base_sha, source_manifest, lock = sys.argv[1:]
engine = Path(engine)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


dependencies = {}
for line in Path(lock).read_text().splitlines():
    if line and not line.startswith("#"):
        key, value = line.split("=", 1)
        dependencies[key.lower()] = value

critical = {}
fixed = (
    "proton",
    "files/bin/wine",
    "files/bin/wineserver",
    "files/lib/wine/x86_64-windows/xgameruntime.dll",
    "files/lib/wine/i386-windows/xgameruntime.dll",
    "files/lib/wine/x86_64-windows/ntdll.dll",
)
for relative in fixed:
    path = engine / relative
    if not path.is_file():
        raise SystemExit(f"missing critical runtime file: {relative}")
    critical[relative] = digest(path)

for path in sorted((engine / "files").rglob("*")):
    if path.is_file() and path.name.lower() in {
        "d3d11.dll",
        "d3d12.dll",
        "d3d12core.dll",
        "dxgi.dll",
    }:
        critical[path.relative_to(engine).as_posix()] = digest(path)

build = {}
for line in (engine / "files/.mcbe-build-env").read_text().splitlines():
    key, value = line.split("=", 1)
    build[key] = value
build["debian_image"] = dependencies["debian_image"]

manifest = {
    "schema": 1,
    "version": version,
    "archive_root": "GDK-Proton-mcbe-gdk/",
    "source": {
        "repository": "https://github.com/veedy-dev/mcbe-gdk-engine",
        "commit": commit,
        "manifest_sha256": digest(Path(source_manifest)),
    },
    "dependencies": {
        "gdk_proton": {
            "repository": dependencies["gdk_proton_repository"],
            "release": dependencies["gdk_proton_release"],
            "asset": dependencies["gdk_proton_asset"],
            "sha256": base_sha,
        },
        "dxvk": dependencies["dxvk_pin"],
        "vkd3d_proton": dependencies["vkd3d_proton_pin"],
        "steam_runtime_image": dependencies["steam_runtime_image"],
    },
    "build_environment": build,
    "critical_files": critical,
}
(engine / "engine-manifest.json").write_text(
    json.dumps(manifest, indent=2, sort_keys=True) + "\n"
)
