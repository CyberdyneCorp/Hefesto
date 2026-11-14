# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""A small GDSII record writer for hand-built golden inputs (m5-mask-gds-cif design D18).

The MASK gds-read cases need streams that no layout tool writes on purpose: bad headers,
a corrupt or missing ``ENDSTR``, every ``PATHTYPE``, AREFs with junk points, properties,
odd TEXT records. Each helper returns the bytes of one record or one element, so a case
assembles exactly the sequence it needs and can splice in a damaged record anywhere::

    from gdsgen import boundary, library, structure
    data = library("lib", [structure("top", [boundary(1, 0, [(0, 0), (10, 0), (10, 10), (0, 10)])])])

Coordinates are database units. ``library`` writes HEADER, BGNLIB, LIBNAME, UNITS, the
structures and ENDLIB, padded with zeros to a whole 2048-byte block as Magic writes it
(``pad=False`` leaves the stream unpadded). Every date is a POSIX time, encoded as six
two-byte values in UTC (the capture image runs with ``TZ=UTC``). ``record`` writes any
record by name with typed values; ``raw`` writes any bytes as a record, with a length
that may lie (``length=``). ``goldenlib/gds.py`` decodes what this module writes
(``decode``, ``dump``), so every input round-trips through the same record table.
Inputs under 64 KB are committed as binary; larger ones are generated at authoring time
and recorded with their SHA-256 (``digest``).
"""
from __future__ import annotations

import hashlib
import struct
import sys
import time
from collections.abc import Iterable, Sequence
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from goldenlib.gds import (BY_NAME, RECORD_TYPES, GdsError, Record, dump,  # noqa: E402,F401
                           encode_payload, encode_real8, encode_record, records)

BLOCK = 2048
Point = tuple[int, int]

# STRANS bits and PRESENTATION fields (GDSII manual, records 0x1A and 0x17).
REFLECT = 0x8000
ABS_MAG = 0x0004
ABS_ANGLE = 0x0002


def record(name: str, *values) -> bytes:
    """One record by name; the values are encoded by the record's data type."""
    try:
        rtype = BY_NAME[name]
    except KeyError:
        raise GdsError(f"unknown record '{name}'") from None
    dtype = RECORD_TYPES[rtype][1]
    return encode_record(rtype, dtype, encode_payload(dtype, values))


def raw(rtype: int | str, dtype: int, payload: bytes = b"", length: int | None = None) -> bytes:
    """Any record, unchecked: a wrong data type, an odd payload, or a ``length`` that lies."""
    code = BY_NAME[rtype] if isinstance(rtype, str) else rtype
    size = 4 + len(payload) if length is None else length
    return struct.pack(">HBB", size & 0xFFFF, code, dtype) + payload


def date_values(stamp: int) -> list[int]:
    """The six date values of a POSIX time in UTC: year - 1900, month, day, h, m, s."""
    t = time.gmtime(stamp)
    return [t.tm_year - 1900, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec]


def dates(created: int = 0, modified: int | None = None) -> list[int]:
    return date_values(created) + date_values(created if modified is None else modified)


def xy(points: Iterable[Point]) -> bytes:
    return record("XY", *(c for p in points for c in p))


def closed(points: Sequence[Point]) -> list[Point]:
    """``points`` with the first point repeated at the end (a closed BOUNDARY or BOX)."""
    pts = list(points)
    return pts if pts and pts[0] == pts[-1] else pts + pts[:1]


def rect_points(x1: int, y1: int, x2: int, y2: int) -> list[Point]:
    return closed([(x1, y1), (x2, y1), (x2, y2), (x1, y2)])


def properties(props: Iterable[tuple[int, str]]) -> bytes:
    return b"".join(record("PROPATTR", attr) + record("PROPVALUE", value) for attr, value in props)


def _element(head: str, body: Iterable[bytes], props=(), extra=b"") -> bytes:
    return record(head) + extra + b"".join(body) + properties(props) + record("ENDEL")


def boundary(layer: int, datatype: int, points: Sequence[Point], *, close: bool = True,
             props=(), extra: bytes = b"") -> bytes:
    """A BOUNDARY; ``close=False`` keeps an unclosed point list as given."""
    pts = closed(points) if close else list(points)
    return _element("BOUNDARY", [record("LAYER", layer), record("DATATYPE", datatype), xy(pts)], props, extra)


def rect(layer: int, datatype: int, x1: int, y1: int, x2: int, y2: int, **kw) -> bytes:
    return boundary(layer, datatype, rect_points(x1, y1, x2, y2), **kw)


def box(layer: int, boxtype: int, points: Sequence[Point], props=()) -> bytes:
    """A BOX element (5 points expected; any list is written as given)."""
    return _element("BOX", [record("LAYER", layer), record("BOXTYPE", boxtype), xy(points)], props)


