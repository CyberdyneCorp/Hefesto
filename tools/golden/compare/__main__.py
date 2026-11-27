# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Compare two files with a golden comparator.

Usage (from tools/golden):
    python3 -m compare EXPECTED ACTUAL [--comparator NAME] [--mask NAME]... [--display gds-records]
    python3 -m compare --self-test

--self-test runs the plot comparators (ps-text, pnm-image, svg-shape; m10-plot-route task 2.3) on
hand-made pairs (identical, one byte off, one pixel off, different size) and checks each verdict.

Exit status: 0 equivalent (self-test passed), 1 different (self-test failed), 2 usage error.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import COMPARATORS, DISPLAYS, compare_bytes

_PS = (b"%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 72 72 540 540\n%%EndComments\n%%BeginProlog\n"
       b"/bop { 1 setlinecap } def\n/f1 { 0.500 /Helvetica sf } def\n%%EndProlog\n%%Page: 1 1\n"
       b"col10\n0 0 10 10 fb\ngrestore\n%%Trailer\n%%EOF\n")


def _pnm(width: int, height: int, changed: int | None = None) -> bytes:
    raster = bytearray(b"\xff" * (width * height * 3))
    if changed is not None:
        raster[changed * 3] = 0
    return f"P6\n{width} {height}\n255\n".encode() + bytes(raster)


# (comparator, label, expected, actual, should match)
SELF_TEST_PAIRS = (
    ("ps-text", "identical", _PS, _PS, True),
    ("ps-text", "one byte off", _PS, _PS.replace(b"0 0 10 10 fb", b"0 0 10 11 fb"), False),
    ("ps-text", "different size", _PS, _PS[:-6], False),
    ("pnm-image", "identical", _pnm(4, 3), _pnm(4, 3), True),
    ("pnm-image", "one pixel off", _pnm(4, 3), _pnm(4, 3, changed=5), False),
    ("pnm-image", "different size", _pnm(4, 3), _pnm(3, 4), False),
    ("svg-shape", "identical", b'<svg width="512" height="484"/>', b'<svg width="512" height="484"/>', True),
    ("svg-shape", "unit suffix", b'<svg width="512pt" height="484pt"/>',
     b'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="484"><rect/></svg>', True),
    ("svg-shape", "different size", b'<svg width="512pt" height="484pt"/>', b'<svg width="512" height="485"/>',
     False),
)


def self_test(stream=sys.stdout) -> int:
    """Run SELF_TEST_PAIRS; 0 when every verdict is the expected one, else 1."""
    failures = 0
    for comparator, label, expected, actual, should_match in SELF_TEST_PAIRS:
        result = compare_bytes(expected, actual, comparator)
        ok = result.equal == should_match
        failures += not ok
        verdict = "equivalent" if result.equal else f"different: {result.detail}"
        print(f"{'ok  ' if ok else 'FAIL'} {comparator} {label}: {verdict}", file=stream)
    print(f"self-test: {len(SELF_TEST_PAIRS) - failures}/{len(SELF_TEST_PAIRS)} passed", file=stream)
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="python3 -m compare", description=__doc__.split("\n", 1)[0])
    parser.add_argument("expected", type=Path, nargs="?")
    parser.add_argument("actual", type=Path, nargs="?")
    parser.add_argument("--comparator", default="bytes",
                        help="one of " + ", ".join(sorted(COMPARATORS)) + "; spice-cap:<node>[,<node>...]")
    parser.add_argument("--mask", action="append", default=[], help="mask name (repeatable)")
    parser.add_argument("--display", choices=sorted(DISPLAYS), help="render a difference with this view")
    parser.add_argument("--self-test", action="store_true", help="check the plot comparators on built-in pairs")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    if args.expected is None or args.actual is None:
        parser.error("EXPECTED and ACTUAL are required (or --self-test)")
    try:
        result = compare_bytes(args.expected.read_bytes(), args.actual.read_bytes(), args.comparator, args.mask,
                               args.display)
    except (OSError, ValueError) as error:
        print(f"compare: {error}", file=sys.stderr)
        return 2
    print("equivalent" if result.equal else f"different: {result.detail}")
    return 0 if result.equal else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
