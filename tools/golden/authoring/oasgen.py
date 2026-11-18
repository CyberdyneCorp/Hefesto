# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""A clean-room OASIS record writer for hand-built golden inputs (m6-lef-def-oasis design D19).

Built from ``openspec/specs/oasis-format/spec.md`` only (no KLayout, gdstk or Hefesto code).
The OASIS cases OASIS-001..015 need files that no layout tool writes on purpose: every real
form, non-minimal integers, every repetition and point-list type, forward references, mixed
name modes, CBLOCKs that split a record, bad signatures. Each helper returns the bytes of
one primitive or one record, so a case assembles exactly the sequence it needs and can
splice in a damaged record anywhere::

    from oasgen import Builder, cellname, cell, rectangle
    b = Builder(unit=1000)
    b.add(cell("top"), rectangle(layer=1, datatype=0, width=10, height=20, x=0, y=0))
    data = b.finish(validation=CRC32)

Primitives follow the writer rules of oasis-format: minimal integers (``uint(v, size=n)``
writes a non-minimal one on purpose), the real form chosen by "Real numbers" rule 4
(``real_form`` writes any form), g-deltas in form 1 when axis-parallel or diagonal.
Strings are checked against their kind (a-, b-, n-string) unless ``check=False``.

Records take their optional fields as keyword arguments; a field left at ``None`` is
omitted and its info bit cleared, so the reader takes the modal value. ``Builder`` tracks
file offsets (for table offsets and ``S_CELL_OFFSET``) and ``Builder.finish`` writes the
END record: table offsets (when they are not in START), the padding that makes END exactly
256 bytes, the validation scheme and the signature over every byte up to the scheme byte.
``reference_419`` rebuilds the 419-byte file of oasis-format "Complete annotated example".
``goldenlib/oasis.py`` decodes what this module writes (``tests/test_oasis.py``).
"""
from __future__ import annotations

import math
import struct
import zlib
from collections.abc import Iterable, Sequence

MAGIC = b"%SEMI-OASIS\r\n"
NONE, CRC32, CHECKSUM32 = 0, 1, 2
END_LENGTH = 256

Point = tuple[int, int]

# Record IDs (oasis-format "Record identifiers").
PAD, START, END = 0, 1, 2
CELLNAME, CELLNAME_REF, TEXTSTRING, TEXTSTRING_REF = 3, 4, 5, 6
PROPNAME, PROPNAME_REF, PROPSTRING, PROPSTRING_REF = 7, 8, 9, 10
LAYERNAME_GEOMETRY, LAYERNAME_TEXT, CELL_REF, CELL_NAME = 11, 12, 13, 14
XYABSOLUTE, XYRELATIVE, PLACEMENT, PLACEMENT_MAG = 15, 16, 17, 18
TEXT, RECTANGLE, POLYGON, PATH = 19, 20, 21, 22
TRAPEZOID_AB, TRAPEZOID_A, TRAPEZOID_B, CTRAPEZOID, CIRCLE = 23, 24, 25, 26, 27
PROPERTY, PROPERTY_REPEAT, XNAME, XNAME_REF, XELEMENT, XGEOMETRY, CBLOCK = 28, 29, 30, 31, 32, 33, 34


class OasgenError(ValueError):
    """A value that cannot be encoded as asked."""


# --- primitives ------------------------------------------------------------------------------


def uint(value: int, size: int | None = None) -> bytes:
    """An unsigned integer, little-endian base 128; ``size`` pads it to that many bytes
    with ``0x80``-continued zero groups (a non-minimal encoding)."""
    if value < 0 or value >> 64:
        raise OasgenError(f"{value} is not a 64-bit unsigned integer")
    groups = []
    while True:
        groups.append(value & 0x7F)
        value >>= 7
        if not value:
            break
    if size is not None:
        if size < len(groups):
            raise OasgenError(f"{size} bytes cannot hold the value")
        groups += [0] * (size - len(groups))
    return bytes([g | 0x80 for g in groups[:-1]] + [groups[-1]])


def sint(value: int) -> bytes:
    """A signed integer: ``(|v| << 1) | sign`` as an unsigned integer."""
    return uint((abs(value) << 1) | (1 if value < 0 else 0))


def real_form(rtype: int, *fields) -> bytes:
    """A real in an explicit form: types 0-5 take unsigned fields, 6 and 7 one float."""
    if rtype in (6, 7):
        return uint(rtype) + struct.pack("<f" if rtype == 6 else "<d", float(fields[0]))
    return uint(rtype) + b"".join(uint(f) for f in fields)


def real(value: float) -> bytes:
    """A real in the writer's form (oasis-format "Real numbers" rule 4): integer, reciprocal
    of an integer, else IEEE binary64."""
    value = float(value)
    negative = 1 if value < 0 or math.copysign(1.0, value) < 0 else 0
    magnitude = abs(value)
    if math.isfinite(value) and magnitude.is_integer() and magnitude < 2 ** 53:
        return real_form(negative, int(magnitude))
    if math.isfinite(value) and magnitude != 0:
        inverse = 1.0 / magnitude
        if inverse.is_integer() and inverse < 2 ** 53 and 1.0 / inverse == magnitude:
            return real_form(2 + negative, int(inverse))
    return real_form(7, value)


_KINDS = {"a": range(0x20, 0x7F), "b": range(0, 0x100), "n": range(0x21, 0x7F)}


def string(text: str | bytes, kind: str = "b", check: bool = True) -> bytes:
    """A length-prefixed string of kind ``a``, ``b`` or ``n`` (illegal bytes raise unless
    ``check=False``)."""
    data = text.encode("latin-1") if isinstance(text, str) else bytes(text)
    if check and (any(b not in _KINDS[kind] for b in data) or (kind == "n" and not data)):
        raise OasgenError(f"illegal character in {kind}-string {data!r}")
    return uint(len(data)) + data


def astring(text: str | bytes, check: bool = True) -> bytes:
    return string(text, "a", check)


def bstring(data: str | bytes) -> bytes:
    return string(data, "b")


def nstring(text: str | bytes, check: bool = True) -> bytes:
    return string(text, "n", check)


_OCTANTS = {(1, 0): 0, (0, 1): 1, (-1, 0): 2, (0, -1): 3, (1, 1): 4, (-1, 1): 5, (-1, -1): 6, (1, -1): 7}


def _octant(dx: int, dy: int, limit: int) -> tuple[int, int]:
    """(direction, magnitude) of an axis-parallel or diagonal vector within the first ``limit`` directions."""
    magnitude = max(abs(dx), abs(dy))
    if magnitude == 0:
        return 0, 0
    unit = (dx // magnitude, dy // magnitude)
    direction = _OCTANTS.get(unit) if dx % magnitude == 0 and dy % magnitude == 0 else None
    if direction is None or direction >= limit:
        raise OasgenError(f"({dx}, {dy}) has no {limit}-direction encoding")
    return direction, magnitude


def delta2(dx: int, dy: int) -> bytes:
    direction, magnitude = _octant(dx, dy, 4)
    return uint((magnitude << 2) | direction)


def delta3(dx: int, dy: int) -> bytes:
    direction, magnitude = _octant(dx, dy, 8)
    return uint((magnitude << 3) | direction)


def gdelta(dx: int, dy: int) -> bytes:
    """A g-delta: form 1 when the vector is axis-parallel or diagonal, else form 2."""
    if dx == 0 or dy == 0 or abs(dx) == abs(dy):
        direction, magnitude = _octant(dx, dy, 8)
        return uint((magnitude << 4) | (direction << 1))
    return uint((abs(dx) << 2) | (2 if dx < 0 else 0) | 1) + sint(dy)


def point_list(ptype: int, deltas: Sequence) -> bytes:
    """A point list of type ``ptype`` (0-5) from its deltas as written: integers for types 0
    and 1, ``(dx, dy)`` pairs otherwise (type 5: the second differences)."""
    if ptype in (0, 1):
        body = b"".join(sint(d) for d in deltas)
    else:
        encode = {2: delta2, 3: delta3}.get(ptype, gdelta)
        body = b"".join(encode(dx, dy) for dx, dy in deltas)
    return uint(ptype) + uint(len(deltas)) + body


def polygon_points(vertices: Sequence[Point]) -> bytes:
    """A type 4 (g-delta) point list for a polygon or path whose first vertex is the
    element's position: the deltas between consecutive ``vertices`` (P0 excluded)."""
    steps = [(b[0] - a[0], b[1] - a[1]) for a, b in zip(vertices, vertices[1:])]
    return point_list(4, steps)


