#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Compare golden-replay outputs of CI legs byte-wise against the reference leg.

Capability hefesto-testing-strategy, requirement "Cross-platform determinism
checks": CI fails if any platform's golden output differs byte-wise from the
Linux x86_64 output, except for documented time-dependent fields (decision X-1).

Each argument is a directory tree of outputs written by the golden runner on one
leg (`tools/golden/replay.py --output-dir`, layout `<dir>/<AREA>/<case-id>/<file>`).
The first is the reference. Every other tree must contain exactly the same files
with the same bytes.

Time-dependent fields are documented per case, in case.toml (`[[outputs]] masks`,
see tools/golden/compare/masks.py), and replay.py applies those masks before it
writes the trees, for binary formats (GDS dates) as well as text. This script
therefore compares raw bytes.

The first line printed lists the cases of the reference per area, so the job
summary shows what the check covers (m4-scripting task 13.6: the SCRIPT outputs).
`--require-area AREA` (repeatable) fails when the reference has no case of AREA.

`--may-skip PATTERN` (repeatable, fnmatch on `<AREA>/<case-id>`) names cases that
only some legs run, because their external input (a PDK data set restored on some
legs only, such as the xschem sets) is missing elsewhere; replay.py writes no
output for a skipped case. A matching case that one of the two trees lacks as a
whole is listed as "not run" instead of failing; the files of a case both trees
hold are still compared exactly.

Usage:
    compare_golden_outputs.py [--require-area AREA]... [--may-skip PATTERN]... REFERENCE LEG [LEG...]

Exits 1 on any difference or required area without cases, 2 if the reference
directory does not exist.
"""
from __future__ import annotations

import argparse
import fnmatch
import sys
from pathlib import Path


def tree(directory: Path) -> dict[str, Path]:
    return {p.relative_to(directory).as_posix(): p for p in sorted(directory.rglob("*")) if p.is_file()}


def case_of(rel: str) -> str:
    """`<AREA>/<case-id>` of an output path."""
    return "/".join(rel.split("/")[:2])


def skipped_cases(expected: dict[str, Path], actual: dict[str, Path], may_skip: list[str]) -> set[str]:
    """Cases matching `may_skip` that only one of the two trees holds."""
    one_side = {case_of(rel) for rel in expected} ^ {case_of(rel) for rel in actual}
    return {case for case in one_side if any(fnmatch.fnmatchcase(case, pattern) for pattern in may_skip)}


def compare(reference: Path, leg: Path, may_skip: list[str] | None = None) -> list[str]:
    """Return one line per difference between two output trees."""
    expected, actual = tree(reference), tree(leg) if leg.is_dir() else {}
    skipped = skipped_cases(expected, actual, may_skip or [])
    expected = {rel: path for rel, path in expected.items() if case_of(rel) not in skipped}
    actual = {rel: path for rel, path in actual.items() if case_of(rel) not in skipped}
    problems = [f"missing: {rel}" for rel in expected if rel not in actual]
    problems += [f"unexpected: {rel}" for rel in actual if rel not in expected]
    for rel in sorted(expected.keys() & actual.keys()):
        if expected[rel].read_bytes() != actual[rel].read_bytes():
            problems.append(f"differs: {rel}")
    return problems


def not_run(reference: Path, leg: Path, may_skip: list[str]) -> list[str]:
    """One line per allowed skipped case of `leg` against `reference`."""
    expected, actual = tree(reference), tree(leg) if leg.is_dir() else {}
    lines = []
    for case in sorted(skipped_cases(expected, actual, may_skip)):
        side = leg if case in {case_of(rel) for rel in expected} else reference
        lines.append(f"not run on {side.name}: {case}")
    return lines


def area_cases(directory: Path) -> dict[str, int]:
    """Cases per area of an output tree (`<AREA>/<case-id>/...`)."""
    return {area.name: sum(1 for case in area.iterdir() if case.is_dir())
            for area in sorted(directory.iterdir()) if area.is_dir()}


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--require-area", action="append", default=[], metavar="AREA",
                        help="fail when the reference has no case of AREA")
    parser.add_argument("--may-skip", action="append", default=[], metavar="PATTERN",
                        help="cases (<AREA>/<case-id> pattern) a leg may not have run")
    parser.add_argument("reference", type=Path)
    parser.add_argument("legs", nargs="+", type=Path)
    args = parser.parse_args(argv)
    if not args.reference.is_dir():
        print(f"reference directory {args.reference} does not exist", file=sys.stderr)
        return 2
    areas = area_cases(args.reference)
    print("reference cases: " + (", ".join(f"{area} {count}" for area, count in areas.items()) or "none"))
    uncovered = [area for area in args.require_area if not areas.get(area)]
    for area in uncovered:
        print(f"  no {area} case in the reference outputs")
    failed = bool(uncovered)
    for leg in args.legs:
        problems = compare(args.reference, leg, args.may_skip)
        status = "identical" if not problems else f"{len(problems)} difference(s)"
        print(f"{leg.name}: {status} vs {args.reference.name}")
        for problem in problems:
            print(f"  {problem}")
        for line in not_run(args.reference, leg, args.may_skip):
            print(f"  {line}")
        failed = failed or bool(problems)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
