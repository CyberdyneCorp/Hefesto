# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""A clean-room OASIS decoder (m6-lef-def-oasis design D19; capability oasis-format).

Built from ``openspec/specs/oasis-format/spec.md`` only: no KLayout, gdstk or Hefesto code.
It serves the golden tools, never the product:

* the ``oasis-records`` comparator and display view of ``compare/`` (:func:`dump`: one line
  per record, its fields as written, CBLOCK contents indented under their CBLOCK);
* the ``stream-geometry`` comparator (:func:`decode`: cells with their shapes, texts and
  placements, modal variables resolved, names resolved after the whole file is read);
* the tests of ``authoring/oasgen.py``, the record writer of hand-built inputs.

Covered: the magic bytes, gzip wrapping, START and END (table offsets in either, padding,
the validation signature: CRC32 and CHECKSUM32 over every byte up to the scheme byte),
every record type 0-34, the integer, real, string, delta, point-list, repetition and
interval encodings, the modal variables and their reset at START, CELL and every name
record, PROPERTY attachment (file, name record, cell, element; XYABSOLUTE/XYRELATIVE end
element attachment), CBLOCK (raw DEFLATE, spliced into the record stream, so a record may
span the end of a CBLOCK), the TRAPEZOID and CTRAPEZOID vertex tables, and the point-list
closure rules of POLYGON.

