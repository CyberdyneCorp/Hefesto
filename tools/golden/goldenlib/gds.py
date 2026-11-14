# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""GDSII stream records: encoding and decoding (m5-mask-gds-cif design D18).

Shared by ``authoring/gdsgen.py`` (the record writer of hand-built inputs) and the
``gds-records`` display view of ``compare/``. A stream is a sequence of records::

    u16 length (header included, even)  u8 record type  u8 data type  payload

Data types: 0 none, 1 bit array (u16), 2 two-byte signed integers, 3 four-byte signed
integers, 4 four-byte reals (never written by Magic), 5 eight-byte reals (excess-64,
base 16, 56-bit mantissa) and 6 ASCII strings (padded with one NUL to an even length).
A record whose length is below 4 ends the stream (the zero padding after ENDLIB that
fills the last 2048-byte block, or a damaged file). The record table is the GDSII
Stream Format Manual, release 6.0; Magic's names are those of calma/calmaInt.h.
"""
from __future__ import annotations

import math
import struct
from dataclasses import dataclass

NONE, BITARRAY, INT2, INT4, REAL4, REAL8, ASCII = range(7)

# Record type -> (name, data type).
RECORD_TYPES: dict[int, tuple[str, int]] = {
    0x00: ("HEADER", INT2), 0x01: ("BGNLIB", INT2), 0x02: ("LIBNAME", ASCII), 0x03: ("UNITS", REAL8),
    0x04: ("ENDLIB", NONE), 0x05: ("BGNSTR", INT2), 0x06: ("STRNAME", ASCII), 0x07: ("ENDSTR", NONE),
    0x08: ("BOUNDARY", NONE), 0x09: ("PATH", NONE), 0x0A: ("SREF", NONE), 0x0B: ("AREF", NONE),
    0x0C: ("TEXT", NONE), 0x0D: ("LAYER", INT2), 0x0E: ("DATATYPE", INT2), 0x0F: ("WIDTH", INT4),
    0x10: ("XY", INT4), 0x11: ("ENDEL", NONE), 0x12: ("SNAME", ASCII), 0x13: ("COLROW", INT2),
    0x14: ("TEXTNODE", NONE), 0x15: ("NODE", NONE), 0x16: ("TEXTTYPE", INT2), 0x17: ("PRESENTATION", BITARRAY),
    0x18: ("SPACING", NONE), 0x19: ("STRING", ASCII), 0x1A: ("STRANS", BITARRAY), 0x1B: ("MAG", REAL8),
    0x1C: ("ANGLE", REAL8), 0x1D: ("UINTEGER", NONE), 0x1E: ("USTRING", NONE), 0x1F: ("REFLIBS", ASCII),
    0x20: ("FONTS", ASCII), 0x21: ("PATHTYPE", INT2), 0x22: ("GENERATIONS", INT2), 0x23: ("ATTRTABLE", ASCII),
    0x24: ("STYPTABLE", ASCII), 0x25: ("STRTYPE", INT2), 0x26: ("ELFLAGS", BITARRAY), 0x27: ("ELKEY", INT4),
    0x28: ("LINKTYPE", NONE), 0x29: ("LINKKEYS", NONE), 0x2A: ("NODETYPE", INT2), 0x2B: ("PROPATTR", INT2),
    0x2C: ("PROPVALUE", ASCII), 0x2D: ("BOX", NONE), 0x2E: ("BOXTYPE", INT2), 0x2F: ("PLEX", INT4),
    0x30: ("BGNEXTN", INT4), 0x31: ("ENDEXTN", INT4), 0x32: ("TAPENUM", INT2), 0x33: ("TAPECODE", INT2),
    0x34: ("STRCLASS", BITARRAY), 0x35: ("RESERVED", INT4), 0x36: ("FORMAT", INT2), 0x37: ("MASK", ASCII),
    0x38: ("ENDMASKS", NONE), 0x39: ("LIBDIRSIZE", INT2), 0x3A: ("SRFNAME", ASCII), 0x3B: ("LIBSECUR", INT2),
}
BY_NAME: dict[str, int] = {name: rtype for rtype, (name, _) in RECORD_TYPES.items()}

_FIXED = {BITARRAY: ">H", INT2: ">h", INT4: ">i"}
_SIZES = {BITARRAY: 2, INT2: 2, INT4: 4, REAL4: 4, REAL8: 8}


class GdsError(ValueError):
    """A value cannot be encoded, or a stream cannot be decoded."""


def encode_real8(value: float) -> bytes:
    """``value`` as a GDSII eight-byte real: sign bit, excess-64 base-16 exponent, 56-bit
    mantissa ``m`` with ``1/16 <= m < 1`` (zero is eight zero bytes). The mantissa is
    truncated, as Magic's calmaOutR8 does (calma/CalmaWrite.c:3260-3315), so the double nearest 1e-3 is
    ``3E4189374BC6A7F0``; values outside the format's range raise GdsError."""
    if value == 0:
        return bytes(8)
    if not math.isfinite(value):
        raise GdsError(f"cannot encode {value!r} as an eight-byte real")
    sign = 0x80 if value < 0 else 0
    mantissa, exponent = abs(value), 0
    while mantissa >= 1:
        mantissa /= 16
        exponent += 1
    while mantissa < 1 / 16:
        mantissa *= 16
        exponent -= 1
    bits = int(mantissa * (1 << 56))  # exact: scaling by a power of two, then truncation
    if not -64 <= exponent <= 63:
        raise GdsError(f"{value!r} is out of the eight-byte real range")
    return bytes([sign | (exponent + 64)]) + bits.to_bytes(7, "big")


