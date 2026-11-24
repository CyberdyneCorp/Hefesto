# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Pairwise comparator ``meas-table`` for measurement and statistics tables with per-column tolerances.

Capability simulation-measurements-and-statistics (the result tables and their exports:
``statistics.csv``, ``points.csv``, the measurement tables of the SIMADV-MEAS cases); m9-lvs-simulation
design D19.

A table is CSV text (RFC 4180 quoting) whose first row names the columns. The comparator name takes the
tolerances as an argument::

    meas-table                                exact: every cell byte for byte
    meas-table:tpd=1e-3/1e-13                 column tpd within 1e-3 × max(|a|, |b|) + 1e-13
    meas-table:mean=0/1e-9,stdev=1e-2/0,*=1e-6/0

``<column>=<rel>/<abs>`` gives a column ``|a − b| ≤ rel × max(|a|, |b|) + abs``; ``*`` gives every column
not named. Two tables match when they have the same header row, the same number of rows, and every
cell matches: a cell of a column with a tolerance matches when both cells are numbers within it, or
when both texts are equal (a no-value reason such as ``n/a``, a verdict); every other cell matches only
when its text is equal. Row order is significant (the tables are written in a documented order). The
difference names the row, the column and both cells. The canonical form (for display) is the table
with its cells joined by tabs.

Example: ``tpd`` 1.000e-11 against 1.0005e-11 with ``tpd=1e-3/0`` matches (0.05 % < 0.1 %); 1.01e-11
does not.
"""
from __future__ import annotations

import csv
import io
import math

NAME = "meas-table"


def parse_tolerances(name: str) -> dict[str, tuple[float, float]] | None:
    """The column tolerances of ``meas-table[:<col>=<rel>/<abs>,...]``; None when malformed."""
    if name == NAME:
        return {}
    base, _, argument = name.partition(":")
    if base != NAME or not argument:
        return None
    tolerances = {}
    for item in argument.split(","):
        column, _, spec = item.partition("=")
        rel, _, absolute = spec.partition("/")
        try:
            tolerances[column.strip()] = (float(rel), float(absolute or 0.0))
        except ValueError:
            return None
        if not column.strip() or any(math.isnan(v) or v < 0 for v in tolerances[column.strip()]):
            return None
    return tolerances


def _rows(data: bytes) -> list[list[str]]:
    return list(csv.reader(io.StringIO(data.decode("utf-8", errors="replace"))))


def _number(text: str) -> float | None:
    try:
        value = float(text)
    except ValueError:
        return None
    return None if math.isnan(value) else value


def _cell_matches(a: str, b: str, tolerance: tuple[float, float] | None) -> bool:
    if a == b:
        return True
    if tolerance is None:
        return False
    x, y = _number(a), _number(b)
    if x is None or y is None:
        return False
    rel, absolute = tolerance
    return abs(x - y) <= rel * max(abs(x), abs(y)) + absolute


def compare_tables(expected: bytes, actual: bytes, tolerances: dict[str, tuple[float, float]]) -> str | None:
    """None when the tables match under ``tolerances`` (module docstring), else the first difference."""
    exp, act = _rows(expected), _rows(actual)
    if not exp or not act or exp[0] != act[0]:
        return f"header: expected {exp[0] if exp else []}, got {act[0] if act else []}"
    if len(exp) != len(act):
        return f"expected {len(exp) - 1} rows, got {len(act) - 1}"
    header = exp[0]
    for number, (exp_row, act_row) in enumerate(zip(exp[1:], act[1:]), 1):
        difference = _row_difference(number, header, exp_row, act_row, tolerances)
        if difference:
            return difference
    return None


def _row_difference(number: int, header: list[str], exp_row: list[str], act_row: list[str],
                    tolerances: dict[str, tuple[float, float]]) -> str | None:
    if len(exp_row) != len(act_row):
        return f"row {number}: expected {len(exp_row)} cells, got {len(act_row)}"
    for column, (a, b) in enumerate(zip(exp_row, act_row)):
        name = header[column] if column < len(header) else str(column)
        tolerance = tolerances.get(name, tolerances.get("*")) if column < len(header) else None
        if not _cell_matches(a, b, tolerance):
            return f"row {number} column {name}: expected {a!r}, got {b!r}"
    return None


def pairwise(name: str):
    """The pairwise comparison for comparator ``name``, or None when ``name`` is not a meas-table name."""
    tolerances = parse_tolerances(name)
    if tolerances is None:
        return None
    return lambda expected, actual: compare_tables(expected, actual, tolerances)


def canonicalize(data: bytes) -> bytes:
    """Display form: one row per line, cells joined by tabs."""
    return ("\n".join("\t".join(row) for row in _rows(data)) + "\n").encode("utf-8")


def parametrized(name: str):
    """The display canonicalizer for a ``meas-table:<tolerances>`` name, or None when it is malformed."""
    return canonicalize if parse_tolerances(name) is not None else None
