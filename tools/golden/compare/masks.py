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
``mag-timestamp``
    ``.mag`` header line ``timestamp <n>``: the number becomes ``0``.
``ext-timestamp``
    ``.ext`` header line ``timestamp <n>``: the number becomes ``0``.
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


_TIMESTAMP_LINE = re.compile(rb"^(timestamp )-?\d+(\r?)$", re.MULTILINE)


def mask_timestamp_line(data: bytes) -> bytes:
    return _TIMESTAMP_LINE.sub(rb"\g<1>0\g<2>", data)


def _regex_mask(pattern: str) -> Mask:
    compiled = re.compile(pattern.encode())
    return lambda data: compiled.sub(b"<masked>", data)


_NAMED: dict[str, Mask] = {
    "gds-dates": mask_gds_dates,
    "mag-timestamp": mask_timestamp_line,
    "ext-timestamp": mask_timestamp_line,
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
