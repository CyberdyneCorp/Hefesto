# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Masks for fields documented as time-dependent (decision X-1).

Capability hefesto-output-determinism, requirement "Canonical comparison
against Magic": hash-order and tile-order records are compared byte for byte
*after masking fields documented as time-dependent, such as GDS modification
dates*. A mask rewrites only those fields, to a fixed value, on both sides, and
leaves every other byte alone, so masking can never hide a layout difference.

A case lists masks by name in ``[[outputs]] masks``. Available masks:

``gds-dates``
    GDSII BGNLIB and BGNSTR records carry 12 two-byte date fields
    (modification and access time). All 24 bytes are zeroed.
``gds-mod-dates``
    Only the *modification* date of BGNLIB and BGNSTR: the last 12 of their 24 date
    bytes (m5-mask-gds-cif design D12). Under magic-compat ON it is the time of the
    write (Q-MASK-202); the creation date in front of it is compared. For cases that
    set ``gds datestamp <n>`` or whose creation dates come from reproduced cell stamps.
``completed-percent``
    Console lines ``Completed <n>%``: the progress of a long hierarchical generation
    (cif/CIFhier.c:1093 and 1117, extract/ExtSubtree.c:320 and 385), printed by time
    and informative in Hefesto (Q-MASK-414). Each whole line is removed.
``cif-preamble``
    The six host-, build- or time-dependent values of a CIF file's preamble
    (cif/CIFwrite.c:275-288, Q-MASK-324): ``( @@user : <v> );``, ``@@machine``,
    ``@@source``, ``@@tool``, ``@@compiled`` and ``@@date``; each value becomes
    ``<masked>``. ``@@technology``, ``@@version``, ``@@techdesc`` and ``@@style``
    are compared.
``mag-timestamp``
    ``.mag`` header line ``timestamp <n>``: the number becomes ``0``.
``ext-timestamp``
    ``.ext`` header line ``timestamp <n>``: the number becomes ``0``.
``magic-tech-sections``
    Console output (stdout/stderr) of Magic's ``tech load`` that comes from
    the extract section, which Hefesto reads but does not interpret until
    M7 (openspec/changes/m1-core-viewer/design.md, Non-Goals: extraction;
    capability extract-*). Exactly these Magic messages are removed, each
    line matched in full:
    ``Contact size value ignored (using GDS generation rules).``
    (extract/ExtTech.c:2296), ``The following types are not handled by
    extraction ...`` with its type-list line (extract/ExtTech.c:3609-3618), and
    the ``Warning: Extraction Style <s>`` block with its planeorder lines and
    the blank line before it (extract/ExtTech.c:3822-3846).
``mag-checkpaint``
    ``.mag`` ``<< checkpaint >>`` section and its ``rect`` lines: the areas
    Magic's DRC still has to check (database/DBio.c, DRC check plane). M1 has
    no DRC (design.md Non-Goals; capability drc-engine, M3). The M3 DRC cases do
    not use it; it is retired from the M1/M2 cases once the M3 engine marks every
    edit as Magic does (authoring/casegen.py CHECKPAINT_MASK_RETIRED).
