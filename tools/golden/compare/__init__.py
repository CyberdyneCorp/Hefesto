# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonical comparators for golden outputs (design D8; decision X-1).

A comparator is a *canonicalizer*: a ``bytes -> bytes`` function applied to
both the expected (reference) and the actual (Hefesto) output, after the
output's masks. The outputs match when the canonical forms are identical.

``bytes``
    Identity. For records whose order Hefesto reproduces exactly (tile order,
    name-keyed hash order): byte-for-byte after masking.
``ext-cap``
    ``.ext`` files: cap records put in canonical order, everything else byte
    for byte. See ext_cap.py.
``magic-console``
    stdout/stderr of a Magic run: Magic's leading startup preamble removed,
    everything else byte for byte. See magic_console.py.
``drc-console``
    A DRC case's stdout/stderr: ``magic-console``, then ``drc listall why`` texts,
    ``drc list(all) count`` pairs and ``drc count`` lines in canonical order. See
    drc_console.py.
``mag-backup``
    Crash-backup and ``archive write`` files: the ``file`` records put in
    canonical order (Magic: hash order, Hefesto: sorted by name), every byte
    of each record kept. See mag_backup.py.

To add a format canonicalizer: put it in its own module with its rules in the
module docstring (the spec requires each canonicalizer to be specified next
to its harness), register it in ``COMPARATORS`` and add unit tests.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

from . import drc_console, ext_cap, mag_backup, magic_console
from .masks import apply_masks

COMPARATORS: dict[str, Callable[[bytes], bytes]] = {
    "bytes": lambda data: data,
    "ext-cap": ext_cap.canonicalize,
    "magic-console": magic_console.canonicalize,
    "mag-backup": mag_backup.canonicalize,
    "drc-console": drc_console.canonicalize,
}


@dataclass(frozen=True)
class Result:
    equal: bool
    detail: str = ""


def _first_difference(expected: bytes, actual: bytes) -> str:
    exp_lines, act_lines = expected.split(b"\n"), actual.split(b"\n")
    for number, (exp, act) in enumerate(zip(exp_lines, act_lines), start=1):
        if exp != act:
            return f"line {number}: expected {exp[:120]!r}, got {act[:120]!r}"
    return f"length differs: expected {len(exp_lines)} lines, got {len(act_lines)}"


def compare_bytes(expected: bytes, actual: bytes, comparator: str, masks=()) -> Result:
    """Compare two outputs with a named comparator and masks."""
    try:
        canonical = COMPARATORS[comparator]
    except KeyError:
        raise ValueError(f"unknown comparator '{comparator}'") from None
    exp = canonical(apply_masks(expected, masks))
    act = canonical(apply_masks(actual, masks))
    if exp == act:
        return Result(True)
    return Result(False, _first_difference(exp, act))
