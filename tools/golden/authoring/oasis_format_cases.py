# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""OASIS-001..015 (capability oasis-format): hand-assembled files read against their GDS equivalent.

Every file is built record by record with ``oasgen.py``, damaged on purpose where the row asks
for it, and read by both tools (``oasis_common.read_case``). A file whose decoding stops creates
only the cells completed before the error (oasis-read "Two-pass decoding" rule 2), which its GDS
equivalent holds; a reported but non-fatal problem leaves the decoded content unchanged. Files of
one case use distinct cell names, so the reads of one session do not collide. OASIS-012 (gdstk
files) is in ``oasis_captured_cases.py``.
"""
from __future__ import annotations

import struct
import zlib

import gdsgen as g
import oasgen as o
from oasis_common import M1, M2, Read, ocell, ofile, read_case, rect

# --- OASIS-001 ------------------------------------------------------------------------------------


def raw_rect(lay: tuple[int, int], w: int, h: int, x: bytes, y: bytes) -> bytes:
    """A RECTANGLE whose x and y fields are raw bytes (non-minimal or negative-zero forms)."""
    return o.uint(o.RECTANGLE) + bytes([0x7B]) + o.uint(lay[0]) + o.uint(lay[1]) + o.uint(w) + o.uint(h) + x + y


def raw_placement18(name: str, real: bytes, x: int, y: int = 0) -> bytes:
    """PLACEMENT record 18 with an inline cell name, the magnification ``real`` (any real form) and
    a position: info bits C, X, Y and M."""
    return o.uint(o.PLACEMENT_MAG) + bytes([0xB4]) + o.nstring(name) + real + o.sint(x) + o.sint(y)


def integer_files() -> dict[str, bytes]:
    """Unit 10 (one database unit is one lambda), so the integers are coordinates as they are."""
    ints = ocell("ints", rect(M1, 0, 0, 127, 1), rect(M1, 0, 2, 128, 3),
                 raw_rect(M1, 5, 1, b"\x80\x80\x00", o.sint(4)),       # x: a non-minimal 0
                 raw_rect(M1, 5, 1, o.sint(10), b"\x01"),             # y: a negative 0
                 o.rectangle(49, 0, 1 << 63, 1, 0, 6),                # coordinate overflow: dropped
                 rect(M1, 0, 8, 300, 9), rect(M1, 0, 10, 16383, 11), rect(M1, 0, 12, 16384, 13))
    over = ocell("over", rect(M1, 0, 0, 10, 10),
                 o.uint(o.RECTANGLE) + bytes([0x7B]) + o.uint(49) + o.uint(0) + b"\x80" * 9 + b"\x02"
                 + o.uint(1) + o.sint(0) + o.sint(0))                 # a 65-bit width: decoding stops
    return {"ints": ofile(ints, unit=10), "over": ofile(over, unit=10)}


REAL_FORMS = (o.real_form(0, 2), o.real_form(1, 2), o.real_form(2, 2), o.real_form(3, 2), o.real_form(4, 3, 2),
              o.real_form(5, 3, 2), o.real_form(6, 2.0), o.real_form(7, 0.25))


def real_files() -> dict[str, bytes]:
    child = ocell("c", rect(M1, 0, 0, 2000, 2000))
    reals = ocell("reals", *(raw_placement18("c", form, 5000 * k) for k, form in enumerate(REAL_FORMS)))
    rdiv = ocell("rdiv", rect(M1, 0, 0, 100, 100), raw_placement18("c", o.real_form(4, 1, 0), 0))
    return {"reals": ofile(child, reals), "rdiv": ofile(rdiv)}


def raw_text(string: bytes, x: int, y: int) -> bytes:
    """TEXT on 49/0 with an inline a-string written unchecked (info bits C, X, Y, T, L)."""
    return (o.uint(o.TEXT) + bytes([0x5B]) + o.astring(string, check=False) + o.uint(49) + o.uint(0)
            + o.sint(x) + o.sint(y))


def string_files() -> dict[str, bytes]:
    bad_name = o.uint(o.PROPERTY) + bytes([0x14]) + o.nstring(b"bad name", check=False) + o.u(1)
    texts = ocell("strs", rect(M1, 0, 0, 1000, 1000), o.text("a b", 49, 0, 100, 100),
                  raw_text(b"tab\there", 300, 300),                    # illegal a-string byte: kept, warned
                  o.property_("note", [o.str_value(b"\x00\xff", "b")]),
                  bad_name)                                            # illegal n-string: kept, warned
    return {"strs": ofile(texts)}


def oasis_001():
    return read_case("OASIS-001", "oasis-format",
                     ["Unsigned and signed integers", "Real numbers", "Strings and character sets"],
                     "Integers 0, 127, 128, 300, 16383, 16384, a non-minimal 0, a negative 0, 2^63 (dropped) and a "
                     "65-bit overflow; reals of types 0-7 as magnifications and a zero divisor; strings with "
                     "illegal bytes.", Read({**integer_files(), **real_files(), **string_files()}))


# --- OASIS-002, 003, 004 --------------------------------------------------------------------------

PATH_LISTS = ((0, [1000, 500]), (1, [500, 1000]), (2, [(1000, 0), (0, 500)]), (3, [(500, 500), (500, 0)]),
              (4, [(1000, 0), (0, 700)]), (5, [(500, 0), (-500, 500)]))


def oasis_002():
    pl = ocell("pl",
               o.polygon(49, 0, o.point_list(0, [400, 300]), 0, 0),
               o.polygon(49, 0, o.point_list(1, [2000, 1000, -1000, 2000]), 1000, 0),
               o.polygon(49, 0, o.point_list(2, [(400, 0), (0, 400), (-400, 0)]), 4000, 0),
               o.polygon(49, 0, o.point_list(3, [(1000, 0), (0, 1000)]), 5000, 0),
               o.polygon(49, 0, o.point_list(4, [(1000, 0), (-300, 700)]), 7000, 0),
               o.polygon(49, 0, o.point_list(5, [(1000, 0), (-1000, 1000), (-1000, -1000)]), 9000, 0),
               o.polygon(49, 0, o.point_list(0, [400, 300, -200]), 11000, 0),   # odd type-0 count
               o.polygon(49, 0, o.point_list(4, []), 13000, 0),                 # count 0: dropped
               o.rectangle(49, 0, 100, 100, 13000, 0),
               *(o.path(51, 0, 50, ("flush", "flush"), o.point_list(t, deltas), 2000 * t, 5000)
                 for t, deltas in PATH_LISTS))
    return read_case("OASIS-002", "oasis-format", ["Delta encodings", "Point lists"],
                     "POLYGON point lists of types 0-5, an odd type-0 count, a count of 0, and a PATH per type.",
                     Read({"pl": ofile(pl)}))


REPETITIONS = (
    o.repetition(1, 1, 0, 400, 400), o.repetition(2, 1, 400), o.repetition(3, 1, 400), o.repetition(4, 1, 300, 500),
    o.repetition(5, 1, 100, 3, 5), o.repetition(6, 1, 300, 500), o.repetition(7, 1, 100, 3, 5),
    o.repetition(8, 0, 0, (400, 400), (-400, 400)), o.repetition(9, 1, (400, -400)),
    o.repetition(10, 1, (300, 0), (0, 300)), o.repetition(11, 1, 100, (3, 1), (1, 3)), o.repetition(0),
)


def oasis_003():
    reps = ocell("reps", *(rect(M1, 3000 * (k % 4), 3000 * (k // 4), 3000 * (k % 4) + 200,
                                3000 * (k // 4) + 200, r) for k, r in enumerate(REPETITIONS)))
    rep0 = ocell("rep0", rect(M1, 0, 0, 100, 100, o.repetition(0)))            # undefined modal repetition
    grid0 = ocell("grid0", rect(M1, 0, 0, 100, 100, o.repetition(5, 0, 0, 3)))  # zero grid
    return read_case("OASIS-003", "oasis-format", ["Repetitions"],
                     "One RECTANGLE per repetition type 1-11 and a type 0 reuse; type 0 with an undefined modal "
                     "repetition; grid 0 in type 5.",
                     Read({"reps": ofile(reps), "rep0": ofile(rep0), "grid0": ofile(grid0)}))


def oasis_004():
    names = [o.layername("ALL", o.interval(), o.interval()),
             o.layername("LOW", o.uint(1) + o.uint(49), o.interval()),
             o.layername("HIGH", o.uint(2) + o.uint(49), o.interval(0, 0)),
             o.layername("ONE", o.interval(49, 49), o.interval(0, 0), text=True),
             o.layername("SPAN", o.interval(49, 51), o.uint(4) + o.uint(0) + o.uint(5)),
             o.layername("INV", o.uint(4) + o.uint(51) + o.uint(49), o.interval(0, 0))]   # inverted interval
    data = ofile(*names, ocell("ivl", rect(M1, 0, 0, 1000, 1000), rect(M2, 0, 0, 500, 500)))
    return read_case("OASIS-004", "oasis-format", ["Intervals", "Name records and reference numbers"],
                     "LAYERNAME records with interval types 0-4 and an inverted interval; the layer mapping is "
                     "unchanged (oasis-read).", Read({"ivl": data}))


# --- OASIS-005, 006: START and END ----------------------------------------------------------------

ZERO_TABLES = [(0, 0)] * 6


def raw_start_file(start: bytes, *cells: bytes) -> bytes:
    """Magic bytes, the given START bytes (offset-flag 1), ``cells`` and END with zero table offsets."""
    prefix = o.MAGIC + start + b"".join(cells)
    return prefix + o.end(prefix, ZERO_TABLES)


def start_files() -> dict[str, bytes]:
    v2 = o.Builder(version="2.0")
    v2.add(ocell("sv2", rect(M1, 0, 0, 100, 100)))
    u0 = o.Builder(unit=0)
    u0.add(ocell("su0", rect(M1, 0, 0, 100, 100)))
    off0 = o.Builder(tables=ZERO_TABLES)
    off0.add(ocell("soff0", rect(M1, 0, 0, 100, 100)))
    unit7 = o.uint(o.START) + o.astring("1.0") + o.real_form(7, 1000.0) + o.uint(1)
    return {"sv2": v2.finish(tables=ZERO_TABLES), "su0": u0.finish(tables=ZERO_TABLES),
            "su7": raw_start_file(unit7, ocell("su7", rect(M1, 0, 0, 100, 100))),
            "soff0": off0.finish(), "soff1": ofile(ocell("soff1", rect(M1, 0, 0, 100, 100)))}


def oasis_005():
    return read_case("OASIS-005", "oasis-format", ["START record and table offsets"],
                     "START with version 2.0 and unit 0 (refused), unit as a type-7 real, offset-flag 0 with the "
                     "table offsets in START, offset-flag 1.", Read(start_files()))


def end_record(prefix: bytes, delta: int = 0, scheme: int = o.CRC32, signature: int | None = None) -> bytes:
    """END after ``prefix`` with zero table offsets, ``delta`` padding bytes more than the 256-byte
    record needs, the validation ``scheme`` and a signature (computed unless given; none for 0)."""
    head = o.uint(o.END) + o.tables_bytes(ZERO_TABLES)
    sig_len = 0 if scheme == o.NONE else 4
    content = 256 - (len(head) + 1 + sig_len) - 2 + delta
    record = head + o.uint(content) + bytes(content) + o.uint(scheme)
    if scheme == o.NONE:
        return record
    computed = zlib.crc32(prefix + record) if scheme == o.CRC32 else sum(prefix + record)
    return record + struct.pack("<I", (computed if signature is None else signature) & 0xFFFFFFFF)


def end_files() -> dict[str, bytes]:
    files = {}
    for name, delta, scheme in (("e255", -1, o.CRC32), ("e257", 1, o.CRC32), ("val0", 0, o.NONE),
                                ("val1", 0, o.CRC32), ("val2", 0, o.CHECKSUM32), ("val3", 0, 3)):
        prefix = o.MAGIC + o.start() + ocell(name, rect(M1, 0, 0, 100, 100))
        files[name] = prefix + end_record(prefix, delta, scheme, 0 if scheme == 3 else None)
    prefix = o.MAGIC + o.start() + ocell("bad", rect(M1, 0, 0, 100, 100))
    files["bad"] = prefix + end_record(prefix, signature=0x12345678)          # wrong CRC32
    trail = ofile(ocell("trail", rect(M1, 0, 0, 100, 100)))
    files["trail"] = trail + b"\x00\x00\x00\x00"                             # data after END
    return files


def oasis_006():
    return read_case("OASIS-006", "oasis-format", ["END record and padding", "Validation signature"],
                     "END records of 255, 256 and 257 bytes; data after END; validation schemes 0, 1, 2 and 3; "
                     "a wrong CRC32. Every file's cell is still read.", Read(end_files()))


# --- OASIS-007, 008: names and modal variables ----------------------------------------------------

def name_files() -> dict[str, bytes]:
    return {
        "imp": ofile(o.cellname("imp_a"), o.cellname("imp_top"), ocell(0, rect(M1, 0, 0, 100, 100)),
                     ocell(1, o.placement(0, 0, 0))),
        "exp": ofile(o.cellname("exp_a", 7), o.cellname("exp_top", 3), ocell(7, rect(M1, 0, 0, 100, 100)),
                     ocell(3, o.placement(7, 500, 0))),
        "mixed": ofile(o.cellname("mix_a"), o.cellname("mix_b", 5), ocell(0, rect(M1, 0, 0, 100, 100))),
        "dupref": ofile(o.cellname("dr_a", 1), o.cellname("dr_b", 1), ocell(1, rect(M1, 0, 0, 100, 100))),
        "dupname": ofile(o.cellname("dup", 0), o.cellname("dup", 1), ocell(0, rect(M1, 0, 0, 100, 100)),
                         ocell(1, rect(M2, 0, 0, 200, 200))),
        "fwd": ofile(ocell(1, o.placement(0, 0, 0), rect(M2, 0, 0, 100, 100)), ocell(0, rect(M1, 0, 0, 100, 100)),
                     o.cellname("fwd_a"), o.cellname("fwd_top")),
        "undef": ofile(o.cellname("undef_top"), ocell(0, o.placement(9, 0, 0), o.placement(5, 500, 0)),
                       ocell(5, rect(M1, 0, 0, 100, 100))),
    }


def dupname_gds() -> bytes:
    """The GDS equivalent of the cell defined twice: two structures named ``dup`` (gds-read's
    duplicate rule then decides; goldenlib/oasis.py keeps only the first cell of a name)."""
    return g.library("lib", [g.structure("dup", [g.rect(49, 0, 0, 0, 100, 100)]),
                             g.structure("dup", [g.rect(51, 0, 0, 0, 200, 200)])])


def oasis_007():
    return read_case("OASIS-007", "oasis-format", ["Name records and reference numbers"],
                     "CELLNAME tables: implicit only, explicit only, mixed (error), a duplicate refnum (error), a "
                     "name defined twice and used by two CELLs, forward references, undefined refnums (#<n>).",
                     Read(name_files(), gds={"dupname": dupname_gds()}))


def modal_files() -> dict[str, bytes]:
    modal = ocell("modal",
                  rect(M1, 0, 0, 200, 100), o.rectangle(x=500), o.rectangle(y=300),
                  o.polygon(points=o.point_list(3, [(300, 0), (0, 300)]), x=1000, y=0), o.polygon(x=2000),
                  o.path(51, 0, 50, ("half", "half"), o.point_list(4, [(500, 0)]), 0, 1000), o.path(y=1500),
                  o.text("A", 49, 0, 100, 50), o.text(x=600), o.text(y=350),
                  o.trapezoid(100, -100, layer=49, datatype=0, width=400, height=200, x=3000, y=0), o.trapezoid(100, 0, y=500),
                  o.placement("modal_c", 5000, 0), o.placement(x=6000), o.placement(y=1000))
    child = ocell("modal_c", rect(M1, 0, 0, 300, 300))
    xyrel = ocell("xyrel", o.xyrelative(), rect(M1, 100, 0, 200, 100), o.rectangle(x=200), o.rectangle(x=200, y=200),
                  o.text("R", 49, 0, 50, 50), o.text(x=100), o.xyabsolute(), o.rectangle(x=0, y=1000))
    reset = ofile(ocell("ra", rect(M1, 0, 0, 100, 100)), o.textstring("between"),
                  ocell("rb", rect(M2, 0, 0, 100, 100)))
    undefined = ofile(ocell("mu", o.rectangle(width=100, height=100, x=0, y=0)))   # layer never set: stops
    return {"modal": ofile(child, modal), "xyrel": ofile(xyrel), "reset": reset, "mundef": undefined}


def oasis_008():
    return read_case("OASIS-008", "oasis-format", ["Modal variables and reset rules"],
                     "Every element type omitting each optional field after it was set, XYRELATIVE runs, a name "
                     "record between two cells, and an undefined modal layer (error).", Read(modal_files()))


# --- OASIS-009, 010, 011 --------------------------------------------------------------------------

def oasis_009():
    child = ocell("c", rect(M1, 0, 0, 1000, 500), rect(M2, 0, 0, 200, 1000))
    p17 = ocell("p17", *(o.placement("c", 3000 * k, 0, rotation=90 * (k % 4), flip=k >= 4) for k in range(8)))
    p18 = ocell("p18", o.placement("c", 0, 0, mag=0.5), o.placement("c", 5000, 0, mag=2.0),
                raw_placement18("c", o.real_form(1, 1), 10000),
                *(o.placement("c", 15000 + 5000 * k, 0, angle=a) for k, a in enumerate((45.0, 90.0, 450.0))))
    return read_case("OASIS-009", "oasis-format", ["PLACEMENT records"],
                     "Record 17 with AA 0-3 x flip; record 18 with magnification 0.5, 2 and -1 and angles 45, 90 "
                     "and 450.", Read({"place": ofile(child, p17, p18)}))


ALL_VALUES = [o.real_form(t, *f) for t, f in ((0, (1,)), (1, (1,)), (2, (2,)), (3, (2,)), (4, (1, 2)),
                                              (5, (1, 2)), (6, (1.5,)), (7, (1.5,)))]
ALL_VALUES += [o.u(7), o.s(-7), o.str_value("a"), o.str_value(b"\x00b", "b"), o.str_value("n", "n"),
               o.ref_value(0, "a"), o.ref_value(0, "b"), o.ref_value(0, "n")]


def property_file() -> bytes:
    gds_prop = o.property_("S_GDS_PROPERTY", [o.u(61), o.str_value("inst", "b")], standard=True)
    cell = ocell("props", o.property_("on_cell", [o.u(1)]),
                 o.placement("pc", 0, 0), gds_prop,
                 o.placement("pc", 1000, 0), o.property_repeat(),
                 rect(M1, 0, 2000, 100, 2100), o.property_("zero", []), o.property_("fourteen", [o.u(k) for k in range(14)]),
                 o.property_("fifteen", [o.u(k) for k in range(15)]), o.property_(None, None),
                 o.property_("all", ALL_VALUES),
                 o.pad(), o.property_("after_pad", [o.u(2)]),
                 o.cblock(rect(M1, 500, 2000, 600, 2100) + o.property_("in_cblock", [o.u(3)])),
                 o.xyrelative(), o.property_("after_xy", [o.u(4)]), o.xyabsolute())
    names = [o.propstring("ps"), o.property_("on_name", [o.u(5)])]
    return ofile(o.property_("on_file", [o.u(6)]), *names, ocell("pc", rect(M1, 0, 0, 300, 300)), cell)


def oasis_010():
    return read_case("OASIS-010", "oasis-format", ["PROPERTY records", "Standard properties", "PAD record"],
                     "PROPERTY records with 0, 14 and 15+ values, V reuse, record 29, every value type, attached "
                     "after PAD, inside a CBLOCK, to a CELL, a name record, the file and after XYRELATIVE; one "
                     "S_GDS_PROPERTY repeated onto a second placement.", Read({"props": property_file()}))


def raw_cblock(content: bytes, ctype: int = 0, size_delta: int = 0) -> bytes:
    compressor = zlib.compressobj(9, zlib.DEFLATED, -15)
    packed = compressor.compress(content) + compressor.flush()
    return o.uint(o.CBLOCK) + o.uint(ctype) + o.uint(len(content) + size_delta) + o.uint(len(packed)) + packed


def cblock_files() -> dict[str, bytes]:
    record = rect(M1, 0, 0, 100, 100)
    return {
        "cbody": ofile(o.cell("cbody") + o.cblock(record + rect(M2, 0, 0, 200, 200))),
        "ctable": ofile(o.cblock(o.cellname("ctable")), ocell(0, record)),
        "csize": ofile(ocell("cs_ok", record), o.cell("cs_bad") + raw_cblock(record, size_delta=1)),
        "ctype": ofile(ocell("ct_ok", record), o.cell("ct_bad") + raw_cblock(record, ctype=1)),
        "cnest": ofile(ocell("cn_ok", record), o.cell("cn_bad") + o.cblock(o.cblock(record))),
        "csplit": ofile(o.cell("csplit") + o.cblock(record[:4]) + record[4:] + rect(M2, 0, 0, 200, 200)),
    }


def oasis_011():
    return read_case("OASIS-011", "oasis-format", ["CBLOCK compression"],
                     "CBLOCKs: a compressed cell body and name table, a size mismatch, compression type 1 and a "
                     "nested CBLOCK (errors after a complete cell), and a record split across a CBLOCK's end.",
                     Read(cblock_files()))


# --- OASIS-013, 014, 015 --------------------------------------------------------------------------

def oasis_013():
    return read_case("OASIS-013", "oasis-format", ["Complete annotated example"],
                     "The 419-byte reference file of oasis-format: the decoded cell top. KLayout reads it without "
                     "a warning and LayoutDiff finds no difference from its GDS equivalent (oasis-validate).",
                     Read({"ref419": o.reference_419()}),
                     notes=["The KLayout and gdstk checks of the file run in golden-capture.yml mode oasis-validate "
                            "(design D20); their run ID and versions go in tasks.md and here."])


def strict_file(name: str, body: list[bytes], tables: dict[int, list[bytes]]) -> bytes:
    """A strict file: the cell records, then each table's records (index in the END table list) with
    strict-flag 1 at its offset."""
    builder = o.Builder()
    builder.add(*body)
    entries = [(1, 0)] * 6
    for index, records in tables.items():
        entries[index] = (1, builder.add(*records))
    return builder.finish(tables=entries)


def strict_files() -> dict[str, bytes]:
    record = rect(M1, 0, 0, 100, 100)
    return {
        "st_text": strict_file("st_text", [ocell(0, record, o.text("A", 49, 0, 50, 50))],
                               {0: [o.cellname("st_text")]}),
        "st_cell": strict_file("st_cell", [ocell("st_cell", record)], {0: [o.cellname("st_other")]}),
        "st_pname": strict_file("st_pname", [o.propname("early"), ocell(0, record)],
                                {0: [o.cellname("st_pname")], 2: [o.propname("late")]}),
        "st_pstr": strict_file("st_pstr", [ocell(0, record, o.property_(0, [o.str_value("inline")]))],
                               {0: [o.cellname("st_pstr")], 2: [o.propname("p")]}),
    }


def oasis_014():
    return read_case("OASIS-014", "oasis-format", ["Strict mode"],
                     "Strict-mode violations in files declaring strict tables: an inline TEXT string, a CELL by "
                     "name, a PROPNAME outside its table, an inline property string. Warnings only (Q-OASIS-7).",
                     Read(strict_files()))


def oasis_015():
    xgeometry = o.xgeometry(7, b"\x01\x02", layer=49, datatype=0, x=1000, y=0, rep=o.repetition(2, 0, 500))
    data = ofile(o.xname(1, b"vendor"), o.xname(2, b"other"),
                 ocell("ext", rect(M2, 0, 0, 100, 100), o.xelement(3, b"payload"), xgeometry,
                       o.rectangle(width=200, height=200, x=0, y=1000, rep=o.repetition(0)),
                       o.rectangle(width=100, height=100, y=2000)))
    explicit = ofile(o.xname(1, b"vendor", 4), ocell("ext2", o.xelement(3, b"x"), rect(M1, 0, 0, 100, 100)))
    return read_case("OASIS-015", "oasis-format", ["Extension records"],
                     "XNAME in its implicit and explicit forms, XELEMENT, and an XGEOMETRY with a repetition "
                     "followed by RECTANGLEs relying on the layer, repetition and x it left modal.",
                     Read({"ext": data, "ext2": explicit}))


# --- OASIS-030: trapezoids (task 15.6) -------------------------------------------------------------

CTRAP_WIDTH_ONLY = {16, 17, 18, 19, 22, 23, 25}
CTRAP_HEIGHT_ONLY = {20, 21}
TRAPEZOIDS = [(1000, 500, False), (-500, 1000, False), (1000, 0, False), (0, -1000, False),
              (500, 500, True), (-500, -500, True), (1000, 0, True), (0, 1000, True)]


def ctrapezoid(ctype: int, x: int, y: int) -> bytes:
    width = None if ctype in CTRAP_HEIGHT_ONLY else 4000
    height = None if ctype in CTRAP_WIDTH_ONLY else 2000
    return o.ctrapezoid(ctype, 49, 0, width, height, x, y)


def oasis_030():
    ctraps = [ctrapezoid(t, 6000 * (t % 6), 6000 * (t // 6)) for t in range(26)]
    traps = [o.trapezoid(a, b, 51, 0, 4000, 4000, 6000 * k, 30000, vertical=v) for k, (a, b, v) in enumerate(TRAPEZOIDS)]
    data = ofile(ocell("traps", *ctraps, *traps))
    return read_case("OASIS-030", "oasis-format", ["CTRAPEZOID record", "TRAPEZOID records"],
                     "One CTRAPEZOID of each type 0-25 (w = 40, h = 20 where used) and TRAPEZOIDs 23, 24 and 25, "
                     "horizontal and vertical, with positive and negative deltas (Q-OASIS-4).",
                     Read({"trap": data}),
                     notes=["KLayout's strm2gds of trap.oas is XORed with trap.gds (the equivalent Magic reads) and "
                            "gdstk reads trap.oas, in oasis-validate.",
                            "Approved deviation (Q-OASIS-2, amended 2026-11-18; design D19): no commercial OASIS "
                            "writer's file is available, so the vertex tables are checked against the spec's "
                            "(oasgen) file and the KLayout and gdstk readers only. Not a deviations.toml entry: "
                            "the case passes."])


def specs() -> list:
    return [oasis_001(), oasis_002(), oasis_003(), oasis_004(), oasis_005(), oasis_006(), oasis_007(),
            oasis_008(), oasis_009(), oasis_010(), oasis_011(), oasis_013(), oasis_014(), oasis_015(), oasis_030()]
