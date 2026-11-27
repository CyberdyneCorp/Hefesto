#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Writes the seed corpora of the M7 fuzz targets (m7-extraction design D22,
task 19.2): fuzz_extract_section, fuzz_ext_netlist, fuzz_res_ext_reader and
fuzz_extract_layout. Seeds come from the golden case inputs, the fuzz base
technologies and hand-written hostile inputs (M7-S1..S8 of design D24).
Each seed is a mode byte followed by the target's input (see each target's
file comment). Run from the repository root; the output is deterministic.

Owner: INTEGRATION (tests/fuzz/, design D2).
"""

from __future__ import annotations

import random
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "tests" / "fuzz" / "corpus"
NETLIST_CASES = ROOT / "tests" / "golden" / "NETLIST" / "cases"


def write(target: str, name: str, mode: int, body: str | bytes) -> None:
    data = body.encode() if isinstance(body, str) else body
    out = CORPUS / target
    out.mkdir(parents=True, exist_ok=True)
    (out / name).write_bytes(bytes([mode]) + data)


# --- fuzz_extract_section -------------------------------------------------

HOSTILE_SECTIONS = {
    # Q-EXTRACT-30: numbers past the int range and NaN saturate.
    "hostile-numbers": "style h\n lambda 1e30\n step 1e30\n sidehalo nan\n cscale 1e300\n rscale -1e300\n"
    "    planeorder well 0\n    planeorder active 1\n    planeorder metal1 2\n"
    "    device mosfet nfet nfet ndiff pwell VSUB l<1e30 w>=nan\n    areacap metal1 1e300\n"
    "    resist metal1 1e300\n    sidewall metal1 space space metal1 1e300 nan\n",
    # Variants and many styles (style selection and the variant expansion).
    "variants": "style a variants (x),(y),(z)\n    lambda 1\n    areacap metal1 10\n"
    "style b\n    lambda 0.5\n    units microns\n    perimc metal1 space 20\n",
    # Unknown keywords, short lines, a lone `style` and an empty style.
    "garbage": "bogus 1\nstyle\nstyle e\ndevice\ndevice mosfet\nresist\ncontact via\nsubstrate\n"
    "antenna metal1\nantenna metal1 surface 1 2 3 4\nmodel partial cumulative\ntiedown ndiff\n"
    "planeorder nosuch 9\nnoplaneordering\nheight metal1\nfringeshieldhalo -5\n",
    # Antenna rules (antenna-check "Antenna tech-file keywords grammar").
    "antenna": "style ant\n    lambda 1\n    planeorder well 0\n    planeorder active 1\n"
    "    planeorder metal1 2\n    planeorder metal2 3\n    antenna metal1 surface 400\n"
    "    antenna metal2 sidewall 400 none\n    antenna poly surface 200 10 2\n"
    "    model partial cumulative\n    tiedown ndiff,pdiff\n"
    "    device mosfet nfet nfet ndiff pwell VSUB\n",
    # Duplicate sideoverlap rules (Q-EXTRACT-17, Q-EXTRACT-33).
    "duplicate-rules": "style d\n    lambda 1\n    sidehalo 10\n    planeorder well 0\n"
    "    planeorder active 1\n    planeorder metal1 2\n    planeorder metal2 3\n"
    "    overlap metal2 metal1 20\n    sideoverlap metal2 space metal1 6\n"
    "    sideoverlap metal2 space metal1 6\n    defaultsideoverlap metal2 metal2 metal1 metal1 5\n",
}


def extract_section_seeds() -> None:
    target = "fuzz_extract_section"
    for base in range(6):
        for compat_off in (0, 8):
            write(target, f"base{base}-{'off' if compat_off else 'on'}", base | compat_off, "")
    # Rescales: n/d = 2/1, 1/2, 3/4 on the M7 base.
    for name, mode in (("rescale-2-1", 2 | 0x10), ("rescale-1-2", 2 | 0x40), ("rescale-3-4", 2 | 0x20 | 0xC0)):
        write(target, name, mode, "")
    for name, body in HOSTILE_SECTIONS.items():
        write(target, name + "-on", 2, body)
        write(target, name + "-off", 2 | 8 | 0x50, body)


# --- fuzz_ext_netlist -----------------------------------------------------

def case_files(case: Path) -> str | None:
    """The `.ext` inputs of an NL case as one fuzz input: the root first."""
    exts = sorted((case / "input").glob("*.ext"))
    if not exts:
        return None
    script = case / "input" / "commands.tcl"
    commands = script.read_text(errors="replace") if script.exists() else ""
    stems = [p.stem for p in exts]
    root = "top" if "top" in stems else stems[0]
    for stem in stems:
        if re.search(rf"\b{re.escape(stem)}\b", commands.split("ext2s")[-1] if "ext2s" in commands else ""):
            root = stem
            break
    text = (case / "input" / f"{root}.ext").read_text(errors="replace")
    for p in exts:
        if p.stem != root:
            text += f"@@ {p.stem}\n" + p.read_text(errors="replace")
    return text


NETLIST_MODES = (0x00, 0x01, 0x02, 0x04, 0x10, 0x20, 0x40, 0x23, 0x35, 0x7F, 0x26, 0x31)

HOSTILE_EXT = {
    # Q-NETLIST-62: port numbers past the cap and at it.
    "hostile-port-number": "port \"a\" 2147483647 0 0 1 1 metal1\nport \"b\" 1048576 0 0 1 1 metal1\n"
    "port \"c\" -5 0 0 1 1 metal1\nnode \"a\" 0 0 0 0 metal1\n",
    "hostile-port-number-cap": "port \"a\" 1048577 0 0 1 1 metal1\nnode \"a\" 0 0 0 0 metal1\n",
    # Q-NETLIST-31, Q-NETLIST-60: two shorted-port pairs in one cell and a subcell.
    "shorted-ports": "port \"A\" 0 0 0 1 1 metal1\nport \"B\" 1 9 0 10 1 metal1\nport \"C\" 2 20 0 21 1 metal1\n"
    "port \"D\" 3 30 0 31 1 metal1\nnode \"A\" 10 2 0 0 metal1\nnode \"B\" 10 2 9 0 metal1\n"
    "node \"C\" 10 2 20 0 metal1\nnode \"D\" 10 2 30 0 metal1\nequiv \"A\" \"B\"\nequiv \"C\" \"D\"\n"
    "use sub s0 1 0 0 0 1 0\nmerge \"s0/X\" \"A\" 3\n"
    "@@ sub\nport \"X\" 0 0 0 1 1 metal1\nport \"Y\" 1 5 0 6 1 metal1\nnode \"X\" 5 1 0 0 metal1\n"
    "node \"Y\" 5 1 5 0 metal1\nequiv \"X\" \"Y\"\n",
    # M6-S8 caps: a use cycle, a huge array, a deep chain and a merge chain.
    "use-cycle": "use a a0 1 0 0 0 1 0\n@@ a\nuse top t0 1 0 0 0 1 0\n",
    "huge-array": "use leaf u[0:100000:1][0:100000:1] 1 0 0 0 1 0\n@@ leaf\nnode \"n\" 1 1 0 0 metal1\n"
    "cap \"n\" \"n\" 1\n",
    "merge-array": "use leaf u[0:63:1][0:63:1] 1 0 0 0 1 0\nmerge \"u[0:63,0:63]/n\" \"u[0:63,0:63]/m\"\n"
    "@@ leaf\nnode \"n\" 1 1 0 0 metal1\nnode \"m\" 1 1 1 0 metal1\n",
    # Devices of every class, odd terminal counts, parameters and attributes.
    "devices": "scale 100 1000 1\nparameters nmos l=l w=w a1=as p1=ps\nnode \"g\" 1 2 0 0 poly\n"
    "node \"s\" 1 2 1 0 ndiff\nnode \"d\" 1 2 2 0 ndiff\nattr \"g\" 0 0 0 0 poly \"G$\"\n"
    "device mosfet nmos 0 0 1 1 2 4 \"GND!\" \"g\" 4 0 \"s\" 2 \"S$\",4,6 \"d\" 2 0\n"
    "device mosfet nmos 0 0 1 1 2 4 \"GND!\" \"g\" 4 0 \"s\" 2 0 \"d\" 2 0\n"
    "device resistor None 0 0 1 1 25.5 \"g\" 0 0 \"s\" 0 0 \"d\" 0 0\n"
    "device capacitor None 0 0 1 1 12 \"s\" 0 0 \"d\" 0 0\n"
    "device diode dn 0 0 1 1 a=4 \"s\" 0 0 \"d\" 0 0\n"
    "device bjt npn 0 0 1 1 2 2 \"d\" \"g\" 0 0 \"s\" 0 0\n"
    "device subckt sub1 0 0 1 1 \"s\" 0 0\ndevice rsubckt rs 0 0 1 1 2 3 \"g\" \"s\" 0 0 \"d\" 0 0\n"
    "device vsource v1 0 0 1 1 \"s\" 0 0 \"d\" 0 0\n"
    "fet nfet 0 0 1 1 8 12 \"GND!\" \"g\" 4 0 \"s\" 2 0 \"d\" 2 0\n"
    "resist \"s\" \"d\" 1e30\nresist \"s\" \"d\" -5\ncap \"s\" \"d\" 1e39\nkillnode \"d\"\n",
    # Names: trimming, commas, brackets, hspice dots and long names.
    "names": "node \"a!#,b=c[0].d\" 1 1 0 0 metal1\nnode \",lead\" 1 1 1 0 metal1\n"
    "node \"" + "x" * 3000 + "\" 1 1 2 0 metal1\nequiv \"a!#,b=c[0].d\" \",lead\"\n",
}


def ext_netlist_seeds() -> None:
    target = "fuzz_ext_netlist"
    index = 0
    for case in sorted(NETLIST_CASES.iterdir()):
        if not re.match(r"NL-(FLAT|HIER|SIM|SPICE|CHK)-|NETLIST-SMOKE", case.name):
            continue
        text = case_files(case)
        if text is None:
            continue
        mode = NETLIST_MODES[index % len(NETLIST_MODES)]
        write(target, f"{case.name.lower()}-m{mode:02x}", mode, text)
        index += 1
    for name, body in HOSTILE_EXT.items():
        for mode in (0x00, 0x23, 0x7F):
            write(target, f"{name}-m{mode:02x}", mode, body)


# --- fuzz_res_ext_reader ----------------------------------------------------

RES_INPUTS = {
    # Q-NETLIST-68: negative R, a terminal on an unknown net, a model with no device line.
    "hostile-input": "scale 1000 1000 1\nnode \"n\" -1 3000 0 0 metal1\n"
    "device mosfet nosuch 0 0 1 1 2 4 \"GND!\" \"g\" 4 0 \"zz\" 2 0 \"yy\" 2 0\n"
    "device mosfet nfet 0 0 1 1 2 4 \"GND!\" \"g\" 4 0 \"s\" 2 0 \"d\" 2 0\n",
    # Q-NETLIST-29: attr before any node, a long res:min, attr on the previous node.
    "attributes": "attr a 0 0 0 0 poly \"res:skip\"\nnode \"a\" 0 0 0 0 poly\nnode \"b\" 0 0 5 0 poly\n"
    "attr a 0 0 0 0 poly \"res:min=" + "9" * 400 + "\"\nattr b 3 4 0 0 poly \"res:drive\"\n"
    "attr a 0 0 0 0 poly \"res:force\"\nattr nosuch 0 0 0 0 poly \"res:skip\"\n",
    # A parent with `connect` records into `top` (ResReadDrivePoint's eight
    # fields: llx lly urx ury type up down), and an overlay. A `connect` of
    # seven fields or a `substrate` of fewer than seven reads past the record
    # in extresist's reader (security.md, M7 review): those reproducers stay
    # out of the corpus until the reader is fixed.
    "parent-and-overlay": "node \"w\" 10 100 0 0 metal1\nport \"w\" 0 0 0 1 1 metal1\n"
    "cap \"w\" \"w\" 5\n@@ parent\nuse top t0 1 0 0 0 1 0\nconnect 0 0 1 1 metal1 w t0/w\n"
    "connect -9999999 -9999999 9999999 9999999 metal1 w t0/w\n@@ top.res\nscale 1 1 1\n"
    "killnode \"w\"\nrnode \"w_0\" 0 2000 0 0 0\nrnode \"w_1\" 0 2000 100 0 0\n"
    "resist \"w_1\" \"w_0\" 2500\nresist \"w_1\" \"nosuch\" -1\n",
    # Overlay records that do not parse or name unknown nodes.
    "overlay-garbage": "node \"w\" 1 1 0 0 metal1\n@@ top.res\nkillnode\nrnode \"x\"\nresist \"a\" \"b\"\n"
    "device mosfet nfet 0 0 1 1 2 4 \"GND!\" \"w\" 4 0\nkillnode \"w\"\nkillnode \"w\"\n",
}


def res_ext_seeds() -> None:
    target = "fuzz_res_ext_reader"
    for case in ("NL-FLAT-01", "NL-FLAT-06", "NL-SPICE-03", "NL-SIM-01"):
        text = case_files(NETLIST_CASES / case)
        if text:
            for mode in (0x00, 0x05, 0x0B):
                write(target, f"{case.lower()}-m{mode:02x}", mode, text)
    for overlay in sorted(NETLIST_CASES.glob("NL-RESNET-*/expected/collected/*.res.ext"))[:6]:
        body = "node \"w_0\" 100 2000 0 0 metal1\n@@ top.res\n" + overlay.read_text(errors="replace")
        write(target, f"{overlay.parent.parent.parent.name.lower()}-overlay", 0x00, body)
    for name, body in RES_INPUTS.items():
        for mode in (0x00, 0x01, 0x06, 0x0F):
            write(target, f"{name}-m{mode:02x}", mode, body)


# --- fuzz_extract_layout ----------------------------------------------------

TYPES = ["pwell", "nwell", "dnwell", "ndiff", "pdiff", "psd", "ndiffh", "nfet", "pfet", "poly",
         "rpoly", "ndc", "m1c", "metal1", "via", "metal2", "metal3", "mimcap", "thkox"]


def rect(kind: str, x: int, y: int, w: int, h: int, label: bool = False, sub: bool = False) -> bytes:
    t = TYPES.index(kind) | (0x20 if label else 0) | (0x40 if sub else 0)
    return bytes([t, x & 0xFF, y & 0xFF, (w - 1) & 0x1F, (h - 1) & 0x1F])


def inverter(sub: bool) -> bytes:
    """An nfet/pfet inverter with contacts, wires and labels."""
    parts = [rect("pwell", -2, -2, 14, 12, sub=sub), rect("ndiff", 0, 0, 10, 4, sub=sub),
             rect("nfet", 4, 0, 2, 4, sub=sub), rect("poly", 4, -2, 2, 2, sub=sub),
             rect("poly", 4, 4, 2, 8, sub=sub), rect("ndc", 0, 0, 2, 2, label=True, sub=sub),
             rect("metal1", 0, 0, 2, 20, sub=sub), rect("nwell", -2, 12, 14, 12, sub=sub),
             rect("pdiff", 0, 14, 10, 4, sub=sub), rect("pfet", 4, 14, 2, 4, sub=sub),
             rect("poly", 4, 12, 2, 2, sub=sub), rect("m1c", 4, 8, 2, 2, label=True, sub=sub),
             rect("metal1", 8, 0, 2, 18, label=True, sub=sub), rect("metal2", -4, 6, 20, 4, sub=sub),
             rect("via", 8, 6, 2, 2, sub=sub), rect("rpoly", 12, 4, 6, 2, sub=sub),
             rect("mimcap", 20, 0, 6, 6, sub=sub), rect("thkox", 3, -1, 4, 6, sub=sub)]
    return b"".join(parts)


def layout_seeds() -> None:
    target = "fuzz_extract_layout"
    for mode in (0x00, 0x01, 0x02, 0x03, 0x0C, 0x1D, 0x2E, 0x3F):
        write(target, f"inverter-m{mode:02x}", mode, inverter(False) + inverter(True))
    # Overlapping wires of top and sub (merge records, Q-EXTRACT-40).
    overlap = rect("metal1", 0, 0, 10, 4, label=True) + rect("metal1", 0, 0, 10, 4, label=True, sub=True) + \
        rect("metal1", 2, 2, 20, 2, sub=True) + rect("metal2", 0, 0, 30, 3) + rect("via", 1, 1, 2, 2)
    for mode in (0x00, 0x01, 0x08, 0x09, 0x34):
        write(target, f"overlap-m{mode:02x}", mode, overlap)
    # Two gate types touching (Q-EXTRACT-41) and a big plate at the coordinate cap.
    abut = rect("ndiff", 0, 0, 2, 4) + rect("nfet", 2, 0, 2, 4) + rect("pfet", 2, 4, 2, 4) + \
        rect("pdiff", 0, 4, 2, 4) + rect("metal1", -128, -128, 32, 32) + rect("metal3", 127, 127, 32, 32)
    for mode in (0x00, 0x01, 0x02):
        write(target, f"abut-m{mode:02x}", mode, abut)
    rng = random.Random(20261126)
    for i in range(12):
        body = bytes(rng.randrange(256) for _ in range(5 * rng.randrange(8, 140)))
        write(target, f"random-{i:02d}", rng.randrange(256), body)


def main() -> int:
    extract_section_seeds()
    ext_netlist_seeds()
    res_ext_seeds()
    layout_seeds()
    return 0


if __name__ == "__main__":
    sys.exit(main())