``magic-crash-file``
    Console line ``Created database crash recovery file /tmp/MAG<pid>.<XXXXXX>``
    (Magic's crash backups, database/DBio.c via mkstemp): the process id and
    the random suffix become ``<masked>``.
``magic-tech-menu-order``
    Lines ``<i> <name> {magic::tech load <name> ;  magic::techmanager update}``
    (the technology manager's menu, tcltk/techbuilder.tcl): Magic lists the
    example technologies of its system directory in ``glob`` order, which is
    the directory order of the file system the reference image was built on,
    so each run of such lines is sorted by name and its indexes become
    ``<i>``.
``regex:<pattern>``
    Every match of the Python bytes regex ``<pattern>`` is replaced with
    ``<masked>``. For one-off fields; prefer adding a named mask when a field
    recurs, and document it here.

To add a mask: write a ``bytes -> bytes`` function, register it in ``_NAMED``,
document it above and add a unit test in tests/test_compare.py.
"""
from __future__ import annotations

import re
import struct
from typing import Callable

Mask = Callable[[bytes], bytes]

_GDS_DATE_RECORDS = {0x01, 0x05}  # BGNLIB, BGNSTR
_GDS_DATE_BYTES = 24


def mask_gds_dates(data: bytes) -> bytes:
    out = bytearray(data)
    pos = 0
    while pos + 4 <= len(out):
        (length,) = struct.unpack_from(">H", out, pos)
        if length < 4:  # zero padding after ENDLIB, or a malformed stream
            break
        if out[pos + 2] in _GDS_DATE_RECORDS and length >= 4 + _GDS_DATE_BYTES:
            out[pos + 4 : pos + 4 + _GDS_DATE_BYTES] = bytes(_GDS_DATE_BYTES)
        pos += length
    return bytes(out)


_GDS_MOD_DATE_OFFSET = 12  # creation date (6 x I2), then modification date (6 x I2)


def mask_gds_mod_dates(data: bytes) -> bytes:
    out = bytearray(data)
    pos = 0
    while pos + 4 <= len(out):
        (length,) = struct.unpack_from(">H", out, pos)
        if length < 4:  # zero padding after ENDLIB, or a malformed stream
            break
        if out[pos + 2] in _GDS_DATE_RECORDS and length >= 4 + _GDS_DATE_BYTES:
            start = pos + 4 + _GDS_MOD_DATE_OFFSET
            out[start : pos + 4 + _GDS_DATE_BYTES] = bytes(_GDS_DATE_BYTES - _GDS_MOD_DATE_OFFSET)
        pos += length
    return bytes(out)


_COMPLETED = re.compile(rb"^Completed \d+%\r?\n", re.MULTILINE)


def mask_completed_percent(data: bytes) -> bytes:
    return _COMPLETED.sub(b"", data)


_CIF_PREAMBLE = re.compile(rb"^(\( @@(?:user|machine|source|tool|compiled|date) : ).*?( \);\r?)$", re.MULTILINE)


def mask_cif_preamble(data: bytes) -> bytes:
    return _CIF_PREAMBLE.sub(rb"\g<1><masked>\g<2>", data)


_TIMESTAMP_LINE = re.compile(rb"^(timestamp )-?\d+(\r?)$", re.MULTILINE)


def mask_timestamp_line(data: bytes) -> bytes:
    return _TIMESTAMP_LINE.sub(rb"\g<1>0\g<2>", data)


_TECH_LINE = re.compile(rb"^Contact size value ignored \(using GDS generation rules\)\.\r?$")
_EXTRACT_STYLE_BODY = re.compile(
    rb"^(?:  (?:Missing|Conflicting) planeorder for plane\(s\):"
    rb"|  Magic will use the default planeorder for this style:"
    rb"|   (?: [^ ,]+,)+)\r?$"
)


def mask_magic_tech_sections(data: bytes) -> bytes:
    lines = data.split(b"\n")
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if _TECH_LINE.match(line):
            i += 1
            continue
        if line.startswith(b"The following types are not handled by extraction and will be treated as "
                           b"non-electrical types:"):
            i += 2  # the message and its type-list line
            continue
        if line.startswith(b"Warning: Extraction Style "):
            if out and out[-1] == b"":
                out.pop()  # TxError("\nWarning: ...")
            i += 1
            while i < len(lines) and _EXTRACT_STYLE_BODY.match(lines[i]):
                i += 1
            continue
        out.append(line)
        i += 1
    return b"\n".join(out)


_CHECKPAINT = re.compile(rb"^<< checkpaint >>\r?\n(?:rect -?\d+ -?\d+ -?\d+ -?\d+\r?\n)*", re.MULTILINE)


def mask_mag_checkpaint(data: bytes) -> bytes:
    return _CHECKPAINT.sub(b"", data)


# Magic prints this note (DBCellWrite via dbStampFunc) only when the cell is
# saved in the same wall-clock second as its previous stamp, so whether it
# appears depends on timing in Magic and in Hefesto alike.
_STAMP_NOTE = re.compile(rb"^Magic error: writing out-of-date timestamp for [^\n]*\.\r?\n", re.MULTILINE)


def mask_magic_stamp_note(data: bytes) -> bytes:
    return _STAMP_NOTE.sub(b"", data)


# Magic names its crash backup with the process id and a mkstemp suffix.
_CRASH_FILE = re.compile(rb"(crash recovery file \S*/MAG)\d+\.\w+")


def mask_magic_crash_file(data: bytes) -> bytes:
    return _CRASH_FILE.sub(rb"\1<masked>", data)


# The technology manager lists Magic's example technologies in directory order.
_TECH_MENU_LINE = re.compile(rb"^\d+ (\S+) \{magic::tech load \1 ;  magic::techmanager update\}\r?$")


def mask_magic_tech_menu_order(data: bytes) -> bytes:
    lines = data.split(b"\n")
    out: list[bytes] = []
    run: list[bytes] = []
    for line in lines + [b""]:
        if _TECH_MENU_LINE.match(line):
            run.append(re.sub(rb"^\d+", b"<i>", line))
            continue
        out.extend(sorted(run))
        run = []
        out.append(line)
    return b"\n".join(out[:-1])


def _regex_mask(pattern: str) -> Mask:
    compiled = re.compile(pattern.encode())
    return lambda data: compiled.sub(b"<masked>", data)


_NAMED: dict[str, Mask] = {
    "gds-dates": mask_gds_dates,
    "gds-mod-dates": mask_gds_mod_dates,
    "completed-percent": mask_completed_percent,
    "cif-preamble": mask_cif_preamble,
    "mag-timestamp": mask_timestamp_line,
    "ext-timestamp": mask_timestamp_line,
    "magic-tech-sections": mask_magic_tech_sections,
    "mag-checkpaint": mask_mag_checkpaint,
    "magic-stamp-note": mask_magic_stamp_note,
    "magic-crash-file": mask_magic_crash_file,
    "magic-tech-menu-order": mask_magic_tech_menu_order,
}


def get_mask(name: str) -> Mask:
    if name.startswith("regex:"):
        return _regex_mask(name[len("regex:") :])
    try:
        return _NAMED[name]
    except KeyError:
        raise ValueError(f"unknown mask '{name}'") from None


def apply_masks(data: bytes, names) -> bytes:
    for name in names:
        data = get_mask(name)(data)
    return data
