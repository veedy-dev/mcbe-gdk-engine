#!/usr/bin/env python3
import hashlib
import json
import posixpath
import sys
import tarfile
from pathlib import PurePosixPath

archive = sys.argv[1]
expected_version = sys.argv[2]
root = "GDK-Proton-mcbe-gdk/"

with tarfile.open(archive, "r:gz") as bundle:
    members = bundle.getmembers()
    for member in members:
        name = member.name.rstrip("/") + ("/" if member.isdir() else "")
        if not name.startswith(root):
            raise SystemExit(f"unexpected archive path: {member.name}")
        normalized = posixpath.normpath("/" + member.name)
        if not normalized.startswith("/GDK-Proton-mcbe-gdk"):
            raise SystemExit(f"unsafe archive path: {member.name}")
        if member.issym() or member.islnk():
            target = PurePosixPath(member.linkname)
            if target.is_absolute():
                raise SystemExit(f"unsafe archive link: {member.name}")
            if member.issym():
                resolved = posixpath.normpath(
                    posixpath.join(posixpath.dirname(member.name), member.linkname)
                )
            else:
                resolved = posixpath.normpath(member.linkname)
            if not (
                resolved == "GDK-Proton-mcbe-gdk"
                or resolved.startswith(root)
            ):
                raise SystemExit(f"archive link escapes root: {member.name}")

    manifest_member = bundle.getmember(root + "engine-manifest.json")
    manifest = json.load(bundle.extractfile(manifest_member))
    if manifest["schema"] != 1 or manifest["version"] != expected_version:
        raise SystemExit("manifest schema or version mismatch")
    if manifest["archive_root"] != root:
        raise SystemExit("manifest archive root mismatch")
    if len(manifest["source"]["commit"]) != 40:
        raise SystemExit("manifest source commit is not a full SHA")
    build = manifest["build_environment"]
    ntsync = manifest["dependencies"]["ntsync_uapi"]
    if build.get("ntsync_enabled") != "1":
        raise SystemExit("build did not enable NTSync")
    if build.get("wine_cflags") != (
        "-O2 -march=nocona -mtune=core-avx2 -mfpmath=sse"
    ):
        raise SystemExit("Wine runtime is missing the reviewed CPU tuning")
    if (
        build.get("ntsync_uapi_version") != ntsync["version"]
        or build.get("ntsync_uapi_sha256") != ntsync["sha256"]
    ):
        raise SystemExit("NTSync UAPI build metadata mismatch")

    for relative, expected in manifest["critical_files"].items():
        member = bundle.getmember(root + relative)
        actual = hashlib.sha256(bundle.extractfile(member).read()).hexdigest()
        if actual != expected:
            raise SystemExit(f"critical file hash mismatch: {relative}")

    relative = "files/bin/wineserver"
    member = bundle.getmember(root + relative)
    if b"/dev/ntsync" not in bundle.extractfile(member).read():
        raise SystemExit(f"NTSync support is missing: {relative}")
