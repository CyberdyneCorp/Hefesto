# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Completeness checks of a Magic-written ``.ext`` file and of the ``.ext`` probe's log.

Used by ``ext_inputs.py`` (golden-capture.yml mode ``ext-inputs``) and by
``authoring/lefdef_cases.py --sync-ext`` before an ``.ext`` file becomes a case input
(openspec/changes/archive/2026-11-22-m6-lef-def-oasis task 1.6). The record keywords are those of
``extract-file-format`` "Record grammar"; the header order is its "Header records".
A file is complete when it is ASCII, starts with the six header records, ends with a
newline after a whole record, and has no ``error`` line.
"""
from __future__ import annotations

import re

HEADER = ("timestamp", "version", "tech", "style", "scale", "resistclasses")
RECORDS = frozenset((*HEADER, "parameters", "use", "abstract", "port", "node", "substrate", "attr", "equiv",
                     "cap", "fet", "device", "primitive", "merge", "connect", "distance"))
PROBE_PREFIX = "extprobe: "
_ERROR = re.compile(r"\berror\b", re.IGNORECASE)


def check_ext(data: bytes) -> list[str]:
    """The problems of one ``.ext`` file (empty when it is complete)."""
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError:
        return ["not ASCII"]
    if not text:
        return ["empty"]
    problems = [] if text.endswith("\n") else ["does not end with a newline (truncated record)"]
    lines = text.split("\n")[:-1] if text.endswith("\n") else text.split("\n")
    keywords = [line.split(" ", 1)[0] for line in lines]
    if tuple(keywords[:len(HEADER)]) != HEADER:
        problems.append(f"header is {' '.join(keywords[:len(HEADER)])!r}, not {' '.join(HEADER)!r}")
    for number, (line, keyword) in enumerate(zip(lines, keywords), start=1):
        if keyword.lower() == "error":
            problems.append(f"line {number}: error line {line!r}")
        elif keyword not in RECORDS:
            problems.append(f"line {number}: unknown record {line[:60]!r}")
    return problems


def probe_events(log: str) -> dict:
    """The facts the probe printed: the ``extract all`` result code, the feedback count,
    the files written and whether it reached its end."""
    events = {"extract_code": None, "feedback": None, "wrote": {}, "done": False}
    for line in log.splitlines():
        if not line.startswith(PROBE_PREFIX):
            continue
        fields = line[len(PROBE_PREFIX):].split(" ")
        if fields[:2] == ["extract", "all"] and len(fields) > 2:
            events["extract_code"] = int(fields[2])
        elif fields[0] == "feedback" and len(fields) == 2:
            events["feedback"] = int(fields[1])
        elif fields[0] == "wrote" and len(fields) == 3:
            events["wrote"][fields[1]] = int(fields[2])
        elif fields == ["done"]:
            events["done"] = True
    return events


def check_probe_log(log: str) -> list[str]:
    """The problems of one probe run, from Magic's console output (stdout then stderr)."""
    events = probe_events(log)
    problems = []
    if not events["done"]:
        problems.append("the probe did not finish (no 'extprobe: done')")
    if events["extract_code"] not in (0, None):
        problems.append(f"extract all failed (Tcl code {events['extract_code']})")
    if events["feedback"]:
        problems.append(f"extract all left {events['feedback']} feedback area(s)")
    if events["done"] and not events["wrote"]:
        problems.append("extract all wrote no .ext file")
    problems += [f"Magic printed an error: {line.strip()!r}" for line in log.splitlines()
                 if not line.startswith(PROBE_PREFIX) and _ERROR.search(line)]
    return problems
