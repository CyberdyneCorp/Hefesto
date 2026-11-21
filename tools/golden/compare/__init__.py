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
``spice-canonical``
    A SPICE netlist compared electrically, never by bytes (m8-schematic design D10, Q-SCHEM-15, the flat
    netlist of SCHEM-NL-21): both sides are parsed into devices with subcircuits expanded, and they match
    when a bijection of the non-global nodes maps one device set onto the other (same names compared
    case-insensitively, kinds, models and parameter values). A pairwise comparator: the verdict comes
    from ``spice_canonical.compare``, not from equal canonical bytes. See spice_canonical.py.

``oasis-records``
    OASIS files compared as their decoded records (goldenlib/oasis.py ``dump``: one line per
    record with its fields as written, CBLOCK contents decompressed and indented, then the
    validation signature, warnings and any decoding error), so a CBLOCK's compressed bytes are
    compared through its decoded content and sizes (m6-lef-def-oasis design D19). Also a
    display view.
``stream-geometry``
    A GDSII or OASIS stream (either format on either side) compared as flat per-layer merged
    geometry: Manhattan areas in the canonical form of ``polygon-set``, other polygons as a
    canonical polygon list, then texts (m6-lef-def-oasis design D19). See stream_geometry.py.
``stream-hierarchy``
    A GDSII or OASIS stream compared cell by cell without flattening: cell names, each cell's
    own geometry (as ``stream-geometry`` canonicalizes it) and texts, and one line per placed
    instance (AREFs and PLACEMENT repetitions expanded) with its transform and its reference
    properties (PROPATTR/PROPVALUE, ``S_GDS_PROPERTY``) (m6-lef-def-oasis design D19, the OASIS
    write cases). See stream_hierarchy.py.
``stream-elements``
    A GDSII or OASIS stream compared as each cell's element sequence in record order: rectangles,
    polygons from their first vertex, paths, texts and placements (arrays as their sorted
    displacements) with their properties; an OASIS shape repetition shows as such (m6-lef-def-oasis
    design D19, OASIS-052: ``repetitions no`` keeps gds-write's element order). See stream_elements.py.
``spice-cap``
    SPICE netlists of ``ext2spice``: coupling-capacitor lines compared as a multiset (terminals in
    order, index masked), every other line byte for byte (m7-extraction design D21, Q-NETLIST-1).
    See spice_cap.py.
``sim-cap``
    ``.sim`` files of ``ext2sim``: the same rule for the ``C <n1> <n2> <value>`` coupling lines.
    See sim_cap.py.
``res-ext``
    ``.res.ext`` files of ``extresist``: the resistor records of one net in canonical order, every
    other record byte for byte; record order only, never topology (design D21). See res_ext.py.
``lef-text``, ``def-text``
    LEF and DEF files, and the console of LEF/DEF commands: plain text after removing the
    informative progress lines (``Diagnostic:`` lines, ``(This can take a while!)``,
    LefEstimate percentages) and Magic's leading startup preamble (m6-lef-def-oasis design
    D19). See lefdef_text.py.

A *display view* only renders a difference readably; pass or fail is still decided by
the comparator. An output selects one with ``display = "<view>"`` in case.toml:

``gds-records``
    GDSII streams decoded one record per line (goldenlib/gds.py ``dump``), after the
    masks, so the first differing record is named instead of a byte offset.
``oasis-records``
    OASIS files decoded one record per line (goldenlib/oasis.py ``dump``), for outputs
    compared with ``bytes`` (the self-golden byte files of design D19).

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
from goldenlib.oasis import dump as oasis_dump

from . import (drc_console, ext_cap, lefdef_text, mag_backup, magic_console, polygon_set, res_ext, sim_cap,
               spice_canonical, spice_cap, stream_elements, stream_geometry, stream_hierarchy)
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
    "spice-canonical": spice_canonical.canonical_text,
    "oasis-records": lambda data: oasis_dump(data).encode("utf-8"),
    "stream-geometry": stream_geometry.canonicalize,
    "stream-hierarchy": stream_hierarchy.canonicalize,
    "stream-elements": stream_elements.canonicalize,
    "lef-text": lefdef_text.canonicalize,
    "def-text": lefdef_text.canonicalize,
    "spice-cap": spice_cap.canonicalize,
    "sim-cap": sim_cap.canonicalize,
    "res-ext": res_ext.canonicalize,
}

# Comparators whose verdict needs both sides at once (a node bijection); they return the first difference.
PAIRWISE: dict[str, Callable[[bytes, bytes], str | None]] = {
    "spice-canonical": lambda exp, act: spice_canonical.compare(exp.decode("utf-8", errors="replace"),
                                                                act.decode("utf-8", errors="replace")),
}

# Applied before the masks (the masks address fields of the decompressed stream).
DECODERS: dict[str, Callable[[bytes], bytes]] = {"gzip": gunzip}

DISPLAYS: dict[str, Callable[[bytes], bytes]] = {
    "gds-records": lambda data: gds_dump(data).encode("utf-8"),
    "oasis-records": lambda data: oasis_dump(data).encode("utf-8"),
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
    if comparator in PAIRWISE:
        difference = PAIRWISE[comparator](exp_masked, act_masked)
        return Result(True) if difference is None else Result(False, difference)
    exp, act = canonical(exp_masked), canonical(act_masked)
    if exp == act:
        return Result(True)
    if display is not None:
        view = DISPLAYS[display]
        return Result(False, f"{display}: {_first_difference(view(exp_masked), view(act_masked))}")
    return Result(False, _first_difference(exp, act))


def compare_collected(entries, name: str, expected: bytes, actual: bytes) -> str:
    """'' when the collected output ``name`` (``collected/<path>``, goldenlib/collect.py) matches
    under the comparator and masks of the ``[[collect]]`` entry its file name belongs to, else
    the difference."""
    from goldenlib.collect import entry_for

    entry = entry_for(entries, name)
    if entry is None:
        return "no [[collect]] pattern matches it"
    result = compare_bytes(expected, actual, entry.comparator, entry.masks)
    return "" if result.equal else result.detail