Fatal decoding errors (the spec's "decoding stops") raise :class:`OasisError` from
:func:`records`; :func:`decode` keeps what was decoded before the error in
``Library.error``. Warnings are collected in ``Library.warnings``. Not covered: strict-mode
violation checks and the ``S_CELL_OFFSET`` cross-check (KLayout checks both in the
``oasis-validate`` capture mode, design D18), and the 32-bit coordinate-overflow rule.
"""
from __future__ import annotations

import gzip
import math
import struct
import sys
import zlib
from dataclasses import dataclass, field
from typing import Callable, Iterator

MAGIC = b"%SEMI-OASIS\r\n"
END_LENGTH = 256
MAX_EXPANSION = 1 << 22  # repetition displacements and CBLOCK bytes: a cap so bad files fail fast

RECORD_NAMES = (
    "PAD", "START", "END", "CELLNAME", "CELLNAME", "TEXTSTRING", "TEXTSTRING", "PROPNAME", "PROPNAME",
    "PROPSTRING", "PROPSTRING", "LAYERNAME", "LAYERNAME", "CELL", "CELL", "XYABSOLUTE", "XYRELATIVE",
    "PLACEMENT", "PLACEMENT", "TEXT", "RECTANGLE", "POLYGON", "PATH", "TRAPEZOID", "TRAPEZOID",
    "TRAPEZOID", "CTRAPEZOID", "CIRCLE", "PROPERTY", "PROPERTY", "XNAME", "XNAME", "XELEMENT",
    "XGEOMETRY", "CBLOCK",
)
NAME_TABLES = ("CELLNAME", "TEXTSTRING", "PROPNAME", "PROPSTRING", "LAYERNAME", "XNAME")
VALIDATION_SCHEMES = {0: "none", 1: "crc32", 2: "checksum32"}

Point = tuple[int, int]


class OasisError(ValueError):
    """A file that cannot be decoded further (the spec's "decoding stops")."""


# --- primitives ------------------------------------------------------------------------------


@dataclass(frozen=True)
class PointList:
    """A point list as written: its type and the vertices P1..Pn relative to P0 = (0, 0)."""

    type: int
    points: tuple[Point, ...]

    def __str__(self) -> str:
        return f"points(t={self.type} {' '.join(f'{x},{y}' for x, y in self.points)})"

    def polygon(self) -> tuple[Point, ...]:
        """P0..Pn, plus the synthesized closing vertex of types 0 and 1 (POLYGON closure)."""
        vertices = ((0, 0), *self.points)
        if self.type not in (0, 1) or not self.points:
            return vertices
        last_horizontal = (len(self.points) - 1 + self.type) % 2 == 0
        px, py = self.points[-1]
        return (*vertices, (px, 0) if last_horizontal else (0, py))


@dataclass(frozen=True)
class Repetition:
    """A repetition as written (type and fields) and its displacements in order."""

    type: int
    fields: tuple
    displacements: tuple[Point, ...]

    def __str__(self) -> str:
        shown = " ".join(f"{v[0]},{v[1]}" if isinstance(v, tuple) else str(v) for v in self.fields)
        return f"rep(t={self.type}{' ' + shown if shown else ''})"


_DIRECTIONS = ((1, 0), (0, 1), (-1, 0), (0, -1), (1, 1), (-1, 1), (-1, -1), (1, -1))


def _grid(nx: int, ny: int, a: Point, b: Point) -> tuple[Point, ...]:
    if nx * ny > MAX_EXPANSION:
        raise OasisError("repetition too large")
    return tuple((i * a[0] + j * b[0], i * a[1] + j * b[1]) for j in range(ny) for i in range(nx))


def _running(steps: list[Point]) -> tuple[Point, ...]:
    x = y = 0
    out = [(0, 0)]
    for dx, dy in steps:
        x, y = x + dx, y + dy
        out.append((x, y))
    return tuple(out)


class Cursor:
    """Reads the primitive encodings of oasis-format from a byte stream.

    ``splice`` replaces a CBLOCK by its decompressed bytes; ``offset`` still names file
    offsets (``"<cblock offset>+<k>"`` inside decompressed bytes)."""

    def __init__(self, data: bytes, pos: int = 0) -> None:
        self.data = data
        self.pos = pos
        self._cblock: tuple[int, int, int] | None = None  # CBLOCK file offset, inflated length, offset after

    def at_end(self) -> bool:
        return self.pos >= len(self.data)

    def in_cblock(self) -> bool:
        return self._cblock is not None and self.pos < self._cblock[1]

    def offset(self) -> int | str:
        if self._cblock is None:
            return self.pos
        origin, length, after = self._cblock
        return f"{origin}+{self.pos}" if self.pos < length else after + self.pos - length

    def splice(self, origin: int, inflated: bytes) -> None:
        after = self.offset()
        self.data = inflated + self.data[self.pos:]
        self.pos = 0
        self._cblock = (origin, len(inflated), after)

    def take(self, count: int) -> bytes:
        if self.pos + count > len(self.data):
            raise OasisError("unexpected end of file")
        chunk = self.data[self.pos:self.pos + count]
        self.pos += count
        return chunk

    def byte(self) -> int:
        return self.take(1)[0]

    def uint(self) -> int:
        value, shift = 0, 0
        while True:
            b = self.byte()
            value |= (b & 0x7F) << shift
            shift += 7
            if value >> 64:
                raise OasisError("integer overflow")
            if not b & 0x80:
                return value

    def uint32(self) -> int:
        value = self.uint()
        if value > 0xFFFFFFFF:
            raise OasisError("integer overflow")
        return value

    def sint(self) -> int:
        u = self.uint()
        return -(u >> 1) if u & 1 else u >> 1

    def real(self) -> float:
        return self.real_body(self.uint32())

    def real_body(self, rtype: int) -> float:
        if rtype in (0, 1):
            value = float(self.uint())
        elif rtype in (2, 3):
            value = self._ratio(1, self.uint())
        elif rtype in (4, 5):
            value = self._ratio(self.uint(), self.uint())
        elif rtype == 6:
            return struct.unpack("<f", self.take(4))[0]
        elif rtype == 7:
            return struct.unpack("<d", self.take(8))[0]
        else:
            raise OasisError("invalid real type")
        return -value if rtype % 2 else value

    @staticmethod
    def _ratio(numerator: int, divisor: int) -> float:
        if divisor == 0:
            raise OasisError("division by zero in real")
        return float(numerator) / float(divisor)

    def string(self) -> bytes:
        return self.take(self.uint())

    def delta2(self) -> Point:
        u = self.uint()
        dx, dy = _DIRECTIONS[u & 3]
        return dx * (u >> 2), dy * (u >> 2)

    def delta3(self) -> Point:
        u = self.uint()
        dx, dy = _DIRECTIONS[u & 7]
        return dx * (u >> 3), dy * (u >> 3)

    def gdelta(self) -> Point:
        u = self.uint()
        if u & 1 == 0:
            dx, dy = _DIRECTIONS[(u >> 1) & 7]
            return dx * (u >> 4), dy * (u >> 4)
        x = -(u >> 2) if u & 2 else u >> 2
        return x, self.sint()

    def point_list(self) -> PointList:
        ptype, count = self.uint32(), self.uint()
        if ptype > 5:
            raise OasisError("invalid point list")
        if count == 0:  # oasis-format "Point lists": reported, and the element is dropped
            return PointList(ptype, ())
        if count > MAX_EXPANSION:
            raise OasisError("point list too long")
        if ptype in (0, 1):
            steps = [self._axis_delta(i + ptype) for i in range(count)]
        else:
            read = {2: self.delta2, 3: self.delta3}.get(ptype, self.gdelta)
            steps = [read() for _ in range(count)]
        if ptype == 5:
            steps = list(_running(steps)[1:])
        return PointList(ptype, _running(steps)[1:])

    def _axis_delta(self, index: int) -> Point:
        d = self.sint()
        return (d, 0) if index % 2 == 0 else (0, d)

    def repetition(self) -> Repetition:
        rtype = self.uint32()
        if rtype > 11:
            raise OasisError("invalid repetition type")
        if rtype == 0:
            return Repetition(0, (), ())
        return _REPETITIONS[rtype](self, rtype)

    def interval(self) -> tuple[int, int]:
        kind = self.uint32()
        top = 0xFFFFFFFF
        if kind == 0:
            return 0, top
        if kind == 1:
            return 0, self.uint()
        if kind in (2, 3):
            a = self.uint()
            return (a, top) if kind == 2 else (a, a)
        if kind == 4:
            return self.uint(), self.uint()
        raise OasisError("invalid interval type")


def _rep_matrix(c: Cursor, rtype: int) -> Repetition:
    nx, ny, sx, sy = c.uint() + 2, c.uint() + 2, c.uint(), c.uint()
    return Repetition(rtype, (nx, ny, sx, sy), _grid(nx, ny, (sx, 0), (0, sy)))


def _rep_row(c: Cursor, rtype: int) -> Repetition:
    n, s = c.uint() + 2, c.uint()
    along = (s, 0) if rtype == 2 else (0, s)
    return Repetition(rtype, (n, s), _grid(n, 1, along, (0, 0)))


def _rep_list(c: Cursor, rtype: int) -> Repetition:
    n = c.uint() + 2
    grid = c.uint() if rtype in (5, 7) else 1
    if grid == 0:
        raise OasisError("zero repetition grid")
    if n > MAX_EXPANSION:
        raise OasisError("repetition too large")
    spacings = [c.uint() for _ in range(n - 1)]
    steps = [(s * grid, 0) if rtype in (4, 5) else (0, s * grid) for s in spacings]
    shown = (n, grid, *spacings) if rtype in (5, 7) else (n, *spacings)
    return Repetition(rtype, shown, _running(steps))


def _rep_vectors(c: Cursor, rtype: int) -> Repetition:
    if rtype == 8:
        n, m = c.uint() + 2, c.uint() + 2
        a, b = c.gdelta(), c.gdelta()
        return Repetition(rtype, (n, m, a, b), _grid(n, m, a, b))
    n = c.uint() + 2
    a = c.gdelta()
    return Repetition(rtype, (n, a), _grid(n, 1, a, (0, 0)))


def _rep_gdeltas(c: Cursor, rtype: int) -> Repetition:
    n = c.uint() + 2
    grid = c.uint() if rtype == 11 else 1
    if grid == 0:
        raise OasisError("zero repetition grid")
    if n > MAX_EXPANSION:
        raise OasisError("repetition too large")
    steps = [c.gdelta() for _ in range(n - 1)]
    scaled = [(x * grid, y * grid) for x, y in steps]
    shown = (n, grid, *steps) if rtype == 11 else (n, *steps)
    return Repetition(rtype, shown, _running(scaled))


_REPETITIONS: dict[int, Callable[[Cursor, int], Repetition]] = {
    1: _rep_matrix, 2: _rep_row, 3: _rep_row, 4: _rep_list, 5: _rep_list, 6: _rep_list, 7: _rep_list,
    8: _rep_vectors, 9: _rep_vectors, 10: _rep_gdeltas, 11: _rep_gdeltas,
}


# --- records ---------------------------------------------------------------------------------


@dataclass(frozen=True)
class Record:
    """One record: its file offset (``"<cblock>+<k>"`` inside a CBLOCK), ID and fields as written."""

    offset: int | str
    rid: int
    fields: dict
    in_cblock: bool = False

    @property
    def name(self) -> str:
        return RECORD_NAMES[self.rid]


@dataclass
class _Context:
    data: bytes
    offset_flag: int = 0
    warnings: list[str] = field(default_factory=list)


def _opt(c: Cursor, info: int, bit: int, read: Callable[[], object]):
    return read() if info & bit else None


def _fields(**values) -> dict:
    return {k: v for k, v in values.items() if v is not None}


def _p_none(c: Cursor, rid: int, ctx: _Context) -> dict:
    return {}


def _p_tables(c: Cursor) -> tuple[tuple[int, int], ...]:
    return tuple((c.uint(), c.uint()) for _ in NAME_TABLES)


def _p_start(c: Cursor, rid: int, ctx: _Context) -> dict:
    version = c.string().decode("latin-1")
    if version != "1.0":
        raise OasisError(f"unsupported OASIS version {version}")
    unit = c.real()
    if not (math.isfinite(unit) and unit > 0):
        raise OasisError("invalid unit")
    ctx.offset_flag = c.uint()
    tables = _p_tables(c) if ctx.offset_flag == 0 else None
    return _fields(version=version, unit=unit, offset_flag=ctx.offset_flag, tables=tables)


def _p_end(c: Cursor, rid: int, ctx: _Context) -> dict:
    tables = _p_tables(c) if ctx.offset_flag else None
    padding = c.string()
    scheme = c.uint()
    covered = c.offset()
    signature = int.from_bytes(c.take(4), "little") if scheme else None
    return _fields(tables=tables, padding=len(padding), validation=scheme, signature=signature, covered=covered)


def _p_name(c: Cursor, rid: int, ctx: _Context) -> dict:
    name = c.string()
    refnum = c.uint() if rid % 2 == 0 else None
    return _fields(name=name, refnum=refnum)


def _p_layername(c: Cursor, rid: int, ctx: _Context) -> dict:
    return {"name": c.string(), "layers": c.interval(), "datatypes": c.interval()}


def _p_cell(c: Cursor, rid: int, ctx: _Context) -> dict:
    return {"refnum": c.uint()} if rid == 13 else {"name": c.string()}


def _cellref(c: Cursor, info: int) -> int | bytes:
    return c.uint() if info & 0x40 else c.string()


def _p_placement(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    cell = _opt(c, info, 0x80, lambda: _cellref(c, info))
    mag = angle = rotation = None
    if rid == 18:
        mag = _opt(c, info, 0x04, c.real)
        angle = _opt(c, info, 0x02, c.real)
    else:
        rotation = 90 * ((info >> 1) & 3)
    return _fields(info=info, cell=cell, mag=mag, angle=angle, rotation=rotation, flip=info & 1,
                   x=_opt(c, info, 0x20, c.sint), y=_opt(c, info, 0x10, c.sint),
                   repetition=_opt(c, info, 0x08, c.repetition))


def _p_text(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    if info & 0x80:
        ctx.warnings.append("TEXT info bit 7 set")
    string = _opt(c, info, 0x40, lambda: c.uint() if info & 0x20 else c.string())
    return _fields(info=info, string=string, textlayer=_opt(c, info, 0x01, c.uint32),
                   texttype=_opt(c, info, 0x02, c.uint32), x=_opt(c, info, 0x10, c.sint),
                   y=_opt(c, info, 0x08, c.sint), repetition=_opt(c, info, 0x04, c.repetition))


def _layer_fields(c: Cursor, info: int) -> dict:
    return _fields(layer=_opt(c, info, 0x01, c.uint32), datatype=_opt(c, info, 0x02, c.uint32))


def _position_fields(c: Cursor, info: int) -> dict:
    return _fields(x=_opt(c, info, 0x10, c.sint), y=_opt(c, info, 0x08, c.sint),
                   repetition=_opt(c, info, 0x04, c.repetition))


def _p_rectangle(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    out = {"info": info, **_layer_fields(c, info)}
    out.update(_fields(width=_opt(c, info, 0x40, c.uint), height=_opt(c, info, 0x20, c.uint)))
    return {**out, **_position_fields(c, info)}


def _p_polygon(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    out = {"info": info, **_layer_fields(c, info), **_fields(points=_opt(c, info, 0x20, c.point_list))}
    return {**out, **_position_fields(c, info)}


def _p_extensions(c: Cursor) -> tuple:
    scheme = c.uint()
    start = c.sint() if (scheme >> 2) & 3 == 3 else None
    end = c.sint() if scheme & 3 == 3 else None
    return scheme, start, end


def _p_path(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    out = {"info": info, **_layer_fields(c, info), **_fields(halfwidth=_opt(c, info, 0x40, c.uint))}
    out.update(_fields(extensions=_opt(c, info, 0x80, lambda: _p_extensions(c)),
                       points=_opt(c, info, 0x20, c.point_list)))
    return {**out, **_position_fields(c, info)}


def _p_trapezoid(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    out = {"info": info, **_layer_fields(c, info)}
    out.update(_fields(width=_opt(c, info, 0x40, c.uint), height=_opt(c, info, 0x20, c.uint)))
    out["a"] = c.sint() if rid in (23, 24) else 0
    out["b"] = c.sint() if rid in (23, 25) else 0
    return {**out, **_position_fields(c, info)}


def _p_ctrapezoid(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    out = {"info": info, **_layer_fields(c, info), **_fields(type=_opt(c, info, 0x80, c.uint32))}
    out.update(_fields(width=_opt(c, info, 0x40, c.uint), height=_opt(c, info, 0x20, c.uint)))
    return {**out, **_position_fields(c, info)}


def _p_circle(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    out = {"info": info, **_layer_fields(c, info), **_fields(radius=_opt(c, info, 0x20, c.uint))}
    return {**out, **_position_fields(c, info)}


def _prop_value(c: Cursor):
    vtype = c.uint()
    if vtype <= 7:
        return c.real_body(vtype)
    if vtype == 8:
        return ("u", c.uint())
    if vtype == 9:
        return ("s", c.sint())
    if vtype in (10, 11, 12):
        return ("abn"[vtype - 10], c.string())
    if vtype in (13, 14, 15):
        return ("ref", c.uint())
    raise OasisError(f"invalid property value type {vtype}")


def _p_property(c: Cursor, rid: int, ctx: _Context) -> dict:
    if rid == 29:
        return {}
    info = c.byte()
    name = _opt(c, info, 0x04, lambda: c.uint() if info & 0x02 else c.string())
    count = info >> 4
    values = None
    if not info & 0x08:
        if count == 15:
            count = c.uint()
        if count > MAX_EXPANSION:
            raise OasisError("property value list too long")
        values = tuple(_prop_value(c) for _ in range(count))
    return _fields(info=info, name=name, standard=info & 1, values=values)


def _p_xname(c: Cursor, rid: int, ctx: _Context) -> dict:
    attribute, data = c.uint(), c.string()
    return _fields(attribute=attribute, data=data, refnum=c.uint() if rid == 31 else None)


def _p_xelement(c: Cursor, rid: int, ctx: _Context) -> dict:
    return {"attribute": c.uint(), "data": c.string()}


def _p_xgeometry(c: Cursor, rid: int, ctx: _Context) -> dict:
    info = c.byte()
    out = {"info": info, "attribute": c.uint(), **_layer_fields(c, info), "data": c.string()}
    return {**out, **_position_fields(c, info)}


def _p_cblock(c: Cursor, rid: int, ctx: _Context) -> dict:
    ctype, size, csize = c.uint(), c.uint(), c.uint()
    if ctype != 0:
        raise OasisError(f"unsupported CBLOCK compression type {ctype}")
    if size > MAX_EXPANSION * 64:
        raise OasisError("CBLOCK too large")
    compressed = c.take(csize)
    inflater = zlib.decompressobj(-15)
    try:
        inflated = inflater.decompress(compressed, size + 1)
    except zlib.error as error:
        raise OasisError(f"CBLOCK size mismatch ({error})") from None
    if len(inflated) != size or not inflater.eof or inflater.unused_data:
        raise OasisError("CBLOCK size mismatch")
    return {"compression": ctype, "size": size, "compressed": csize, "inflated": inflated}


_PARSERS: dict[int, Callable[[Cursor, int, _Context], dict]] = {
    0: _p_none, 1: _p_start, 2: _p_end, 13: _p_cell, 14: _p_cell, 15: _p_none, 16: _p_none,
    17: _p_placement, 18: _p_placement, 19: _p_text, 20: _p_rectangle, 21: _p_polygon, 22: _p_path,
    23: _p_trapezoid, 24: _p_trapezoid, 25: _p_trapezoid, 26: _p_ctrapezoid, 27: _p_circle,
    28: _p_property, 29: _p_property, 30: _p_xname, 31: _p_xname, 32: _p_xelement, 33: _p_xgeometry,
    34: _p_cblock,
}
_PARSERS.update({rid: _p_name for rid in range(3, 11)})
_PARSERS.update({11: _p_layername, 12: _p_layername})


def unwrap(data: bytes) -> bytes:
    """``data``, decompressed when it is gzip-wrapped (first bytes ``1F 8B``)."""
    if data[:2] == b"\x1f\x8b":
        try:
            return gzip.decompress(data)
        except (OSError, EOFError, zlib.error) as error:
            raise OasisError(f"bad gzip wrapping ({error})") from None
    return data


def _check_placement(rid: int, cursor: Cursor) -> None:
    if rid > 34:
        raise OasisError(f"invalid record type {rid}")
    if rid == 1:
        raise OasisError("START record after the start of the file")
    if cursor.in_cblock() and rid in (2, 34):
        raise OasisError(f"{RECORD_NAMES[rid]} record inside a CBLOCK")


def _records(data: bytes, ctx: _Context) -> Iterator[Record]:
    if data[:len(MAGIC)] != MAGIC:
        raise OasisError("not an OASIS file")
    cursor = Cursor(data, len(MAGIC))
    if cursor.uint() != 1:
        raise OasisError("the first record is not START")
    yield Record(len(MAGIC), 1, _p_start(cursor, 1, ctx))
    while True:
        if cursor.at_end():
            raise OasisError("unexpected end of file")
        offset, inside = cursor.offset(), cursor.in_cblock()
        rid = cursor.uint()
        _check_placement(rid, cursor)
        fields = _PARSERS[rid](cursor, rid, ctx)
        yield Record(offset, rid, fields, inside)
        if rid == 34:
            cursor.splice(offset, fields["inflated"])
        if rid == 2:
            break
    if not cursor.at_end():
        ctx.warnings.append("data after END record")


def records(data: bytes) -> Iterator[Record]:
    """Every record of ``data`` in stream order (CBLOCK contents follow their CBLOCK record).

    Raises OasisError where oasis-format says decoding stops."""
    data = unwrap(data)
    yield from _records(data, _Context(data))


def signature(data: bytes, scheme: int) -> int:
    """The validation signature of scheme 1 (CRC32) or 2 (CHECKSUM32) over ``data``."""
    if scheme == 1:
        return zlib.crc32(data) & 0xFFFFFFFF
    return sum(data) & 0xFFFFFFFF


# --- layout ----------------------------------------------------------------------------------


@dataclass
class Property:
    name: object  # str once resolved; a refnum (int) or raw name (bytes) before
    standard: bool
    values: tuple


@dataclass
class Shape:
    """A geometry element with its modal fields resolved; ``points`` are absolute.

    ``kind`` is ``rectangle``, ``polygon``, ``trapezoid`` or ``ctrapezoid`` (``points`` is the
    outline), ``path`` (``points`` is the centerline), ``circle`` (``points`` is the centre) or
    ``xgeometry`` (no points)."""

    kind: str
    layer: int
    datatype: int
    points: tuple[Point, ...]
    halfwidth: int = 0
    extensions: tuple[int, int] = (0, 0)
    radius: int = 0
    repetition: tuple[Point, ...] = ((0, 0),)
    properties: list[Property] = field(default_factory=list)


@dataclass
class Text:
    string: object
    layer: int
    texttype: int
    position: Point
    repetition: tuple[Point, ...] = ((0, 0),)
    properties: list[Property] = field(default_factory=list)


@dataclass
class Placement:
    cell: object
    position: Point
    mag: float = 1.0
    angle: float = 0.0
    flip: bool = False
    repetition: tuple[Point, ...] = ((0, 0),)
    properties: list[Property] = field(default_factory=list)
    pattern: Repetition | None = None  # the repetition as written (type 0 resolved), None without one


@dataclass
class Cell:
    """A cell: its shapes, texts and placements by kind, and all of them in record order
    (``elements``, which the GDS equivalent of authoring/oasequiv.py follows)."""

    name: object
    shapes: list[Shape] = field(default_factory=list)
    texts: list[Text] = field(default_factory=list)
    placements: list[Placement] = field(default_factory=list)
    properties: list[Property] = field(default_factory=list)
    elements: list[object] = field(default_factory=list)


@dataclass
class Name:
    table: str
    refnum: int | None
    value: object
    properties: list[Property] = field(default_factory=list)


@dataclass
class Library:
    """A decoded file. ``cells`` maps resolved cell names to cells, in file order."""

    unit: float = 0.0
    offset_flag: int = 0
    tables: tuple = ()
    cells: dict[str, Cell] = field(default_factory=dict)
    names: list[Name] = field(default_factory=list)
    layernames: list[tuple[str, str, tuple[int, int], tuple[int, int]]] = field(default_factory=list)
    properties: list[Property] = field(default_factory=list)
    end_offset: int | None = None
    end_length: int | None = None
    validation: int = 0
    stored_signature: int | None = None
    computed_signature: int | None = None
    warnings: list[str] = field(default_factory=list)
    error: str | None = None

    @property
    def valid(self) -> bool:
        return self.validation == 0 or self.stored_signature == self.computed_signature


class _Modal(dict):
    def need(self, name: str):
        if name not in self:
            raise OasisError(f"modal variable {name} undefined")
        return self[name]

    def reset(self) -> None:
        self.clear()
        self.update({"placement-x": 0, "placement-y": 0, "text-x": 0, "text-y": 0, "geometry-x": 0,
                     "geometry-y": 0, "xy-mode": "absolute"})

    def take(self, fields: dict, key: str, name: str):
        if key in fields:
            self[name] = fields[key]
        return self.need(name)

    def coord(self, fields: dict, key: str, name: str) -> int:
        if key in fields:
            relative = self["xy-mode"] == "relative"
            self[name] = self[name] + fields[key] if relative else fields[key]
        return self[name]

    def repetition(self, fields: dict) -> tuple[Point, ...]:
        pattern = self.pattern(fields)
        return ((0, 0),) if pattern is None else pattern.displacements

    def pattern(self, fields: dict) -> Repetition | None:
        """The element's repetition with type 0 resolved to the modal one, or None."""
        rep = fields.get("repetition")
        if rep is None:
            return None
        if rep.type == 0:
            return self.need("repetition")
        self["repetition"] = rep
        return rep


def _dedupe(points: list[Point]) -> tuple[Point, ...]:
    """``points`` with consecutive coincident vertices merged (cyclically)."""
    out: list[Point] = []
    for p in points:
        if not out or out[-1] != p:
            out.append(p)
    while len(out) > 1 and out[0] == out[-1]:
        out.pop()
    return tuple(out)


def trapezoid_vertices(w: int, h: int, a: int, b: int, vertical: bool) -> list[Point]:
    """The TRAPEZOID outline relative to (x, y) (oasis-format "TRAPEZOID records")."""
    if vertical:
        return [(0, max(a, 0)), (0, h + min(b, 0)), (w, h - max(b, 0)), (w, -min(a, 0))]
    return [(max(a, 0), h), (w + min(b, 0), h), (w - max(b, 0), 0), (-min(a, 0), 0)]


_CTRAPEZOIDS: tuple[Callable[[int, int], list[Point]], ...] = (
    lambda w, h: [(0, 0), (0, h), (w - h, h), (w, 0)],
    lambda w, h: [(0, 0), (0, h), (w, h), (w - h, 0)],
    lambda w, h: [(0, 0), (h, h), (w, h), (w, 0)],
    lambda w, h: [(h, 0), (0, h), (w, h), (w, 0)],
    lambda w, h: [(0, 0), (h, h), (w - h, h), (w, 0)],
    lambda w, h: [(h, 0), (0, h), (w, h), (w - h, 0)],
    lambda w, h: [(0, 0), (h, h), (w, h), (w - h, 0)],
    lambda w, h: [(h, 0), (0, h), (w - h, h), (w, 0)],
    lambda w, h: [(0, 0), (0, h), (w, h - w), (w, 0)],
    lambda w, h: [(0, 0), (0, h - w), (w, h), (w, 0)],
    lambda w, h: [(0, 0), (0, h), (w, h), (w, w)],
    lambda w, h: [(0, w), (0, h), (w, h), (w, 0)],
    lambda w, h: [(0, 0), (0, h), (w, h - w), (w, w)],
    lambda w, h: [(0, w), (0, h - w), (w, h), (w, 0)],
    lambda w, h: [(0, 0), (0, h - w), (w, h), (w, w)],
    lambda w, h: [(0, w), (0, h), (w, h - w), (w, 0)],
    lambda w, h: [(0, 0), (0, w), (w, 0)],
    lambda w, h: [(0, 0), (0, w), (w, w)],
    lambda w, h: [(0, 0), (w, w), (w, 0)],
    lambda w, h: [(0, w), (w, w), (w, 0)],
    lambda w, h: [(0, 0), (h, h), (2 * h, 0)],
    lambda w, h: [(0, h), (2 * h, h), (h, 0)],
    lambda w, h: [(0, 0), (0, 2 * w), (w, w)],
    lambda w, h: [(w, 0), (0, w), (w, 2 * w)],
    lambda w, h: [(0, 0), (0, h), (w, h), (w, 0)],
    lambda w, h: [(0, 0), (0, w), (w, w), (w, 0)],
)
_CTRAP_WIDTH_ONLY = {16, 17, 18, 19, 22, 23, 25}
_CTRAP_HEIGHT_ONLY = {20, 21}


def ctrapezoid_vertices(ctype: int, w: int, h: int) -> list[Point]:
    """The CTRAPEZOID outline of type ``ctype`` relative to (x, y)."""
    if ctype > 25:
        raise OasisError("invalid CTRAPEZOID type")
    return _CTRAPEZOIDS[ctype](w, h)


def _extension(code: int, explicit: int | None, halfwidth: int, modal: _Modal, name: str) -> int:
    if code == 0:
        return modal.need(name)
    modal[name] = (0, halfwidth, explicit)[code - 1]
    return modal[name]


class _Builder:
    """Interprets the record stream into a :class:`Library`."""

    def __init__(self, data: bytes) -> None:
        self.data = data
        self.lib = Library()
        self.modal = _Modal()
        self.modal.reset()
        self.cell: Cell | None = None
        self.target: object = self.lib
        self.tables: dict[str, dict] = {t: {"mode": None, "next": 0, "refs": {}} for t in NAME_TABLES}

    # -- names
    def name_record(self, rec: Record) -> None:
        self.modal.reset()
        table = rec.name
        if table == "LAYERNAME":
            kind = "geometry" if rec.rid == 11 else "text"
            entry = Name(table, None, rec.fields["name"].decode("latin-1"))
            self.lib.layernames.append((entry.value, kind, rec.fields["layers"], rec.fields["datatypes"]))
        else:
            value = rec.fields.get("name", rec.fields.get("data"))
            entry = Name(table, self._refnum(table, rec), value)
        self.lib.names.append(entry)
        self.target = entry
        self.cell = None

    def _refnum(self, table: str, rec: Record) -> int:
        state = self.tables[table]
        explicit = "refnum" in rec.fields
        mode = "explicit" if explicit else "implicit"
        if state["mode"] not in (None, mode):
            raise OasisError(f"implicit and explicit {table} records mixed")
        state["mode"] = mode
        refnum = rec.fields["refnum"] if explicit else state["next"]
        state["next"] += 0 if explicit else 1
        if refnum in state["refs"]:
            raise OasisError(f"duplicate {table} refnum {refnum}")
        state["refs"][refnum] = rec.fields.get("name", rec.fields.get("data"))
        return refnum

    # -- cells and elements
    def cell_record(self, rec: Record) -> None:
        self.modal.reset()
        self.cell = Cell(rec.fields.get("refnum", rec.fields.get("name")))
        self.lib.cells[f"#{len(self.lib.cells)}"] = self.cell
        self.target = self.cell

    def _in_cell(self, rec: Record) -> Cell:
        if self.cell is None:
            raise OasisError(f"{rec.name} record outside a cell")
        return self.cell

    def xy_record(self, rec: Record) -> None:
        self.modal["xy-mode"] = "absolute" if rec.rid == 15 else "relative"
        self.target = self._in_cell(rec)

    def placement(self, rec: Record) -> None:
        cell, f, m = self._in_cell(rec), rec.fields, self.modal
        ref = m.take(f, "cell", "placement-cell")
        x, y = m.coord(f, "x", "placement-x"), m.coord(f, "y", "placement-y")
        mag = f.get("mag", 1.0)
        angle = f.get("angle", float(f.get("rotation", 0)))
        pattern = m.pattern(f)
        if not (math.isfinite(mag) and mag > 0):
            # oasis-format/oasis-read "Placement transforms" (D3(b) of the user decisions of 2026-11-19):
            # reported and dropped, as Hefesto's codec does (OASIS-CODEC-029); the modal variables stay set.
            self._drop(rec, "invalid magnification")
            return
        reps = ((0, 0),) if pattern is None else pattern.displacements
        element = Placement(ref, (x, y), mag, angle, bool(f["flip"]), reps, pattern=pattern)
        cell.placements.append(element)
        cell.elements.append(element)
        self.target = element

    def text(self, rec: Record) -> None:
        cell, f, m = self._in_cell(rec), rec.fields, self.modal
        string = m.take(f, "string", "text-string")
        layer, texttype = m.take(f, "textlayer", "textlayer"), m.take(f, "texttype", "texttype")
        x, y = m.coord(f, "x", "text-x"), m.coord(f, "y", "text-y")
        element = Text(string, layer, texttype, (x, y), m.repetition(f))
        cell.texts.append(element)
        cell.elements.append(element)
        self.target = element

    def _geometry(self, rec: Record) -> tuple[int, int, int, int]:
        f, m = rec.fields, self.modal
        layer, datatype = m.take(f, "layer", "layer"), m.take(f, "datatype", "datatype")
        return layer, datatype, m.coord(f, "x", "geometry-x"), m.coord(f, "y", "geometry-y")

    def _add_shape(self, rec: Record, shape: Shape) -> None:
        cell = self._in_cell(rec)
        cell.shapes.append(shape)
        cell.elements.append(shape)
        self.target = shape

    def _drop(self, rec: Record, warning: str) -> None:
        """A dropped element: ``warning`` is reported and later PROPERTY records go to the cell."""
        self.lib.warnings.append(warning)
        self.target = self._in_cell(rec)

    def _outline(self, rec: Record, kind: str, vertices: list[Point], position: tuple) -> None:
        layer, datatype, x, y = position
        reps = self.modal.repetition(rec.fields)
        outline = _dedupe([(x + vx, y + vy) for vx, vy in vertices])
        if len(outline) < 3:
            self.lib.warnings.append(f"{kind} with fewer than 3 vertices dropped")
            self.target = self._in_cell(rec)
            return
        self._add_shape(rec, Shape(kind, layer, datatype, outline, repetition=reps))

    def rectangle(self, rec: Record) -> None:
        f, m = rec.fields, self.modal
        square = bool(f["info"] & 0x80)
        position = self._geometry(rec)
        w = m.take(f, "width", "geometry-w")
        if square and "height" in f:
            self.lib.warnings.append("RECTANGLE with S and H set: height ignored")
        h = w if square else m.take(f, "height", "geometry-h")
        m["geometry-h"] = h
        layer, datatype, x, y = position
        reps = m.repetition(f)
        outline = ((x, y), (x + w, y), (x + w, y + h), (x, y + h))
        self._add_shape(rec, Shape("rectangle", layer, datatype, outline, repetition=reps))

    def polygon(self, rec: Record) -> None:
        position = self._geometry(rec)
        points = self.modal.take(rec.fields, "points", "polygon-point-list")
        if not points.points:
            self._drop(rec, "invalid point list")
            return
        if points.type in (0, 1) and len(points.points) % 2:
            self.lib.warnings.append("type 0/1 point list with odd count")
        self._outline(rec, "polygon", list(points.polygon()), position)

    def path(self, rec: Record) -> None:
        f, m = rec.fields, self.modal
        layer, datatype, x, y = self._geometry(rec)
        halfwidth = m.take(f, "halfwidth", "path-halfwidth")
        scheme, start, end = f.get("extensions", (0, None, None))
        ext = (_extension((scheme >> 2) & 3, start, halfwidth, m, "path-start-extension"),
               _extension(scheme & 3, end, halfwidth, m, "path-end-extension"))
        points = m.take(f, "points", "path-point-list")
        reps = m.repetition(f)
        if not points.points:
            self._drop(rec, "invalid point list")
            return
        line = tuple((x + px, y + py) for px, py in ((0, 0), *points.points))
        self._add_shape(rec, Shape("path", layer, datatype, line, halfwidth, ext, repetition=reps))

    def trapezoid(self, rec: Record) -> None:
        f, m = rec.fields, self.modal
        position = self._geometry(rec)
        w, h = m.take(f, "width", "geometry-w"), m.take(f, "height", "geometry-h")
        vertical = bool(f["info"] & 0x80)
        self._outline(rec, "trapezoid", trapezoid_vertices(w, h, f["a"], f["b"], vertical), position)

    def ctrapezoid(self, rec: Record) -> None:
        f, m = rec.fields, self.modal
        position = self._geometry(rec)
        ctype = m.take(f, "type", "ctrapezoid-type")
        w = m.take(f, "width", "geometry-w") if ctype not in _CTRAP_HEIGHT_ONLY else f.get("width", 0)
        h = m.take(f, "height", "geometry-h") if ctype not in _CTRAP_WIDTH_ONLY else f.get("height", 0)
        if "height" in f:
            m["geometry-h"] = f["height"]
        if "width" in f:
            m["geometry-w"] = f["width"]
        vertices = ctrapezoid_vertices(ctype, w, h)
        m["geometry-w"] = max(p[0] for p in vertices) - min(p[0] for p in vertices)
        m["geometry-h"] = max(p[1] for p in vertices) - min(p[1] for p in vertices)
        self._outline(rec, "ctrapezoid", vertices, position)

    def circle(self, rec: Record) -> None:
        layer, datatype, x, y = self._geometry(rec)
        radius = self.modal.take(rec.fields, "radius", "circle-radius")
        reps = self.modal.repetition(rec.fields)
        self._add_shape(rec, Shape("circle", layer, datatype, ((x, y),), radius=radius, repetition=reps))

    def xgeometry(self, rec: Record) -> None:
        layer, datatype, _x, _y = self._geometry(rec)
        reps = self.modal.repetition(rec.fields)
        self._add_shape(rec, Shape("xgeometry", layer, datatype, (), repetition=reps))

    def xelement(self, rec: Record) -> None:
        self.target = self._in_cell(rec)

    # -- properties
    def property(self, rec: Record) -> None:
        f, m = rec.fields, self.modal
        if rec.rid == 29:
            prop = Property(m.need("last-property-name"), m.need("last-property-standard"),
                            m.need("last-value-list"))
        else:
            name = m.take(f, "name", "last-property-name")
            m["last-property-standard"] = bool(f["standard"])
            prop = Property(name, bool(f["standard"]), m.take(f, "values", "last-value-list"))
        self.target.properties.append(prop)

    # -- END
    def end(self, rec: Record) -> None:
        f = rec.fields
        self.lib.end_offset = rec.offset
        self.lib.end_length = len(self.data) - rec.offset
        if self.lib.end_length != END_LENGTH:
            self.lib.warnings.append(f"END record length {self.lib.end_length}")
        self.lib.tables = f.get("tables", self.lib.tables)
        self.lib.validation = f["validation"]
        if f["validation"] not in VALIDATION_SCHEMES:
            self.lib.warnings.append("unknown validation scheme")
        elif f["validation"]:
            self.lib.stored_signature = f["signature"]
            self.lib.computed_signature = signature(self.data[:f["covered"]], f["validation"])
            if not self.lib.valid:
                self.lib.warnings.append(f"OASIS validation failed (expected {f['signature']:08X}, "
                                         f"computed {self.lib.computed_signature:08X})")

    def start(self, rec: Record) -> None:
        self.lib.unit = rec.fields["unit"]
        self.lib.offset_flag = rec.fields["offset_flag"]
        self.lib.tables = rec.fields.get("tables", ())

    def ignore(self, rec: Record) -> None:
        return

    def run(self, stream: Iterator[Record]) -> None:
        for rec in stream:
            _HANDLERS[rec.rid](self, rec)

    # -- name resolution
    def _lookup(self, table: str, ref) -> str:
        if isinstance(ref, bytes):
            return ref.decode("latin-1")
        found = self.tables[table]["refs"].get(ref)
        if found is None:
            self.lib.warnings.append(f"undefined {table} refnum {ref}")
            return f"<{table} {ref}>"
        return found.decode("latin-1")

    def _resolve_property(self, prop: Property) -> None:
        prop.name = self._lookup("PROPNAME", prop.name)
        prop.values = tuple(("str", self._lookup("PROPSTRING", v[1])) if isinstance(v, tuple) and v[0] == "ref"
                            else v for v in prop.values)

    def resolve(self) -> None:
        cells = {}
        for cell in self.lib.cells.values():
            cell.name = self._lookup("CELLNAME", cell.name)
            if cell.name in cells:
                self.lib.warnings.append(f"cell {cell.name} defined twice")
            cells.setdefault(cell.name, cell)
            self._resolve_cell(cell)
        self.lib.cells = cells
        for owner in (self.lib, *self.lib.names):
            for prop in owner.properties:
                self._resolve_property(prop)

    def _resolve_cell(self, cell: Cell) -> None:
        for placement in cell.placements:
            placement.cell = self._lookup("CELLNAME", placement.cell)
        for text in cell.texts:
            text.string = self._lookup("TEXTSTRING", text.string)
        for owner in (cell, *cell.shapes, *cell.texts, *cell.placements):
            for prop in owner.properties:
                self._resolve_property(prop)


_HANDLERS: dict[int, Callable[[_Builder, Record], None]] = {
    0: _Builder.ignore, 1: _Builder.start, 2: _Builder.end, 13: _Builder.cell_record, 14: _Builder.cell_record,
    15: _Builder.xy_record, 16: _Builder.xy_record, 17: _Builder.placement, 18: _Builder.placement,
    19: _Builder.text, 20: _Builder.rectangle, 21: _Builder.polygon, 22: _Builder.path, 23: _Builder.trapezoid,
    24: _Builder.trapezoid, 25: _Builder.trapezoid, 26: _Builder.ctrapezoid, 27: _Builder.circle,
    28: _Builder.property, 29: _Builder.property, 32: _Builder.xelement, 33: _Builder.xgeometry,
    34: _Builder.ignore,
}
_HANDLERS.update({rid: _Builder.name_record for rid in (*range(3, 13), 30, 31)})


def decode(data: bytes) -> Library:
    """``data`` decoded into a :class:`Library`; a fatal error is kept in ``Library.error``."""
    try:
        data = unwrap(data)
    except OasisError as error:
        return Library(error=str(error))
    builder = _Builder(data)
    ctx = _Context(data)
    try:
        builder.run(_records(data, ctx))
    except OasisError as error:
        builder.lib.error = str(error)
    builder.resolve()
    builder.lib.warnings[:0] = ctx.warnings
    return builder.lib


# --- the oasis-records view ------------------------------------------------------------------


def _format_value(value) -> str:
    if isinstance(value, bytes):
        return '"' + value.decode("latin-1").encode("unicode_escape").decode("ascii").replace('"', '\\"') + '"'
    if isinstance(value, float):
        return repr(value)
    if isinstance(value, tuple):
        return "(" + " ".join(_format_value(v) for v in value) + ")"
    return str(value)


_SHARED_NAMES = {name for name in RECORD_NAMES if RECORD_NAMES.count(name) > 1}
_SHOWN_AS = {"info": "0x{:02X}", "signature": "0x{:08X}", "padding": "{} bytes"}


def _format_fields(rec: Record) -> str:
    shown = []
    for key, value in rec.fields.items():
        if key in ("inflated", "covered"):
            continue
        text = _SHOWN_AS[key].format(value) if key in _SHOWN_AS else _format_value(value)
        shown.append(f"{key}={text}")
    return " ".join(shown)


def record_label(rec: Record) -> str:
    """The record's name, with its ID when several IDs share the name (``CELLNAME/3``)."""
    return f"{rec.name}/{rec.rid}" if rec.name in _SHARED_NAMES else rec.name


def dump(data: bytes, offsets: bool = False) -> str:
    """One line per record, CBLOCK contents indented: the ``oasis-records`` view.

    Then a ``signature`` line (stored and computed) when the file is validated, a ``warning``
    line per warning, and an ``error`` line when decoding stopped. ``offsets`` prefixes each
    record with its file offset (``<cblock offset>+<k>`` inside a CBLOCK)."""
    lines: list[str] = []
    ctx = _Context(b"")
    end: Record | None = None
    try:
        data = unwrap(data)
        ctx.data = data
        for rec in _records(data, ctx):
            prefix = f"@{rec.offset} " if offsets else ""
            indent = "  " if rec.in_cblock else ""
            lines.append(f"{indent}{prefix}{record_label(rec)} {_format_fields(rec)}".rstrip())
            end = rec if rec.rid == 2 else end
    except OasisError as error:
        ctx.warnings.append(f"error {error}")
    if end is not None and end.fields["validation"] in (1, 2):
        computed = signature(data[:end.fields["covered"]], end.fields["validation"])
        state = "ok" if computed == end.fields["signature"] else "mismatch"
        lines.append(f"signature stored=0x{end.fields['signature']:08X} computed=0x{computed:08X} {state}")
    lines += [w if w.startswith("error ") else f"warning {w}" for w in ctx.warnings]
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    """``python3 -m goldenlib.oasis FILE``: print the records of FILE with their offsets."""
    if len(argv) != 1:
        print("usage: python3 -m goldenlib.oasis FILE", file=sys.stderr)
        return 2
    with open(argv[0], "rb") as handle:
        sys.stdout.write(dump(handle.read(), offsets=True))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
