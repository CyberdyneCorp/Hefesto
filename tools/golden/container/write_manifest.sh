#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The image manifest, /opt/ref/share/hefesto-golden/manifest.json (openspec/changes/m7-extraction
# task 2.2, Open Question 3). It records what a capture depends on beyond the pinned commits:
#
#   glibc     `ldd --version` (first line) and `getconf GNU_LIBC_VERSION`: the C library whose `atan`
#             the fringe-halo capacitances of the EX and PDK cases go through (design D22; the pinned
#             exception table of Open Question 3 is generated against this version);
#   magic     the Magic commit;
#   extcheck  how build_extcheck.sh built Magic's standalone extcheck (tcl-tree, tcl-free or
#             unavailable, with the reason).
#
# goldenlib/refrunners.py copies the manifest into each case's provenance.json (image_manifest), and
# golden-capture.yml prints it in the job summary.
#
# Usage: write_manifest.sh MAGIC_COMMIT PREFIX
set -euo pipefail

COMMIT=$1 PREFIX=$2
DIR=${PREFIX}/share/hefesto-golden
mkdir -p "${DIR}"
# sed reads all of ldd's output: `head -n 1` closes the pipe early, and under pipefail ldd's SIGPIPE
# status (141) failed the image build (golden-capture run 44961580186).
LDD=$(ldd --version 2>&1 | sed -n 1p)
GETCONF=$(getconf GNU_LIBC_VERSION)
python3 - "${DIR}/manifest.json" "${COMMIT}" "${LDD}" "${GETCONF}" "${DIR}/extcheck.status" <<'EOF'
import json
import sys
from pathlib import Path

out, commit, ldd, getconf, status_file = sys.argv[1:]
status = dict(line.split("=", 1) for line in Path(status_file).read_text().splitlines() if "=" in line) \
    if Path(status_file).is_file() else {"build": "not attempted"}
manifest = {
    "schema": 1,
    "glibc": {"ldd_version": ldd, "gnu_libc_version": getconf.split()[-1], "getconf": getconf},
    "magic": {"commit": commit},
    "extcheck": status,
}
Path(out).write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps(manifest, indent=2))
EOF
