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

A source that matches no file yields no output (the case then fails as if a plain file
were missing).
"""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

PREFIX = "derived:"
_REDUCERS = ("mag-drc-sections", "digest")
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


_ENTRY = {"mag-drc-sections": _drc_sections_entry, "digest": _digest_entry}


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
