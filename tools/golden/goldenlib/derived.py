# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Derived outputs: one compared file reduced from several files of the work directory.

m3-drc design D12/D14: the standard-cell corpus cases save hundreds of cells, and only
their DRC results are of interest. An ``[[outputs]]`` entry whose ``source`` is

    derived:mag-drc-sections:<glob>

is built, by capture.py and replay.py alike, from the files of the work directory that
match ``<glob>`` (``pathlib`` glob, relative to the work directory), in sorted path order:
for each file that has at least one of the sections ``<< checkpaint >>``, ``<< error_p >>``,
``<< error_s >>`` and ``<< error_ps >>``, a line ``file <relative path>`` followed by
those sections (header line and ``rect`` lines) as they appear in the file. Files with
none of them (cells DRC found clean and checked completely) contribute nothing, so the
output of a clean library is empty.

    derived:digest:<glob>

is one line ``<sha256> <size> <relative path>`` per matching file, for outputs too large
to commit (``drc printrules`` of a PDK technology is 26 MB); a difference fails the case
without showing where, so use it only where a small case covers the same records.

    derived:files:<glob>

is every matching file in turn, each preceded by a line ``file <relative path>``, for
cases whose saved cells have names fixed only at run time (the generated-cell names of
the M4 Desktop Alpha cases, m4-scripting design D18). The output's masks (for example
``mag-timestamp``) apply to every file in it.

    derived:gds-digest:<glob>
    derived:gds-summary:<glob>

are for GDS outputs too large to commit (the library round trips MASK-RT-*,
m5-mask-gds-cif design D19). Each matching file is first decompressed when it is gzip
data and masked with ``gds-mod-dates`` (compare/masks.py). ``gds-digest`` is then one line
``<sha256> <size> <relative path>`` of those bytes, the pass/fail check; ``gds-summary`` is
a line ``file <relative path>`` followed by the decoded summary of goldenlib/gds.py
``summary`` (library name and creation date, one line per structure with its record count),
so a differing digest can be located.

    derived:lef-summary:<glob>

is for LEF outputs too large to commit (the library round trips LEFDEF-RT-*, m6-lef-def-oasis
task 15.5, compared through ``digest``): a line ``file <relative path>``, then one line
``<part> <name> <lines> <sha256/16>`` per part of the file: ``head`` (the text before the first
MACRO), each ``macro`` (from its ``MACRO`` line through its ``END <name>`` line) and ``text`` (the
lines between two macros or after the last), so a differing digest can be located to a macro.

    derived:gl-faces:<glob>

is for the 3D-view geometry cases (m10-3d-view design D13, task 12.4): a line ``file <relative
path>`` followed by the ``gl-faces 1`` form that tools/golden/gl_capture/faces.py reduces the
``gl-capture 1`` polygon log to (per style and z, the union of the horizontal faces and the
boundary parts of the side faces), or a line ``error <reason>`` when the log is malformed.

