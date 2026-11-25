#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Arguments of the Open Question 3 capture-image probe (tasks.md 6.4).

Writes one double per line (repr) to stdout: every mult * d the fringe-halo
model forms for the EXTRACT golden cases (mult = float(float(cap) * 0.02)
for each areacap, overlap and defaultareacap value of the cases' exttest.tech
extract sections; d = 0 .. 2 * the largest side halo, which covers the edge
and corner distances), and a sweep of single-precision multipliers 1e-4 .. 4
times distances up to 4096, which brackets the PDK cases (their multipliers
are per internal unit after micron conversion). The PDK cases' own outputs
are compared at integration with the ext-cap comparator.

Usage: gen_args.py <repo root> > args.txt
"""
import math
import pathlib
import re
import struct
import sys


def f32(v):
    return struct.unpack("f", struct.pack("f", v))[0]


def case_caps(root):
    caps, halos = set(), {10}
    for tech in sorted((root / "tests/golden/EXTRACT/cases").glob("EX-*/input/*.tech")):
        text = tech.read_text(errors="replace")
        for m in re.finditer(r"^\s*(areacap|overlap|defaultareacap|defaultoverlap)\s+(.*)$", text, re.M):
            for tok in m.group(2).split():
                try:
                    caps.add(float(tok))
                except ValueError:
                    pass
        for m in re.finditer(r"^\s*sidehalo\s+(\S+)", text, re.M):
            halos.add(int(float(m.group(1))))
    return caps, max(halos)


def main():
    root = pathlib.Path(sys.argv[1])
    caps, halo = case_caps(root)
    args = set()
    for cap in caps:
        mult = f32(f32(cap) * 0.02)
        for d in range(0, 2 * halo + 1):
            args.add(mult * d)
    for i in range(200):
        mult = f32(10 ** (-4 + 4.6 * i / 199))
        for d in (1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584, 4096):
            args.add(mult * d)
    for x in sorted(args):
        print(repr(x))


if __name__ == "__main__":
    main()
