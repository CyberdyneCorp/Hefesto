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
``polygon-set``
    ``box``/``rect`` lists (``cif see`` feedback saved with ``feedback save``)
    compared as the area each text covers, not as tiles. See polygon_set.py.
``gzip``
    Compressed output (``gds compress``, ``.gds.gz``): both sides are decompressed
    (every gzip member) *before* the masks, then compared byte for byte. The gzip
    header and deflate choices are informative (Q-MASK-223, m5-mask-gds-cif D12).
    Data that is not gzip is compared as ``<not gzip>`` followed by its bytes.

A *display view* only renders a difference readably; pass or fail is still decided by
the comparator. An output selects one with ``display = "<view>"`` in case.toml:

``gds-records``
    GDSII streams decoded one record per line (goldenlib/gds.py ``dump``), after the
    masks, so the first differing record is named instead of a byte offset.

To add a format canonicalizer: put it in its own module with its rules in the
module docstring (the spec requires each canonicalizer to be specified next
to its harness), register it in ``COMPARATORS`` and add unit tests.
"""
from __future__ import annotations

import gzip
import zlib
from dataclasses import dataclass
from typing import Callable

from goldenlib.gds import dump as gds_dump

from . import drc_console, ext_cap, mag_backup, magic_console, polygon_set
from .masks import apply_masks


def gunzip(data: bytes) -> bytes:
    """Every gzip member of ``data`` decompressed; non-gzip data marked, not raised."""
    try:
        return gzip.decompress(data)
    except (OSError, EOFError, zlib.error):
        return b"<not gzip>\n" + data


COMPARATORS: dict[str, Callable[[bytes], bytes]] = {
    "bytes": lambda data: data,
    "ext-cap": ext_cap.canonicalize,
    "magic-console": magic_console.canonicalize,
    "mag-backup": mag_backup.canonicalize,
    "drc-console": drc_console.canonicalize,
    "polygon-set": polygon_set.canonicalize,
    "gzip": lambda data: data,
}

# Applied before the masks (the masks address fields of the decompressed stream).
DECODERS: dict[str, Callable[[bytes], bytes]] = {"gzip": gunzip}

DISPLAYS: dict[str, Callable[[bytes], bytes]] = {
    "gds-records": lambda data: gds_dump(data).encode("utf-8"),
}


def normalized(data: bytes, comparator: str, masks=()) -> bytes:
    """``data`` decoded for ``comparator`` (gzip) and masked: what replay saves per output."""
    return apply_masks(DECODERS.get(comparator, lambda d: d)(data), masks)


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


def compare_bytes(expected: bytes, actual: bytes, comparator: str, masks=(), display: str | None = None) -> Result:
    """Compare two outputs with a named comparator and masks; ``display`` renders a difference."""
    try:
        canonical = COMPARATORS[comparator]
    except KeyError:
        raise ValueError(f"unknown comparator '{comparator}'") from None
    if display is not None and display not in DISPLAYS:
        raise ValueError(f"unknown display view '{display}'")
    exp_masked = normalized(expected, comparator, masks)
    act_masked = normalized(actual, comparator, masks)
    exp, act = canonical(exp_masked), canonical(act_masked)
    if exp == act:
        return Result(True)
    if display is not None:
        view = DISPLAYS[display]
        return Result(False, f"{display}: {_first_difference(view(exp_masked), view(act_masked))}")
    return Result(False, _first_difference(exp, act))
