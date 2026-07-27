#!/usr/bin/env bash
# Imported from Wyze3306/BedrockOnLinux at
# ec961ba9024c0d62bf2b793cc2ebba2958147627 (MIT).
set -Eeuo pipefail

SUITE="${1:?usage: pin-apt-snapshot.sh <suite>}"
readonly SNAPSHOT="20260701T000000Z"
readonly BASE="http://snapshot.debian.org/archive/debian/${SNAPSHOT}"
readonly SEC="http://snapshot.debian.org/archive/debian-security/${SNAPSHOT}"

rm -f /etc/apt/sources.list \
  /etc/apt/sources.list.d/*.list \
  /etc/apt/sources.list.d/*.sources 2>/dev/null || true

cat >/etc/apt/sources.list <<EOF
deb [check-valid-until=no] ${BASE}/ ${SUITE} main
deb [check-valid-until=no] ${BASE}/ ${SUITE}-updates main
deb [check-valid-until=no] ${SEC}/ ${SUITE}-security main
EOF