A source that matches no file yields no output (the case then fails as if a plain file
were missing).
"""
from __future__ import annotations

import gzip
import hashlib
import re
from pathlib import Path

from . import gds

PREFIX = "derived:"
_REDUCERS = ("mag-drc-sections", "digest", "files", "gds-digest", "gds-summary", "lef-summary", "gl-faces")
_GZIP_MAGIC = b"\x1f\x8b"
_DRC_SECTIONS = (b"checkpaint", b"error_p", b"error_s", b"error_ps")
_SECTION = re.compile(rb"^<< (\S+) >>\r?$")


def is_derived(source: str) -> bool:
    return source.startswith(PREFIX)


def parse(source: str) -> tuple[str, str]:
    """(reducer, glob) of a derived source; ValueError when malformed."""
    body = source[len(PREFIX):]
    reducer, sep, pattern = body.partition(":")
    if not sep or reducer not in _REDUCERS or not pattern:
        raise ValueError(f"bad derived source '{source}' (expected derived:<{'|'.join(_REDUCERS)}>:<glob>)")
    if Path(pattern).is_absolute() or ".." in Path(pattern).parts:
        raise ValueError(f"derived source glob must stay in the work directory: '{pattern}'")
    return reducer, pattern


def mag_drc_sections(data: bytes) -> bytes:
    """The DRC sections of one .mag file (each section header and its records)."""
    out: list[bytes] = []
    keep = False
    for line in data.split(b"\n"):
        header = _SECTION.match(line)
        if header:
            keep = header.group(1) in _DRC_SECTIONS
        if keep:
            out.append(line)
    return b"".join(line + b"\n" for line in out)


def _drc_sections_entry(name: bytes, data: bytes) -> bytes:
    sections = mag_drc_sections(data)
    return b"file " + name + b"\n" + sections if sections else b""


def _digest_entry(name: bytes, data: bytes) -> bytes:
    return hashlib.sha256(data).hexdigest().encode() + b" " + str(len(data)).encode() + b" " + name + b"\n"


def _file_entry(name: bytes, data: bytes) -> bytes:
    return b"file " + name + b"\n" + data


def canonical_gds(data: bytes) -> bytes:
    """A GDS output as the MASK-RT cases compare it: decompressed, then ``gds-mod-dates`` masked."""
    from compare.masks import mask_gds_mod_dates  # compare/ sits beside goldenlib/ in tools/golden

    if data.startswith(_GZIP_MAGIC):
        data = gzip.decompress(data)
    return mask_gds_mod_dates(data)


def _gds_digest_entry(name: bytes, data: bytes) -> bytes:
    return _digest_entry(name, canonical_gds(data))


def _gds_summary_entry(name: bytes, data: bytes) -> bytes:
    return b"file " + name + b"\n" + gds.summary(canonical_gds(data)).encode("latin-1")


_MACRO = re.compile(rb"^MACRO\s+(\S+)")


def lef_parts(data: bytes) -> list[tuple[bytes, bytes, list[bytes]]]:
    """(part, name, lines) of a LEF file: ``head``, each ``macro`` and ``text`` (see the module doc)."""
    parts: list[tuple[bytes, bytes, list[bytes]]] = [(b"head", b"-", [])]
    macro_end = None
    for line in data.split(b"\n"):
        match = _MACRO.match(line) if macro_end is None else None
        if match:
            parts.append((b"macro", match.group(1), []))
            macro_end = b"END " + match.group(1)
        elif macro_end is None and parts[-1][0] == b"macro":
            parts.append((b"text", b"-", []))
        parts[-1][2].append(line)
        if macro_end is not None and line.strip() == macro_end:
            macro_end = None
    return parts


def _lef_summary_entry(name: bytes, data: bytes) -> bytes:
    lines = [b"file " + name]
    for part, part_name, text in lef_parts(data):
        digest = hashlib.sha256(b"\n".join(text)).hexdigest()[:16].encode()
        lines.append(b" ".join((part, part_name, str(len(text)).encode(), digest)))
    return b"\n".join(lines) + b"\n"


def _gl_faces_entry(name: bytes, data: bytes) -> bytes:
    from gl_capture import faces  # gl_capture/ sits beside goldenlib/ in tools/golden

    try:
        body = faces.reduce_log(data.decode("utf-8"))
    except (faces.LogError, UnicodeDecodeError) as error:
        body = f"error {error}\n"
    return b"file " + name + b"\n" + body.encode("utf-8")


_ENTRY = {"mag-drc-sections": _drc_sections_entry, "digest": _digest_entry, "files": _file_entry,
          "gds-digest": _gds_digest_entry, "gds-summary": _gds_summary_entry, "lef-summary": _lef_summary_entry,
          "gl-faces": _gl_faces_entry}


def build(work: Path, source: str) -> bytes | None:
    """The derived output of ``source`` over ``work``; None when no file matches."""
    reducer, pattern = parse(source)
    files = sorted(p for p in work.glob(pattern) if p.is_file())
    if not files:
        return None
    entry = _ENTRY[reducer]
    return b"".join(entry(p.relative_to(work).as_posix().encode(), p.read_bytes()) for p in files)


def read_output(work: Path, source: str) -> bytes | None:
    """A file output (plain path or derived source) of a run in ``work``; None if absent."""
    if is_derived(source):
        return build(work, source)
    path = work / source
    return path.read_bytes() if path.is_file() else None
