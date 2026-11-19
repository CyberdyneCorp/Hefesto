#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The OASIS golden cases of tests/golden/OASIS/CASES.md (m6-lef-def-oasis design D19, tasks 15.2, 15.6).

Magic has no OASIS support, so each case takes one of three references (CASES.md):

* **Magic GDS cross-check, read** (``oasis_common.read_case``): Magic reads ``<stem>.gds``,
  Hefesto ``<stem>.oas`` (its own script, ``input/hefesto.tcl``). The OASIS file is hand-built with
  ``oasgen.py`` (or captured from KLayout or gdstk, below); the GDS file is its GDS equivalent as
  oasis-read defines it (``oasequiv.gds_equivalent``). Both runs pin the read datestamp, save every
  cell (``writeall force``) and write the same queries to ``report.txt``; the saved cells and the
  report are compared. oasis-read's message wording is not normative (Read diagnostics) and the two
  runs issue different commands, so stdout and stderr are not compared.
* **Magic GDS cross-check, write** (``write_case``): both runs build the same layout; Magic writes
  it with ``gds write out.oas`` (GDS bytes: the name has a dot, so nothing is appended), Hefesto
  with ``oasis write out.oas``. The file is compared with ``stream-geometry`` (flat per-layer merged
  geometry and texts) and ``stream-hierarchy`` (cell names, references with their transforms, array
  elements and reference properties, per-cell geometry). KLayout and gdstk check Hefesto's files in
  golden-capture.yml mode ``oasis-validate`` (``oasis_validate.py``: ``strm2gds``, XOR against Magic's
  captured GDS, strict-mode reads, ``oas_validate``).
* **Hefesto self-golden** (``spec_case``, tool ``oasis-spec``): byte files fixed by the spec
  listings. ``oasgen.py`` assembles the expected bytes into ``input/spec/``; the reference run copies
  them (goldenlib/refrunners.py) and Hefesto writes the file itself. ``tests/test_oasis_cases.py``
  checks them against the listings before capture.

A read case may also carry *spec lines*: report lines whose value an OASIS requirement fixes with
no GDS counterpart (``OASIS_FILE``, ``oasis compress``), written by Magic's script as the spec value
and by Hefesto's as the query result.

Captured inputs (OASIS-012, 020, 021; ``oasis_captured_cases.py``): files gdstk or KLayout write in
the capture image. ``oasis_validate.py produce`` adds their requests to its manifest, ``check`` runs
them, and ``--sync-captured DIR`` copies the artifact's ``captured/<case>/`` files into
``tests/golden/OASIS/captured/``. Those cases are written once their files are committed.

    python3 tools/golden/authoring/oasis_cases.py                  # write every case
    python3 tools/golden/authoring/oasis_cases.py --list           # the case ids
    python3 tools/golden/authoring/oasis_cases.py --not-authored   # rows left out, and why
    python3 tools/golden/authoring/oasis_cases.py --deviations     # approved deviations recorded in case notes
    python3 tools/golden/authoring/oasis_cases.py --sync-captured DIR

``m6_cases.py`` is the M6 driver; this module is its OASIS section. Never touches expected/.
"""
from __future__ import annotations

import argparse
import dataclasses
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # goldenlib

import casegen  # noqa: E402
import oasis_captured_cases  # noqa: E402
import oasis_format_cases  # noqa: E402
import oasis_read_cases  # noqa: E402
import oasis_write_cases  # noqa: E402
import tclcase  # noqa: E402
from oasis_common import CASES_DIR, CASES_MD  # noqa: E402

FAMILIES = (oasis_format_cases, oasis_read_cases, oasis_write_cases, oasis_captured_cases)
ROW = re.compile(r"^\| (OASIS-\d{3}) \|", re.MULTILINE)
# Approved deviations recorded in the case notes and CASES.md, not in deviations.toml (the cases pass;
# design D19, task 15.6).
DEVIATIONS = {
    "OASIS-012": "Q-OASIS-2 (amended 2026-11-18): no commercial OASIS writer's files; gdstk-written inputs only",
    "OASIS-030": "Q-OASIS-2 (amended 2026-11-18): no commercial OASIS writer's files; oasgen inputs, KLayout and "
                 "gdstk readers only",
    "OASIS-061": "Q-OASIS-2 (amended 2026-11-18): no commercial OASIS reader; KLayout and gdstk readers only",
}

# Cases Magic cannot capture yet (m6_cases.py lists them as M6 exit blockers): their case.toml carries
# the reason as ``capture_blocked`` (goldenlib/case.py), so capture.py skips them.
CAPTURE_BLOCKED = {
    "OASIS-021-unique": "GOLDEN: Magic's output differs between captures (cells.sha256 of "
                        "sky130_fd_sc_hd__macro_sparecell_1, golden-capture run 44515678560), so it is left "
                        "uncaptured; to investigate",
    "OASIS-064-arrays": "GOLDEN: Magic wrote no .mag file in the round trip, so it has no capture; fix the case input",
    "OASIS-064-uses": "GOLDEN: Magic wrote no .mag file in the round trip, so it has no capture; fix the case input",
}


def specs() -> list[tclcase.TclCase]:
    cases = [dataclasses.replace(case, capture_blocked=CAPTURE_BLOCKED[case.id]) if case.id in CAPTURE_BLOCKED
             else case for family in FAMILIES for case in family.specs()]
    ids = [c.id for c in cases]
    duplicates = sorted({i for i in ids if ids.count(i) > 1})
    if duplicates:
        raise SystemExit(f"duplicate case ids: {duplicates}")
    return cases


def row_of(case_id: str) -> str:
    """The CASES.md row a case belongs to (``OASIS-051-arrays`` belongs to OASIS-051)."""
    return case_id[:len("OASIS-000")]


def inventory() -> list[str]:
    return ROW.findall(CASES_MD.read_text(encoding="utf-8"))


def not_authored() -> dict[str, str]:
    rows = {**oasis_write_cases.NOT_AUTHORED, **oasis_captured_cases.not_authored()}
    done = {row_of(c.id) for c in specs()} | set(rows)
    rows.update({row: "not authored yet (task 15.2)" for row in inventory() if row not in done})
    return rows


def deviation_lines() -> list[str]:
    authored = {row_of(c.id) for c in specs()}
    return [f"{row}: {why}{'' if row in authored else ' (case not written yet)'}" for row, why in DEVIATIONS.items()]


def case_dir(case: tclcase.TclCase) -> Path:
    return CASES_DIR / case.id


def write_all() -> list[Path]:
    return [tclcase.write(case) for case in specs()]


def sync_captured(artifact: Path) -> list[Path]:
    verdicts = artifact / "verdicts.json"
    versions = json.loads(verdicts.read_text(encoding="utf-8")).get("versions", {}) if verdicts.is_file() else {}
    return oasis_captured_cases.sync(artifact, versions)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--list", action="store_true", help="print the case ids only")
    group.add_argument("--not-authored", action="store_true", help="print the rows left out and why")
    group.add_argument("--deviations", action="store_true", help="print the approved deviations of the cases")
    group.add_argument("--sync-captured", type=Path, metavar="DIR", help="copy an oasis-validate artifact's inputs")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.list:
        print("\n".join(c.id for c in specs()))
    elif args.not_authored:
        print("\n".join(f"{row}: {why}" for row, why in not_authored().items()))
    elif args.deviations:
        print("\n".join(deviation_lines()))
    elif args.sync_captured:
        for path in sync_captured(args.sync_captured):
            print(path.relative_to(casegen.REPO))
    else:
        for path in write_all():
            print(path.relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
