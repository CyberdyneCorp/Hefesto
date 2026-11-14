# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Compare two files with a golden comparator.

Usage (from tools/golden):
    python3 -m compare EXPECTED ACTUAL [--comparator NAME] [--mask NAME]... [--display gds-records]

Exit status: 0 equivalent, 1 different, 2 usage error.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import COMPARATORS, DISPLAYS, compare_bytes


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="python3 -m compare", description=__doc__.split("\n", 1)[0])
    parser.add_argument("expected", type=Path)
    parser.add_argument("actual", type=Path)
    parser.add_argument("--comparator", default="bytes", choices=sorted(COMPARATORS))
    parser.add_argument("--mask", action="append", default=[], help="mask name (repeatable)")
    parser.add_argument("--display", choices=sorted(DISPLAYS), help="render a difference with this view")
    args = parser.parse_args(argv)
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
