#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"

shellcheck .engine/*.sh
python3 -m py_compile .engine/*.py
.engine/source-manifest.py
for file in LICENSE COPYING.LIB LICENSES/BedrockOnLinux-MIT \
  LICENSES/GPL-2.0-only LICENSES/Linux-syscall-note ATTRIBUTION.md; do
  [[ -s "$file" ]] || {
    echo "Missing licence or attribution: $file" >&2
    exit 1
  }
done
.engine/package-test.sh
git diff --check
