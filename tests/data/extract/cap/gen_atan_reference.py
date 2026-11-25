#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Generate atan_reference.txt: arguments and their correctly rounded atan.

Each line is "<x> <atan(x)>" in C99 hex-float form. atan is evaluated with
mpmath at 300 bits and rounded to the nearest binary64, so the file is the
reference for util::cr_atan (tests/unit/extract/cap/cr_atan_test.cpp;
openspec/changes/m7-extraction/design.md Open Question 3 (a), D22).

The set mixes the fringe arguments the extractor forms (a single-precision
multiplier float(0.02 * cap), widened to double, times an integer distance;
ExtTech.c FRINGE_MULT and ExtCouple.c:718-737, 1298, 1492-1508, 1874-1875),
values near the reduction boundaries k/64 and 1, and random doubles over
many binades. Usage: gen_atan_reference.py [count] [seed] > atan_reference.txt
Requires mpmath.
"""
import random
import struct
import sys

import mpmath

mpmath.mp.prec = 300


def cr_atan(x):
    if x == 0:
        return x  # keeps the sign of zero, which mpf drops
    return float(mpmath.atan(mpmath.mpf(x)))


def f32(v):
    return struct.unpack("f", struct.pack("f", v))[0]


def fringe_arguments():
    out = []
    for cap in (1, 2, 5, 6, 8, 9, 10, 12.5, 20, 30, 33.3, 45, 100, 0.0123, 0.5, 1.7, 3.14159):
        mult = f32(f32(cap) * 0.02)
        for d in list(range(0, 41)) + [50, 64, 100, 128, 500, 1000, 2500, 10000, 123456]:
            out.append(mult * d)
    return out


def boundary_arguments():
    out = [0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 1e-30, 2.0**-27, 2.0**-26, 2.0**60, 2.0**61, 1e300,
           float("inf"), -float("inf")]
    for k in range(65):
        c = k / 64
        out += [c, c + 2.0**-40, c - 2.0**-40, c + 1 / 128, c - 1 / 128]
    return [x for x in out if x == x]


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 4000
    rnd = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 2026)
    xs = boundary_arguments() + fringe_arguments()
    while len(xs) < count:
        kind = rnd.randrange(4)
        if kind == 0:
            xs.append(rnd.uniform(0, 1))
        elif kind == 1:
            xs.append(rnd.uniform(1, 100))
        elif kind == 2:
            xs.append(2.0 ** rnd.uniform(-40, 70) * (1 if rnd.random() < 0.5 else -1))
        else:
            xs.append(f32(rnd.uniform(0.001, 2)) * rnd.randrange(0, 20000))
    for x in xs:
        print(x.hex(), cr_atan(x).hex())


if __name__ == "__main__":
    main()
