#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Instructions per `drc check; drc catchup` pass, by function, from two callgrind runs.

Run a tool under callgrind on the one-pass and the two-pass script that
``drc_bench.py --emit DIR --checks 2`` writes; the difference of the two
profiles is one pass with process start, technology and cell loading
cancelled out (the same subtraction as drc_bench.py's timing). Used by the
drc-profile job of .github/workflows/perf.yml (m3-drc design D16).

  python3 bench/drc/callgrind_pass_diff.py callgrind.1 callgrind.2 [--top 40]
"""

from __future__ import annotations

import argparse
import collections
import re
import subprocess
import sys

LINE = re.compile(r"^\s*([\d,]+)\s+\([^)]*\)\s+(\S.*?)(?: \[.*\])?$")


def self_costs(profile: str) -> collections.Counter:
    """Self instructions per function (file names and argument lists dropped)."""
    text = subprocess.run(["callgrind_annotate", "--threshold=100", "--auto=no", profile],
                          capture_output=True, text=True, check=True).stdout
    costs: collections.Counter = collections.Counter()
    for line in text.splitlines():
        match = LINE.match(line)
        if not match or ":" not in match.group(2) or "PROGRAM TOTALS" in line:
            continue
        function = match.group(2).split(":", 1)[1]
        costs[re.sub(r"\(.*", "", function)] += int(match.group(1).replace(",", ""))
    return costs


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("one_pass")
    parser.add_argument("two_passes")
    parser.add_argument("--top", type=int, default=40)
    args = parser.parse_args(argv)
    one, two = self_costs(args.one_pass), self_costs(args.two_passes)
    diff = {name: cost - one.get(name, 0) for name, cost in two.items()}
    total = sum(diff.values())
    print(f"instructions per pass: {total:,}")
    for name, cost in sorted(diff.items(), key=lambda item: -item[1])[:args.top]:
        print(f"{cost:>15,} {100 * cost / max(total, 1):5.1f}%  {name[:160]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
