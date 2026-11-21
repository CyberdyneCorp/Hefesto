# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Outputs collected by name pattern (``[[collect]]`` in case.toml; m7-extraction design D21, task 2.3).

An extraction or netlisting run writes files whose names depend on the layout: one ``.ext`` per cell
of the hierarchy, ``<root>.spice``, ``<root>.sim`` with ``.al`` and ``.nodes``, ``<cell>.res.ext``,
``.res.lump`` and ``.fh``. Listing each of them as an ``[[outputs]]`` entry would tie the case to
the cell names, and a missing or extra file would go unnoticed. A ``[[collect]]`` entry names a
file-name pattern instead::

    [[collect]]
    pattern = "*.res.ext"      # fnmatch pattern on the file name (not the path)
    comparator = "res-ext"     # compare/ comparator of every file it collects
    masks = []                 # compare/masks.py masks of every file it collects

Rules (the same for capture.py and replay.py):

1. Before the run, the work directory (the staged ``input/`` and anything else in it) is snapshot:
   relative path -> SHA-256 of every regular file.
2. After the run, a regular file is a *candidate* when it is new or its bytes changed. Inputs the
   run leaves alone (a hand-written ``.ext`` of a NETLIST case) are never collected. Symbolic links
   are not followed (the replay's ``pdk/`` links, the image's mounts).
3. Each candidate goes to the first ``[[collect]]`` entry, in case.toml order, whose pattern matches
   its file name. ``*.ext`` also matches ``top.res.ext``, so list ``*.res.ext`` first; the M7
   authoring helper does. A candidate no entry matches is ignored (a crash backup, a log).
4. A collected file is the output ``collected/<relative path>``: Magic writes a ``.ext`` next to
   its ``.mag`` (extract/ExtCell.c:153-215), so ``input/top.ext`` and ``top.ext`` are distinct.
   capture.py writes it to ``expected/collected/<relative path>`` and records its digest in
   provenance.json. The set of collected names must match on replay: a file Hefesto does not write,
   or one Magic did not, fails the case.
"""
from __future__ import annotations

import fnmatch
import hashlib
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

PREFIX = "collected/"


@dataclass(frozen=True)
class Collect:
    pattern: str
    comparator: str
    masks: tuple[str, ...] = ()


def parse(raw: object, index: int, known_comparators=None) -> Collect:
    """One ``[[collect]]`` table; ValueError when malformed."""
    where = f"collect[{index}]"
    if not isinstance(raw, dict):
        raise ValueError(f"{where}: must be a table")
    pattern, comparator = raw.get("pattern"), raw.get("comparator")
    if not isinstance(pattern, str) or not pattern or "/" in pattern:
        raise ValueError(f"{where}: 'pattern' must be a file-name pattern without '/'")
    if not isinstance(comparator, str):
        raise ValueError(f"{where}: 'comparator' must be a string")
    if known_comparators is not None and comparator not in known_comparators:
        raise ValueError(f"{where}: unknown comparator '{comparator}'")
    masks = raw.get("masks", [])
    if not isinstance(masks, list) or not all(isinstance(m, str) for m in masks):
        raise ValueError(f"{where}: 'masks' must be a list of strings")
    return Collect(pattern, comparator, tuple(masks))


def _files(root: Path):
    """Regular files under ``root``, symbolic links neither followed nor listed."""
    stack = [root]
    while stack:
        for entry in sorted(stack.pop().iterdir()):
            if entry.is_symlink():
                continue
            if entry.is_dir():
                stack.append(entry)
            elif entry.is_file():
                yield entry


def snapshot(work: Path) -> dict[str, str]:
    """Relative POSIX path -> SHA-256 of every regular file under ``work``."""
    return {p.relative_to(work).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in _files(work)}


def match(entries: tuple[Collect, ...], name: str) -> Collect | None:
    """The first entry whose pattern matches the file name ``name``."""
    return next((c for c in entries if fnmatch.fnmatchcase(name, c.pattern)), None)


def gather(work: Path, before: dict[str, str], entries: tuple[Collect, ...]) -> dict[str, tuple[Collect, bytes]]:
    """``collected/<path>`` -> (entry, bytes) of every new or changed file a pattern matches."""
    out: dict[str, tuple[Collect, bytes]] = {}
    if not entries:
        return out
    for path in _files(work):
        rel = path.relative_to(work).as_posix()
        data = path.read_bytes()
        if before.get(rel) == hashlib.sha256(data).hexdigest():
            continue
        entry = match(entries, PurePosixPath(rel).name)
        if entry is not None:
            out[PREFIX + rel] = (entry, data)
    return dict(sorted(out.items()))


def expected_files(expected_dir: Path) -> dict[str, Path]:
    """``collected/<path>`` -> file of every collected output committed under ``expected_dir``."""
    root = expected_dir / PREFIX.rstrip("/")
    if not root.is_dir():
        return {}
    return {PREFIX + p.relative_to(root).as_posix(): p for p in _files(root)}


def entry_for(entries: tuple[Collect, ...], name: str) -> Collect | None:
    """The entry a committed ``collected/<path>`` belongs to (by its file name)."""
    return match(entries, PurePosixPath(name).name)


def set_difference(expected: set[str], actual: set[str]) -> str:
    """'' when the collected names agree, else which are missing and which are extra."""
    missing, extra = sorted(expected - actual), sorted(actual - expected)
    parts = ([f"missing {', '.join(missing)}"] if missing else []) + ([f"extra {', '.join(extra)}"] if extra else [])
    return "; ".join(parts)
