# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The failing cells of the six PDK cases, grouped by the kind of their first differing record (task 17.4).

Reads the JSON summary of ``replay.py --json`` and, for each EXTRACT-PDK and NETLIST-PDK case, groups the
collected files that differ by the record kind of their first differing line, so a round can see which
extractor or netlister area a failure belongs to::

    python3 tools/golden/replay.py --hefesto-cli <cli> --json replay.json tests/golden/*/cases/*-PDK-*
    python3 tools/golden/authoring/m7_cases.py --pdk-report replay.json

Kinds: the first word of a ``.ext`` record (``cap``, ``node``, ``device``, ``fet``, ``merge``, ``scale``, ...);
in a ``.spice`` netlist ``cap`` (C), ``device`` (R, M, X, D, Q) and ``subckt``; in a ``.sim`` file ``cap`` (C),
``fet`` (e, n, p, d, x), ``device`` (r) and ``units`` (the ``|`` header); ``length`` when the files agree up to
the shorter one's end, ``missing`` or ``extra`` when only one side wrote the file, ``other`` otherwise.
"""
from __future__ import annotations

import ast
import json
import re
from collections import defaultdict
from pathlib import Path

PDK_CASE = re.compile(r"^(EXTRACT|NETLIST)-PDK-")
COLLECTED = "collected/"
_LINE = re.compile(r"^line \d+: expected (b'(?:[^'\\]|\\.)*'|b\"(?:[^\"\\]|\\.)*\"), got (.*)$")
_SPICE = {"c": "cap", "r": "device", "m": "device", "x": "device", "d": "device", "q": "device"}
_SIM = {"C": "cap", "e": "fet", "n": "fet", "p": "fet", "d": "fet", "x": "fet", "r": "device", "|": "units"}
_FIXED = {"hefesto did not write it": "missing", "hefesto wrote it; the reference did not": "extra"}


def _first_line(detail: str) -> str | None:
    """The expected text of the first differing line (``compare._first_difference``), or None."""
    match = _LINE.match(detail)
    if not match:
        return None
    text = ast.literal_eval(match.group(1)).decode("utf-8", errors="replace")
    if not text:  # the reference ended: classify by what Hefesto wrote
        got = match.group(2)
        text = ast.literal_eval(got).decode("utf-8", errors="replace") if got[:2] in ("b'", 'b"') else ""
    return text


def _kind_of_line(suffix: str, text: str) -> str:
    word = text.split(" ", 1)[0]
    if suffix == ".ext":
        return word or "other"
    if suffix == ".spice":
        if word.lower() in (".subckt", ".ends"):
            return "subckt"
        return _SPICE.get(word[:1].lower(), "other")
    if suffix == ".sim":
        return _SIM.get(word[:1], "other")
    return "other"


def record_kind(name: str, detail: str) -> str:
    """The record kind of a collected file's first difference."""
    if detail in _FIXED:
        return _FIXED[detail]
    if detail.startswith("length differs"):
        return "length"
    text = _first_line(detail)
    return "other" if text is None else _kind_of_line(Path(name).suffix, text)


def cell_of(name: str) -> str:
    """``collected/ext/<cell>.ext`` -> ``<cell>``; the NETLIST subdirectory stays: ``flat/<cell>``."""
    path = Path(name[len(COLLECTED):] if name.startswith(COLLECTED) else name)
    parent = path.parent.name
    return path.stem if parent in ("", "ext") else f"{parent}/{path.stem}"


def group(summary: dict) -> dict[str, dict[str, list[str]]]:
    """case id -> record kind -> sorted failing cells, for the PDK cases of a replay summary."""
    grouped: dict[str, dict[str, list[str]]] = {}
    for case in summary.get("cases", []):
        if not PDK_CASE.match(case.get("id", "")):
            continue
        kinds: dict[str, list[str]] = defaultdict(list)
        for out in case.get("outputs", []):
            if out.get("status") != "pass" and out["name"].startswith(COLLECTED):
                kinds[record_kind(out["name"], out.get("detail", ""))].append(cell_of(out["name"]))
        grouped[case["id"]] = {kind: sorted(cells) for kind, cells in sorted(kinds.items())}
    return grouped


def render(grouped: dict[str, dict[str, list[str]]], limit: int = 12) -> str:
    """One block per case: each kind with its cell count and the first ``limit`` cells."""
    lines = []
    for case_id, kinds in sorted(grouped.items()):
        total = sum(len(cells) for cells in kinds.values())
        lines.append(f"{case_id}: {total} failing file(s)")
        for kind, cells in kinds.items():
            shown = ", ".join(cells[:limit]) + (f", ... (+{len(cells) - limit})" if len(cells) > limit else "")
            lines.append(f"  {kind}: {len(cells)}: {shown}")
    return "\n".join(lines) + ("\n" if lines else "no PDK case in the summary\n")


def report(path: Path) -> str:
    return render(group(json.loads(Path(path).read_text(encoding="utf-8"))))