def decode_real8(data: bytes) -> float:
    if len(data) != 8:
        raise GdsError("an eight-byte real needs 8 bytes")
    sign = -1.0 if data[0] & 0x80 else 1.0
    exponent = (data[0] & 0x7F) - 64
    return sign * int.from_bytes(data[1:], "big") / (1 << 56) * 16.0 ** exponent


def decode_real4(data: bytes) -> float:
    sign = -1.0 if data[0] & 0x80 else 1.0
    return sign * int.from_bytes(data[1:], "big") / (1 << 24) * 16.0 ** ((data[0] & 0x7F) - 64)


def encode_ascii(text: str | bytes) -> bytes:
    """A string payload: the bytes, then one NUL if the length is odd."""
    data = text.encode("latin-1") if isinstance(text, str) else bytes(text)
    return data + b"\0" if len(data) % 2 else data


def encode_payload(dtype: int, values) -> bytes:
    if dtype == NONE:
        if values:
            raise GdsError("a record of data type 0 has no values")
        return b""
    if dtype == ASCII:
        return encode_ascii(values[0] if isinstance(values, (list, tuple)) else values)
    if dtype == REAL8:
        return b"".join(encode_real8(float(v)) for v in values)
    if dtype in _FIXED:
        try:
            return b"".join(struct.pack(_FIXED[dtype], int(v)) for v in values)
        except struct.error as error:
            raise GdsError(str(error)) from None
    raise GdsError(f"cannot encode data type {dtype}")


def encode_record(rtype: int, dtype: int, payload: bytes) -> bytes:
    length = 4 + len(payload)
    if length > 0xFFFF or length % 2:
        raise GdsError(f"record length {length} is not an even 16-bit value")
    return struct.pack(">HBB", length, rtype, dtype) + payload


@dataclass(frozen=True)
class Record:
    offset: int
    rtype: int
    dtype: int
    payload: bytes

    @property
    def name(self) -> str:
        known = RECORD_TYPES.get(self.rtype)
        return known[0] if known else f"0x{self.rtype:02X}"

    def values(self) -> tuple:
        """The payload decoded by the record's data type (strings lose their NUL padding)."""
        if self.dtype == NONE and not self.payload:
            return ()
        if self.dtype == ASCII:
            return (self.payload.rstrip(b"\0").decode("latin-1"),)
        size = _SIZES.get(self.dtype)
        if size is None or len(self.payload) % size:
            return (self.payload,)
        chunks = [self.payload[i:i + size] for i in range(0, len(self.payload), size)]
        if self.dtype == REAL8:
            return tuple(decode_real8(c) for c in chunks)
        if self.dtype == REAL4:
            return tuple(decode_real4(c) for c in chunks)
        return tuple(struct.unpack(_FIXED[self.dtype], c)[0] for c in chunks)


def records(data: bytes):
    """Yield every record of ``data`` up to the end padding; GdsError on a truncated record."""
    pos = 0
    while pos + 4 <= len(data):
        length, rtype, dtype = struct.unpack_from(">HBB", data, pos)
        if length < 4:
            return
        if pos + length > len(data):
            raise GdsError(f"record at offset {pos} runs past the end of the stream")
        yield Record(pos, rtype, dtype, bytes(data[pos + 4:pos + length]))
        pos += length


def _format_value(value) -> str:
    if isinstance(value, float):
        return repr(value)
    if isinstance(value, bytes):
        return value.hex()
    if isinstance(value, str):
        return '"' + value.encode("unicode_escape").decode("ascii").replace('"', '\\"') + '"'
    return str(value)


def format_record(record: Record) -> str:
    values = record.values()
    shown = " ".join(_format_value(v) for v in values)
    if record.dtype == BITARRAY:
        shown = " ".join(f"0x{v:04X}" for v in values if isinstance(v, int)) or shown
    return f"{record.name}{(' ' + shown) if shown else ''}"


def dump(data: bytes) -> str:
    """One line per record, element bodies indented: the ``gds-records`` view.

    A stream that cannot be decoded ends with an ``error`` line instead of raising."""
    lines, depth = [], 0
    try:
        for record in records(data):
            if record.name in ("ENDSTR", "ENDEL"):
                depth = max(depth - 1, 0)
            lines.append("  " * depth + format_record(record))
            if record.name in ("BGNSTR", "BOUNDARY", "PATH", "SREF", "AREF", "TEXT", "NODE", "BOX"):
                depth += 1
    except GdsError as error:
        lines.append(f"error {error}")
    return "\n".join(lines) + "\n"
