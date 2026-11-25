#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# The ngspice and OpenVAF step of the golden capture image (Q-SIM-23 (a); openspec/changes/
# m9-lvs-simulation design D19, task 3.2):
#
#   build_ngspice.sh <wrapper dir> <prefix> <version> <openvaf url> <openvaf sha256>
#
# <wrapper dir> is container/ngspice, the build context's copy of third_party/ngspice (pin.json,
# patches/series, the CMake wrapper). It builds the pinned ngspice-47 with the Hefesto patch series
# and the spec's feature table exactly as libngspice-hefesto is built, as the `ngspice` executable
# and the shared library, installs both under <prefix>, and checks that the executable reports
# <version> (<release>-hefesto<n>). The manifest and the source archive go to
# <prefix>/share/ngspice/. Then the pinned OpenVAF-reloaded binary (build-time tool only) is
# installed as <prefix>/bin/openvaf-r after its SHA-256 is checked.
set -euo pipefail

wrapper=$1 prefix=$2 version=$3 openvaf_url=$4 openvaf_sha256=$5
build=/opt/src/hefesto-ngspice-build

apt-get -o Acquire::Check-Valid-Until=false update
apt-get install -y --no-install-recommends cmake ninja-build patch
rm -rf /var/lib/apt/lists/*

cmake -S "${wrapper}" -B "${build}" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DHEFESTO_NGSPICE_EXECUTABLE=ON -DCMAKE_INSTALL_PREFIX="${prefix}" \
    -DFETCHCONTENT_BASE_DIR=/opt/src/hefesto-ngspice-deps
cmake --build "${build}" -j"$(nproc)"
cmake --build "${build}" --target ngspice_hefesto_source_archive
cmake --install "${build}"
install -m 0644 "${build}"/ngspice-hefesto-*-src.tar.gz "${prefix}/share/ngspice/"
ldconfig

python3 -I "${wrapper}/tools/ngspice_manifest.py" check --manifest "${prefix}/share/ngspice/ngspice-manifest.json"
"${prefix}/bin/ngspice" -v | tee /tmp/ngspice-version.txt
grep -q "ngspice-${version} " /tmp/ngspice-version.txt
test -f "${prefix}/lib/libngspice-hefesto.so"
rm -f /tmp/ngspice-version.txt
echo "hefesto-golden: ngspice ${version}"

# OpenVAF-reloaded, checked by SHA-256 (the upstream offers no signature).
cd /tmp
python3 -c 'import sys, urllib.request; urllib.request.urlretrieve(sys.argv[1], sys.argv[2])' \
    "${openvaf_url}" /tmp/openvaf.tar.gz
echo "${openvaf_sha256}  /tmp/openvaf.tar.gz" | sha256sum -c -
tar -xzf /tmp/openvaf.tar.gz -C "${prefix}/bin" openvaf-r
rm -f /tmp/openvaf.tar.gz
chmod 0755 "${prefix}/bin/openvaf-r"
"${prefix}/bin/openvaf-r" --version | head -n 1
rm -rf "${build}"