def path(layer: int, datatype: int, width: int, points: Sequence[Point], *, pathtype: int | None = None,
         bgnextn: int | None = None, endextn: int | None = None, props=()) -> bytes:
    body = [record("LAYER", layer), record("DATATYPE", datatype)]
    if pathtype is not None:
        body.append(record("PATHTYPE", pathtype))
    body.append(record("WIDTH", width))
    if bgnextn is not None:
        body.append(record("BGNEXTN", bgnextn))
    if endextn is not None:
        body.append(record("ENDEXTN", endextn))
    return _element("PATH", body + [xy(points)], props)


def strans(flags: int = 0, mag: float | None = None, angle: float | None = None) -> list[bytes]:
    """STRANS with its optional MAG and ANGLE; nothing when all are defaults (None)."""
    if flags == 0 and mag is None and angle is None:
        return []
    out = [record("STRANS", flags)]
    if mag is not None:
        out.append(record("MAG", mag))
    if angle is not None:
        out.append(record("ANGLE", angle))
    return out


def sref(name: str, at: Point, *, flags: int = 0, mag: float | None = None, angle: float | None = None,
         props=()) -> bytes:
    return _element("SREF", [record("SNAME", name), *strans(flags, mag, angle), xy([at])], props)


def aref(name: str, cols: int, rows: int, origin: Point, col_end: Point, row_end: Point, *, flags: int = 0,
         mag: float | None = None, angle: float | None = None, props=()) -> bytes:
    """An AREF: ``col_end`` is the origin displaced by ``cols`` column pitches, ``row_end``
    by ``rows`` row pitches (the GDSII manual's three points)."""
    body = [record("SNAME", name), *strans(flags, mag, angle), record("COLROW", cols, rows),
            xy([origin, col_end, row_end])]
    return _element("AREF", body, props)


def presentation(font: int = 0, vertical: int = 0, horizontal: int = 0) -> int:
    """PRESENTATION bits: font 0-3, vertical 0 top/1 middle/2 bottom, horizontal 0 left/1 centre/2 right."""
    return (font & 3) << 4 | (vertical & 3) << 2 | (horizontal & 3)


def text(layer: int, texttype: int, at: Point, string: str | bytes, *, pres: int | None = None,
         pathtype: int | None = None, width: int | None = None, flags: int = 0, mag: float | None = None,
         angle: float | None = None, props=()) -> bytes:
    body = [record("LAYER", layer), record("TEXTTYPE", texttype)]
    if pres is not None:
        body.append(record("PRESENTATION", pres))
    if pathtype is not None:
        body.append(record("PATHTYPE", pathtype))
    if width is not None:
        body.append(record("WIDTH", width))
    body += [*strans(flags, mag, angle), xy([at]), record("STRING", string)]
    return _element("TEXT", body, props)


def node(layer: int, nodetype: int, points: Sequence[Point]) -> bytes:
    return _element("NODE", [record("LAYER", layer), record("NODETYPE", nodetype), xy(points)])


def structure(name: str | bytes, elements: Iterable[bytes], *, created: int = 0, modified: int | None = None,
              end: bool = True) -> bytes:
    """BGNSTR, STRNAME, the elements and (``end=False`` leaves it out) ENDSTR."""
    head = record("BGNSTR", *dates(created, modified)) + record("STRNAME", name)
    return head + b"".join(elements) + (record("ENDSTR") if end else b"")


def library(name: str, structures: Iterable[bytes], *, units: tuple[float, float] = (1e-3, 1e-9),
            version: int = 600, created: int = 0, modified: int | None = None,
            header_extra: Iterable[bytes] = (), pad: bool = True) -> bytes:
    """A whole stream. ``header_extra`` goes between LIBNAME and UNITS (REFLIBS, FONTS, ...)."""
    head = [record("HEADER", version), record("BGNLIB", *dates(created, modified)), record("LIBNAME", name),
            *header_extra, record("UNITS", *units)]
    data = b"".join(head) + b"".join(structures) + record("ENDLIB")
    return padded(data) if pad else data


def padded(data: bytes) -> bytes:
    """``data`` followed by zeros to a whole number of 2048-byte blocks."""
    return data + bytes(-len(data) % BLOCK)


def decode(data: bytes) -> list[tuple[str, tuple]]:
    """``(name, values)`` per record (``goldenlib/gds.py``): the round-trip check of the writer."""
    return [(r.name, r.values()) for r in records(data)]


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


__all__ = ["record", "raw", "boundary", "rect", "box", "path", "sref", "aref", "text", "node", "structure",
           "library", "padded", "decode", "dump", "digest", "presentation", "properties", "rect_points",
           "closed", "dates", "date_values", "encode_real8", "REFLECT", "ABS_MAG", "ABS_ANGLE", "BLOCK"]
