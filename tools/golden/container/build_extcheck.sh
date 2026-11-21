#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Magic's standalone extcheck in the capture image (openspec/changes/m7-extraction design D15 and R7,
# task 2.2). Magic's default build makes only `magic` (Makefile.in:10); extcheck is built here at the
# same pin, in two tries, and installed next to `magic` as ${PREFIX}/bin/extcheck:
#
#   try_tcl_tree   the Tcl build's own tree (/opt/src/magic, already built): utils/libutils.a, then
#                  `make -C extcheck` (extflat/libextflat.o and utils/libutils.a, extcheck/Makefile.in).
#   try_tcl_free   the same commit configured again --without-tcl --without-x in /opt/src/magic-notcl:
#                  `make prepare` (database.h), utils/libutils.a and extflat/libextflat.o only, then
#                  `make -C extcheck`.
#
# A try counts only if the program links AND passes the usage smoke of capability netlist-extcheck
# "Invocation" (scenario "Missing root"): with no arguments it exits with status 1 and prints the usage
# text, which starts "Standard arguments:" (extflat/EFargs.c:158-165, 330). A Tcl-tree program that
# links would fail it: under MAGIC_WRAPPER, EFArgs returns NULL without the usage text when no root is
# given (EFargs.c:300-301). The first try that passes is installed.
#
# Neither may link at the pin: libextflat.o has EFantenna.c, which needs the extractor and the CIF
# and selection modules, and EFread.c and EFbuild.c call DBCellLookDef, ExtCurStyle and extGetDevType
# (EFread.c:186, 259; EFbuild.c:1125); libutils.a's main.o, pulled in for `Path`, needs every other
# module. That is not an image failure: the outcome and its reason (the linker's undefined symbols)
# go to ${STATUS} for write_manifest.sh, and the NL-CHK-01 case is then hand-derived (design R7).
#
# Usage: build_extcheck.sh MAGIC_TREE MAGIC_REPO MAGIC_COMMIT PREFIX
set -uo pipefail

TREE=$1 REPO=$2 COMMIT=$3 PREFIX=$4
NOTCL=/opt/src/magic-notcl
STATUS_DIR=${PREFIX}/share/hefesto-golden
STATUS=${STATUS_DIR}/extcheck.status
LOG=/tmp/extcheck-build.log
mkdir -p "${STATUS_DIR}"

# usage_smoke BIN: 0 when BIN with no arguments exits 1 and prints the usage text on stderr.
usage_smoke() {
  local status
  "$1" > /tmp/extcheck.out 2> /tmp/extcheck.err
  status=$?
  echo "extcheck with no arguments: exit status ${status}; stderr:"
  cat /tmp/extcheck.err
  [ "${status}" -eq 1 ] && grep -q '^Standard arguments:' /tmp/extcheck.err
}

# link_reason: the undefined symbols the last link reported, one line.
link_reason() {
  local symbols
  symbols=$(grep -oE "undefined reference to \`[^']+'|\"_[A-Za-z0-9_]+\", referenced" "${LOG}" \
    | sed -E "s/undefined reference to \`([^']+)'/\1/; s/\"_([A-Za-z0-9_]+)\", referenced/\1/" | sort -u)
  if [ -n "${symbols}" ]; then
    echo "link failed: $(echo "${symbols}" | wc -l) undefined symbols, e.g. $(echo "${symbols}" | head -n 8 | tr '\n' ' ')"
  else
    echo "build failed: $(tail -n 3 "${LOG}" | tr '\n' ' ')"
  fi
}

# install_from DIR HOW: install DIR/extcheck/extcheck when it passes the usage smoke.
install_from() {
  local bin=$1/extcheck/extcheck
  if [ ! -x "${bin}" ]; then
    echo "$2: $(link_reason)" >> "${STATUS}.reasons"
    return 1
  fi
  if ! usage_smoke "${bin}"; then
    echo "$2: links, but fails the usage smoke (no usage text or another exit status)" >> "${STATUS}.reasons"
    return 1
  fi
  install -m 0755 "${bin}" "${PREFIX}/bin/extcheck"
  printf 'build=%s\n' "$2" > "${STATUS}"
}

try_tcl_tree() {
  (cd "${TREE}" && make lib@utils module@extflat && make -C extcheck) > "${LOG}" 2>&1
  install_from "${TREE}" tcl-tree
}

try_tcl_free() {
  rm -f "${LOG}"
  (fetch_commit "${NOTCL}" "${REPO}" "${COMMIT}" && cd "${NOTCL}" \
    && ./configure --prefix="${PREFIX}" --without-tcl --without-x --with-opengl=no --with-cairo=no \
    && ! grep -q -- '-DMAGIC_WRAPPER' defs.mak \
    && make prepare && make -j"$(nproc)" lib@utils module@extflat && make -C extcheck) > "${LOG}" 2>&1
  install_from "${NOTCL}" tcl-free
}

rm -f "${STATUS}" "${STATUS}.reasons"
if try_tcl_tree || try_tcl_free; then
  echo "extcheck installed ($(cat "${STATUS}"))"
  "${PREFIX}/bin/extcheck"; echo "extcheck exit status $?"
else
  {
    printf 'build=unavailable\n'
    printf 'reason=%s\n' "$(tr '\n' ';' < "${STATUS}.reasons" | sed 's/;$//')"
  } > "${STATUS}"
  echo "extcheck not available at ${COMMIT}: see ${STATUS}"
fi
cat "${STATUS}"
rm -rf "${NOTCL}" "${STATUS}.reasons" "${LOG}" /tmp/extcheck.out /tmp/extcheck.err
exit 0