def repetition(rtype: int, *fields) -> bytes:
    """A repetition as written: dimensions are passed as stored (minus 2), g-deltas as
    ``(dx, dy)`` pairs, everything else as unsigned integers."""
    return uint(rtype) + b"".join(gdelta(*f) if isinstance(f, tuple) else uint(f) for f in fields)


def matrix(nx: int, ny: int, sx: int, sy: int) -> bytes:
    """An ``nx`` x ``ny`` array with spacings ``sx``, ``sy`` (type 1, or 2/3 for one row or column)."""
    if ny == 1:
        return repetition(2, nx - 2, sx)
    if nx == 1:
        return repetition(3, ny - 2, sy)
    return repetition(1, nx - 2, ny - 2, sx, sy)


def interval(low: int | None = None, high: int | None = None) -> bytes:
    """An interval, in the shortest type: 0 (all), 1 (``0..high``), 2 (``low..``), 3 (exact) or 4."""
    if low is None and high is None:
        return uint(0)
    if low is None or low == 0 and high is not None and high != 0:
        return uint(1) + uint(high)
    if high is None:
        return uint(2) + uint(low)
    if low == high:
        return uint(3) + uint(low)
    return uint(4) + uint(low) + uint(high)


# --- records ---------------------------------------------------------------------------------


