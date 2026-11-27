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
``magic-undo-note``
    The whole console line ``This action cannot be undone.`` that Magic prints at
    the start of ``lef read`` and ``def read`` (lef/lefRead.c:3018, lef/defRead.c:2550).
    Hefesto makes each read one undoable step and never prints it (Q-LEFDEF-22,
    platform; layout-undo "Undoable LEF and DEF reads"). Every line equal to it is
    removed.
``magic-crash-line``
    The whole ``Created database crash recovery file ...`` console line. Magic
    writes its crash backup from a timer, so during a long run (the MASK-RT
    library round trips) the line lands between different messages in each
    capture; the line is dropped wherever it appears.
``magic-tech-menu-order``
    Lines ``<i> <name> {magic::tech load <name> ;  magic::techmanager update}``
    (the technology manager's menu, tcltk/techbuilder.tcl): Magic lists the
    example technologies of its system directory in ``glob`` order, which is
    the directory order of the file system the reference image was built on,
    so each run of such lines is sorted by name and its indexes become
    ``<i>``.
``schem-paths``
    The absolute paths of a SCHEM netlist's ``** sch_path:`` and ``** sym_path:`` comment lines
    (openspec/changes/m8-schematic design D16, Q-SCHEM-13: they name files on the netlisting host). A path
    inside a PDK tree (``.../<pdk>/libs.tech/...`` or ``.../<pdk>/libs.ref/...`` with ``<pdk>`` one of
    ``sky130A``, ``gf180mcuD``, ``ihp-sg13g2``) becomes ``$PDK_ROOT/<pdk>/...``; a path under the work
    directory ``/work/`` becomes relative to it; a path of the system symbol library (``.../xschem_library/...``:
    xschem's ``/opt/ref/share/xschem``, Hefesto's ``<CAD_ROOT>/assets/schematic``, Q-SCHEM-45) becomes
    ``$XSCHEM_SHAREDIR/xschem_library/...``; the rest of the line is kept.
``schem-user-conf``
    The user configuration folder in a SCHEM netlist (a PDK symbol's ``pre_osdi $USER_CONF_DIR/...`` line):
    an absolute path ending in ``/.xschem`` (xschem's ``~/.xschem`` of the capture user, Hefesto's
    ``<document folder>/.xschem``) becomes ``$USER_CONF_DIR``.
``schem-hefesto-driver``
    hefesto-cli's own text in the streams of a SCHEM replay (the Hefesto side of Q-SCHEM-46): the start-up
    line ``info: APP-TECH-002: Using technology "<name>", version <v>`` and the note ``info: SCHEM-LIB-007:
    xschem_execute_scripts yes in <rc file> ignored: ...`` on the capture setting of the harness or PDK rc
    and the lines an ``xschemrc`` prints while it is read (``info: SCHEM-LIB-008: xschemrc: <line>``, the
    lines the capture drops on the xschem side) are dropped.
``schem-version``
    The ``version=<v>`` value of a ``v {xschem version=<v> ...}`` record (the first record of a ``.sch`` or
    ``.sym`` file, Q-SCHEM-1): the value becomes ``<masked>``. For a file written by another xschem version
    compared with a resave; ``file_version`` and the header text are compared.
``ext-times``
    The CPU-time values of Magic's extraction statistics (``*extract time``, extract/ExtTimes.c:
    the per-cell ``Tpaint, Tcell`` and ``Thier, Tincr`` lines and the three ``fets/sec rects/sec``
    lines, ExtTimes.c:542-551, and the ``fets/sec``, ``rects/sec`` and ``tot incr time`` rows of the
    summary, ExtTimes.c:247-253 and 934-943). The numbers (or ``<none>``) of those lines, with the
    ``%8.2f`` padding that varies with them, become ``<t>`` fields separated by one blank; the
    labels, the device and rectangle counts and the area percentages, which do not depend on time,
    are kept. ``magic-console`` applies it to every console stream
    (m7-extraction design D21); list it as well where an output is saved for the cross-platform
    check (replay.py --output-dir applies masks, not comparators).
``extcheck-memory``
    The text after ``Memory used: `` on the first line ``extcheck`` prints (extcheck/extcheck.c:100,
    RunStats of the host's allocator): it becomes ``<masked>``. capability netlist-extcheck
    "Counts and report format" calls the line informative.
``prog-name``
    The program name an external program's usage line starts with (m7-extraction design D15, the
    external-command mode): a leading ``extcheck`` or ``hefesto-extcheck``, with or without a
    directory, followed by ``:``, a blank or the end of the line, becomes ``<prog>``, so the two
    programs' names never differ. Magic's own usage text (extflat/EFargs.c:158-165) does not start
    with the name, so on it the mask changes nothing.
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


# Magic's crash backup comes from a timer: a long run prints the line at no fixed place.
_CRASH_LINE = re.compile(rb"^Created database crash recovery file \S*\r?\n", re.MULTILINE)


def mask_magic_crash_line(data: bytes) -> bytes:
    return _CRASH_LINE.sub(b"", data)


# Magic disables undo for LEF and DEF reads; Hefesto records them (Q-LEFDEF-22).
_UNDO_NOTE = re.compile(rb"^This action cannot be undone\.\r?\n", re.MULTILINE)


def mask_magic_undo_note(data: bytes) -> bytes:
    return _UNDO_NOTE.sub(b"", data)


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


# Q-SCHEM-13: sch_path/sym_path comment lines carry host paths.
_SCHEM_PATH_LINE = re.compile(rb"^(\*\* s(?:ch|ym)_path: )(\S[^\n]*?)(\r?)$", re.MULTILINE)
_SCHEM_PDK_PATH = re.compile(rb"^.*?/((?:sky130A|gf180mcuD|ihp-sg13g2)/libs\.(?:tech|ref)/.*)$")
_SCHEM_WORK = b"/work/"
# The system symbol library: xschem's /opt/ref/share/xschem/xschem_library/... in the capture image, Hefesto's
# <CAD_ROOT>/assets/schematic/xschem_library/... (Q-SCHEM-10, -45) in the replay.
_SCHEM_SYSTEM_PATH = re.compile(rb"^/.*?/(xschem_library/.*)$")


def _schem_relative(path: bytes) -> bytes:
    pdk = _SCHEM_PDK_PATH.match(path)
    if pdk:
        return b"$PDK_ROOT/" + pdk.group(1)
    if path.startswith(_SCHEM_WORK):
        return path[len(_SCHEM_WORK):]
    system = _SCHEM_SYSTEM_PATH.match(path)
    if system:
        return b"$XSCHEM_SHAREDIR/" + system.group(1)
    return path


def mask_schem_paths(data: bytes) -> bytes:
    return _SCHEM_PATH_LINE.sub(lambda m: m.group(1) + _schem_relative(m.group(2)) + m.group(3), data)


# Q-SCHEM-1: the writer's version string in the version record.
_SCHEM_VERSION = re.compile(rb"^(v \{xschem version=)[^\s}]+", re.MULTILINE)


# The user configuration folder in a netlist (a PDK symbol's `pre_osdi $USER_CONF_DIR/...` line): xschem's
# ~/.xschem of the capture user, Hefesto's <document folder>/.xschem.
_SCHEM_USER_CONF = re.compile(rb"(?<![^\s'\"=])/[^\s'\"]*?/\.xschem(?=/)")


def mask_schem_user_conf(data: bytes) -> bytes:
    return _SCHEM_USER_CONF.sub(b"$USER_CONF_DIR", data)


# The Hefesto side of Q-SCHEM-46: hefesto-cli's own text around a SCHEM replay. Its start-up line (Magic's
# "Using technology" banner), the note on the harness rc's `set xschem_execute_scripts yes` (a capture setting
# that Hefesto ignores: Q-SCHEM-11's sandbox always runs tcleval values) and the lines an xschemrc prints while
# it is read (SCHEM-LIB-008), which the capture drops on the xschem side (goldenlib/schem_driver.py).
_SCHEM_HEFESTO_DRIVER = re.compile(
    rb'^info: APP-TECH-002: Using technology "[^"\n]*", version [^\n]*\n'
    rb"|^info: SCHEM-LIB-007: xschem_execute_scripts yes in \S*xschemrc ignored: [^\n]*\n"
    rb"|^info: SCHEM-LIB-008: xschemrc: [^\n]*\n", re.MULTILINE)


def mask_schem_hefesto_driver(data: bytes) -> bytes:
    return _SCHEM_HEFESTO_DRIVER.sub(b"", data)


def mask_schem_version(data: bytes) -> bytes:
    return _SCHEM_VERSION.sub(rb"\1<masked>", data)


# extract/ExtTimes.c: the time-dependent statistics lines. A field is a number or "<none>".
_TIME_FIELD = rb"(?:-?\d+(?:\.\d+)?|nan|-?inf|<none>)"
_EXT_TIMES_CELL = re.compile(
    rb"^( *)" + _TIME_FIELD + rb"( +)" + _TIME_FIELD
    + rb"( (?:Tpaint, Tcell|Thier, Tincr|\((?:paint|hier|flat)\) *fets/sec rects/sec))(\r?)$", re.MULTILINE)
_EXT_TIMES_SUMMARY = re.compile(
    rb"^((?:fets/sec (?:paint|hier |flat )|rects/sec (?:paint|hier |flat )|tot incr time  ))"
    rb"((?: +" + _TIME_FIELD + rb"){4})(\r?)$", re.MULTILINE)


def _mask_cell_times(match: re.Match) -> bytes:
    return b"<t> <t>" + match.group(3) + match.group(4)  # the %8.2f padding varies with the value


def _mask_summary_times(match: re.Match) -> bytes:
    return match.group(1) + b" <t> <t> <t> <t>" + match.group(3)


def mask_ext_times(data: bytes) -> bytes:
    return _EXT_TIMES_SUMMARY.sub(_mask_summary_times, _EXT_TIMES_CELL.sub(_mask_cell_times, data))


# extcheck/extcheck.c:100: printf("Memory used: %s\n", RunStats(RS_MEM, ...)).
_EXTCHECK_MEMORY = re.compile(rb"^(Memory used: )[^\r\n]*", re.MULTILINE)


def mask_extcheck_memory(data: bytes) -> bytes:
    return _EXTCHECK_MEMORY.sub(rb"\1<masked>", data)


# Q-EXTRACT-36 (h): Magic's resistor branch writes an uninitialised width when no boundary estimate exists
# (extract/ExtBasic.c:2988-3060), so the width of such a `device devres` record differs between two Magic
# runs (sky130_fd_sc_hd__probec_p_8: -1123705434, then -1406616154). A width of nine or more digits is
# masked; Hefesto writes 0 there, which stays visible.
_EXT_DEVRES_WIDTH = re.compile(rb"^(device devres \S+(?: -?\d+){5}) -?\d{9,}(?= )", re.MULTILINE)


def mask_ext_devres_uninit_width(data: bytes) -> bytes:
    return _EXT_DEVRES_WIDTH.sub(rb"\1 <uninitialised>", data)


# The external-command roles' programs (goldenlib/external_command.py ROLES).
_PROG_NAME = re.compile(rb"^(?:\S*/)?(?:hefesto-)?extcheck(?=[:\s]|$)", re.MULTILINE)


def mask_prog_name(data: bytes) -> bytes:
    return _PROG_NAME.sub(b"<prog>", data)


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
    "magic-crash-line": mask_magic_crash_line,
    "magic-undo-note": mask_magic_undo_note,
    "magic-tech-menu-order": mask_magic_tech_menu_order,
    "schem-paths": mask_schem_paths,
    "schem-version": mask_schem_version,
    "schem-user-conf": mask_schem_user_conf,
    "schem-hefesto-driver": mask_schem_hefesto_driver,
    "ext-times": mask_ext_times,
    "extcheck-memory": mask_extcheck_memory,
    "prog-name": mask_prog_name,
    "ext-devres-uninit-width": mask_ext_devres_uninit_width,
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
