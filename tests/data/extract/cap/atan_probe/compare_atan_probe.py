#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Compare the capture image's atan with Hefesto's (Open Question 3, tasks.md 6.4).

Reads atan_probe.out (x, glibc atan(x) at 17 digits, %lg of 0.6366*atan(x))
and checks it against the correctly rounded atan that util::cr_atan computes
(tests/unit/extract/cap/cr_atan_test.cpp pins cr_atan to this reference bit
for bit). Prints the arguments whose atan bits differ (last-ULP differences)
and, separately, those whose %lg text differs: only the latter go to the
pinned exception table of Open Question 3. Exit 1 when any %lg text differs.

Usage: compare_atan_probe.py atan_probe.out
Requires mpmath.
"""
import sys

import mpmath

mpmath.mp.prec = 300


def main():
    bit_diffs, text_diffs, n = [], [], 0
    with open(sys.argv[1]) as f:
        for line in f:
            xs, glibc_atan, glibc_lg = line.split()
            x = float(xs)
            cr = float(mpmath.atan(mpmath.mpf(x))) if x != 0 else x
            n += 1
            if float(glibc_atan) != cr:
                bit_diffs.append((x, float(glibc_atan), cr))
            if "%g" % (0.6366 * cr) != glibc_lg:
                text_diffs.append((x, glibc_lg, "%g" % (0.6366 * cr)))
    print(f"arguments {n}; atan bit differences {len(bit_diffs)}; %lg text differences {len(text_diffs)}")
    for x, g, c in bit_diffs:
        print(f"bits x={x.hex()} glibc={g.hex()} cr={c.hex()}")
    for x, g, c in text_diffs:
        print(f"text x={x.hex()} glibc={g} hefesto={c}")
    return 1 if text_diffs else 0


if __name__ == "__main__":
    sys.exit(main())