def _info(*bits: tuple[int, object]) -> int:
    return sum(bit for bit, value in bits if value is not None and value is not False)


def _opt(value, encode) -> bytes:
    return b"" if value is None else encode(value)


def pad() -> bytes:
    return uint(PAD)


def tables_bytes(tables: Sequence[tuple[int, int]]) -> bytes:
    """The table-offsets structure: six (strict-flag, offset) pairs, CELLNAME first."""
    if len(tables) != 6:
        raise OasgenError("table offsets need six entries")
    return b"".join(uint(strict) + uint(offset) for strict, offset in tables)


def start(unit: float = 1000, tables: Sequence[tuple[int, int]] | None = None, version: str = "1.0") -> bytes:
    """START: version, unit, and the table offsets here (``tables``) or in END (``None``)."""
    body = uint(START) + astring(version) + real(unit)
    return body + (uint(1) if tables is None else uint(0) + tables_bytes(tables))


def end(prefix: bytes, tables: Sequence[tuple[int, int]] | None = None, validation: int = CRC32) -> bytes:
    """END for a file whose bytes before END are ``prefix``: padded to 256 bytes, then the
    validation scheme and the signature over ``prefix`` and END up to the scheme byte."""
    head = uint(END) + (b"" if tables is None else tables_bytes(tables))
    sig = 4 if validation else 0
    content = END_LENGTH - (len(head) + 1 + sig) - 2  # the length field is two bytes (128..252)
    record = head + uint(content) + bytes(content) + uint(validation)
    if validation == CRC32:
        record += struct.pack("<I", zlib.crc32(prefix + record) & 0xFFFFFFFF)
    elif validation == CHECKSUM32:
        record += struct.pack("<I", sum(prefix + record) & 0xFFFFFFFF)
    return record


def _name_record(implicit: int, name: bytes, refnum: int | None) -> bytes:
    return uint(implicit) + name if refnum is None else uint(implicit + 1) + name + uint(refnum)


def cellname(name: str | bytes, refnum: int | None = None, check: bool = True) -> bytes:
    return _name_record(CELLNAME, nstring(name, check), refnum)


def textstring(text: str | bytes, refnum: int | None = None, check: bool = True) -> bytes:
    return _name_record(TEXTSTRING, astring(text, check), refnum)


def propname(name: str | bytes, refnum: int | None = None, check: bool = True) -> bytes:
    return _name_record(PROPNAME, nstring(name, check), refnum)


def propstring(value: str | bytes, refnum: int | None = None, kind: str = "a") -> bytes:
    return _name_record(PROPSTRING, string(value, kind), refnum)


def layername(name: str | bytes, layers: bytes, datatypes: bytes, text: bool = False) -> bytes:
    """LAYERNAME (geometry, or text with ``text=True``); the intervals come from ``interval``."""
    return uint(LAYERNAME_TEXT if text else LAYERNAME_GEOMETRY) + nstring(name) + layers + datatypes


