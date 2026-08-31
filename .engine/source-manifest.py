#!/usr/bin/env python3
import argparse
import hashlib
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
output = root / "SOURCE-SHA256SUMS"
excluded = ("./.git/", ".github/", ".engine/", "LICENSES/")
excluded_files = {
    "ATTRIBUTION.md",
    "DEPENDENCIES.lock",
    "ENGINE.md",
    "EXPERIMENTAL-LUKAS.md",
    "SOURCE-SHA256SUMS",
}


def content():
    tracked = subprocess.check_output(
        ["git", "-C", root, "ls-files", "-z"]
    ).decode().split("\0")
    lines = []
    for name in tracked:
        if (
            not name
            or name in excluded_files
            or any(name.startswith(prefix) for prefix in excluded)
        ):
            continue
        path = root / name
        lines.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {name}")
    return "\n".join(sorted(lines)) + "\n"


parser = argparse.ArgumentParser()
parser.add_argument("--write", action="store_true")
args = parser.parse_args()
expected = content()
if args.write:
    output.write_text(expected)
elif not output.is_file() or output.read_text() != expected:
    raise SystemExit("SOURCE-SHA256SUMS is stale; run .engine/source-manifest.py --write")