def xname(attribute: int, data: bytes, refnum: int | None = None) -> bytes:
    body = uint(attribute) + bstring(data)
    return uint(XNAME) + body if refnum is None else uint(XNAME_REF) + body + uint(refnum)


def cell(ref: int | str) -> bytes:
    """CELL by refnum (an int) or by name (a string)."""
    return uint(CELL_REF) + uint(ref) if isinstance(ref, int) else uint(CELL_NAME) + nstring(ref)


def xyabsolute() -> bytes:
    return uint(XYABSOLUTE)


def xyrelative() -> bytes:
    return uint(XYRELATIVE)


def _cellref(ref) -> bytes:
    return uint(ref) if isinstance(ref, int) else nstring(ref)


def placement(cell: int | str | None = None, x: int | None = None, y: int | None = None,
              rotation: int = 0, flip: bool = False, mag: float | None = None, angle: float | None = None,
              rep: bytes | None = None) -> bytes:
    """PLACEMENT: record 17 (quarter-turn ``rotation`` in degrees) unless ``mag`` or ``angle``
    is given, then record 18. ``cell`` is a CELLNAME refnum (int) or a name."""
    by_ref = isinstance(cell, int)
    flags = _info((0x80, cell), (0x40, by_ref or None), (0x01, flip or None))
    if mag is None and angle is None:
        if rotation % 90:
            raise OasgenError("record 17 takes quarter turns only")
        info = flags | _info((0x20, x), (0x10, y), (0x08, rep)) | ((rotation // 90 % 4) << 1)
        fields = b""
    else:
        info = flags | _info((0x20, x), (0x10, y), (0x08, rep), (0x04, mag), (0x02, angle))
        fields = _opt(mag, real) + _opt(angle, real)
    rid = PLACEMENT if mag is None and angle is None else PLACEMENT_MAG
    position = _opt(x, sint) + _opt(y, sint) + _opt(rep, bytes)
    return uint(rid) + bytes([info]) + _opt(cell, _cellref) + fields + position


def text(string_: int | str | None = None, layer: int | None = None, texttype: int | None = None,
         x: int | None = None, y: int | None = None, rep: bytes | None = None) -> bytes:
    """TEXT: ``string_`` is a TEXTSTRING refnum (int) or an inline a-string."""
    by_ref = isinstance(string_, int)
    info = _info((0x40, string_), (0x20, by_ref or None), (0x10, x), (0x08, y), (0x04, rep), (0x02, texttype),
                 (0x01, layer))
    ref = _opt(string_, lambda s: uint(s) if by_ref else astring(s))
    return (uint(TEXT) + bytes([info]) + ref + _opt(layer, uint) + _opt(texttype, uint)
            + _opt(x, sint) + _opt(y, sint) + _opt(rep, bytes))


def _layer(layer, datatype) -> bytes:
    return _opt(layer, uint) + _opt(datatype, uint)


def _position(x, y, rep) -> bytes:
    return _opt(x, sint) + _opt(y, sint) + _opt(rep, bytes)


def _common_bits(layer, datatype, x, y, rep) -> int:
    return _info((0x10, x), (0x08, y), (0x04, rep), (0x02, datatype), (0x01, layer))


def rectangle(layer: int | None = None, datatype: int | None = None, width: int | None = None,
              height: int | None = None, x: int | None = None, y: int | None = None,
              rep: bytes | None = None, square: bool = False) -> bytes:
    info = _common_bits(layer, datatype, x, y, rep) | _info((0x80, square or None), (0x40, width), (0x20, height))
    return (uint(RECTANGLE) + bytes([info]) + _layer(layer, datatype) + _opt(width, uint) + _opt(height, uint)
            + _position(x, y, rep))


def polygon(layer: int | None = None, datatype: int | None = None, points: bytes | None = None,
            x: int | None = None, y: int | None = None, rep: bytes | None = None) -> bytes:
    """POLYGON; ``points`` is a point list (``point_list`` or ``polygon_points``)."""
    info = _common_bits(layer, datatype, x, y, rep) | _info((0x20, points))
    return uint(POLYGON) + bytes([info]) + _layer(layer, datatype) + _opt(points, bytes) + _position(x, y, rep)


_EXTENSION_CODES = {None: 0, "flush": 1, "half": 2}


def _extension(value) -> tuple[int, bytes]:
    if isinstance(value, int) and not isinstance(value, bool):
        return 3, sint(value)
    return _EXTENSION_CODES[value], b""


def path(layer: int | None = None, datatype: int | None = None, halfwidth: int | None = None,
         extensions: tuple | None = None, points: bytes | None = None, x: int | None = None,
         y: int | None = None, rep: bytes | None = None) -> bytes:
    """PATH; ``extensions`` is ``(start, end)``, each ``None`` (modal), ``"flush"``,
    ``"half"`` or an explicit integer."""
    info = _common_bits(layer, datatype, x, y, rep) | _info((0x80, extensions), (0x40, halfwidth), (0x20, points))
    scheme = b""
    if extensions is not None:
        (ss, start_value), (ee, end_value) = _extension(extensions[0]), _extension(extensions[1])
        scheme = uint((ss << 2) | ee) + start_value + end_value
    return (uint(PATH) + bytes([info]) + _layer(layer, datatype) + _opt(halfwidth, uint) + scheme
            + _opt(points, bytes) + _position(x, y, rep))


def trapezoid(a: int = 0, b: int = 0, layer: int | None = None, datatype: int | None = None,
              width: int | None = None, height: int | None = None, x: int | None = None,
              y: int | None = None, rep: bytes | None = None, vertical: bool = False,
              rid: int | None = None) -> bytes:
    """TRAPEZOID: record 24 when ``b`` is 0, 25 when ``a`` is 0, else 23 (``rid`` forces one)."""
    rid = rid or (TRAPEZOID_A if b == 0 else TRAPEZOID_B if a == 0 else TRAPEZOID_AB)
    info = _common_bits(layer, datatype, x, y, rep) | _info((0x80, vertical or None), (0x40, width),
                                                           (0x20, height))
    deltas = (sint(a) if rid in (TRAPEZOID_AB, TRAPEZOID_A) else b"") + (
        sint(b) if rid in (TRAPEZOID_AB, TRAPEZOID_B) else b"")
    return (uint(rid) + bytes([info]) + _layer(layer, datatype) + _opt(width, uint) + _opt(height, uint)
            + deltas + _position(x, y, rep))


def ctrapezoid(ctype: int | None = None, layer: int | None = None, datatype: int | None = None,
               width: int | None = None, height: int | None = None, x: int | None = None,
               y: int | None = None, rep: bytes | None = None) -> bytes:
    info = _common_bits(layer, datatype, x, y, rep) | _info((0x80, ctype), (0x40, width), (0x20, height))
    return (uint(CTRAPEZOID) + bytes([info]) + _layer(layer, datatype) + _opt(ctype, uint) + _opt(width, uint)
            + _opt(height, uint) + _position(x, y, rep))


def circle(layer: int | None = None, datatype: int | None = None, radius: int | None = None,
           x: int | None = None, y: int | None = None, rep: bytes | None = None) -> bytes:
    info = _common_bits(layer, datatype, x, y, rep) | _info((0x20, radius))
    return uint(CIRCLE) + bytes([info]) + _layer(layer, datatype) + _opt(radius, uint) + _position(x, y, rep)


def u(value: int) -> bytes:
    """A property value: unsigned integer (type 8)."""
    return uint(8) + uint(value)


def s(value: int) -> bytes:
    """A property value: signed integer (type 9)."""
    return uint(9) + sint(value)


def str_value(value: str | bytes, kind: str = "a") -> bytes:
    """A property value: an a-, b- or n-string (types 10, 11, 12)."""
    return uint(10 + "abn".index(kind)) + string(value, kind)


def ref_value(refnum: int, kind: str = "a") -> bytes:
    """A property value: a PROPSTRING refnum of an a-, b- or n-string (types 13, 14, 15)."""
    return uint(13 + "abn".index(kind)) + uint(refnum)


def real_value(value: float) -> bytes:
    """A property value: a real (types 0-7)."""
    return real(value)


def property_(name: int | str | None = None, values: Sequence[bytes] | None = None, standard: bool = False,
              count: int | None = None) -> bytes:
    """PROPERTY (record 28): ``name`` is a PROPNAME refnum (int), an inline name or ``None``
    (modal); ``values`` are encoded values (``u``, ``s``, ``str_value``, ...) or ``None`` to
    reuse the modal value list. ``count`` forces the value count written (UUUU or explicit)."""
    count = len(values) if count is None and values is not None else count
    info = _info((0x04, name), (0x02, isinstance(name, int) or None), (0x01, standard or None))
    explicit = b""
    if values is None:
        info |= 0x08
    elif count < 15:
        info |= count << 4
    else:
        info |= 0xF0
        explicit = uint(count)
    name_bytes = _opt(name, lambda n: uint(n) if isinstance(n, int) else nstring(n))
    return uint(PROPERTY) + bytes([info]) + name_bytes + explicit + b"".join(values or ())


def property_repeat() -> bytes:
    return uint(PROPERTY_REPEAT)


def xelement(attribute: int, data: bytes) -> bytes:
    return uint(XELEMENT) + uint(attribute) + bstring(data)


def xgeometry(attribute: int, data: bytes, layer: int | None = None, datatype: int | None = None,
              x: int | None = None, y: int | None = None, rep: bytes | None = None) -> bytes:
    info = _common_bits(layer, datatype, x, y, rep)
    return (uint(XGEOMETRY) + bytes([info]) + uint(attribute) + _layer(layer, datatype) + bstring(data)
            + _position(x, y, rep))


def cblock(content: bytes, level: int = 9) -> bytes:
    """CBLOCK: ``content`` (whole records, or a split record on purpose) as raw DEFLATE."""
    compressor = zlib.compressobj(level, zlib.DEFLATED, -15)
    packed = compressor.compress(content) + compressor.flush()
    return uint(CBLOCK) + uint(0) + uint(len(content)) + uint(len(packed)) + packed


# --- files -----------------------------------------------------------------------------------


class Builder:
    """Assembles a file: magic, START, the records ``add``-ed, then END from ``finish``.

    ``offset`` is the file offset the next record gets (for table offsets and S_CELL_OFFSET)."""

    def __init__(self, unit: float = 1000, tables: Sequence[tuple[int, int]] | None = None,
                 version: str = "1.0") -> None:
        self.data = bytearray(MAGIC + start(unit, tables, version))
        self.tables_in_start = tables is not None

    @property
    def offset(self) -> int:
        return len(self.data)

    def add(self, *records: bytes) -> int:
        """Append ``records``; returns the offset of the first one."""
        first = self.offset
        for rec in records:
            self.data += rec
        return first

    def finish(self, tables: Sequence[tuple[int, int]] | None = None, validation: int = CRC32) -> bytes:
        """The whole file. ``tables`` go in END; pass them iff START had none."""
        if (tables is None) != self.tables_in_start:
            raise OasgenError("table offsets belong in START or in END, exactly once")
        return bytes(self.data) + end(bytes(self.data), tables, validation)


def oasis_file(records: Iterable[bytes], unit: float = 1000, validation: int = CRC32) -> bytes:
    """A file with the table offsets at END, all zero (no table positions given)."""
    builder = Builder(unit)
    builder.add(*records)
    return builder.finish(tables=[(0, 0)] * 6, validation=validation)


def reference_419() -> bytes:
    """The 419-byte file of oasis-format "Complete annotated example" (one cell ``top`` with
    one rectangle on 49/0, strict name tables after the cell, CRC32)."""
    b = Builder(unit=1000)
    b.add(property_(0, [u(4)], standard=True), property_(1, [u(4)], standard=True),
          property_(2, [ref_value(0, "n")], standard=True))
    cell_offset = b.add(cell(0))
    b.add(rectangle(layer=49, datatype=0, width=400, height=200))
    propnames = b.add(*(propname(n) for n in ("S_MAX_SIGNED_INTEGER_WIDTH", "S_MAX_UNSIGNED_INTEGER_WIDTH",
                                              "S_TOP_CELL", "S_CELL_OFFSET")))
    propstrings = b.add(propstring("top"))
    layernames = b.add(layername("M1", interval(49, 49), interval(0, 0)),
                       layername("M1", interval(49, 49), interval(0, 0), text=True))
    cellnames = b.add(cellname("top"), property_(3, [u(cell_offset)], standard=True))
    tables = [(1, cellnames), (1, 0), (1, propnames), (1, propstrings), (1, layernames), (1, 0)]
    return b.finish(tables=tables, validation=CRC32)
