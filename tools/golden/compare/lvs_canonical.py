# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``lvs-canonical`` for the LVS reports ``comp.out`` and ``comp.json``.

Capability lvs-reports, Requirement "Canonical golden comparison"; decision Q-LVS-2 (fix: Hefesto
writes these sections in a deterministic order, netgen in its hash and partition order);
m9-lvs-simulation design D19. Written from the lvs-reports spec text only (its requirements "Two-column
layout", "Subcircuit summary section", "Net mismatch report", "Device mismatch report", "Pin table",
"Property error report" and "JSON file"); the section rules below may be tightened when the LVS captures
fix the texts the spec leaves to capture (task 3.8).

The file is JSON (``comp.json``) when its first non-blank byte is ``[``, else a report (``comp.out``).

**comp.out.** Every line is kept byte for byte and in place, except inside these sections, whose
contents are compared as sets (multisets, so a duplicated line still counts):

1. *Subcircuit summary class rows.* After ``Subcircuit summary:``, its header row and its column
   divider: the class rows up to the ``Number of devices:`` row are sorted. The ``Number of devices:``
   and ``Number of nets:`` rows stay where they are, byte for byte, so a changed count still differs.
2. *Mismatch fragments.* After ``NET mismatches: ...`` or ``DEVICE mismatches: ...`` and its header row,
   each illegal class (an empty line, fragments, a full divider) is reduced to two multisets, one per
   circuit (left and right column), of fragments; a fragment is its ``Net:``/``Instance:`` text and its
   sorted entry texts. Absent sides (``(no matching net)``, ``(no matching instance)``) and empty cells
   are dropped, so the pairing of fragments across the columns does not matter. The classes are then
   sorted. A changed fanout count, name or entry still differs.
3. *Pin table rows.* After ``Subcircuit pins:``, its header row and its column divider: the rows up
   to the full divider are sorted (each row keeps its pairing).
4. *Property blocks.* A run of blocks each starting with a line ``<i1> vs. <i2>:``: each block's other
   lines are sorted, then the blocks. The run ends at an empty line, at a line that starts a result or a
   new section (``Netlists ``, ``Circuits ``, ``Cell ``, ``Subcircuit ``, ``Final result``, ``Result``,
   ``NET mismatches``, ``DEVICE mismatches``) or at the end of the file.
5. *Merge and flatten message runs.* A maximal run of consecutive lines that start with ``Flatten``,
   ``Merg`` or ``Remov`` is sorted.

The column position L of a section is taken from its header row (the ``|`` after ``Circuit 1: ...``), so
any ``format`` widths work.

**comp.json.** The file is parsed and rewritten as one canonical JSON value. The pairs keep their
(queue) order and each pair keeps its key order; ``name`` and ``nets`` compare exactly. The lists whose
order comes from netgen's internal order compare as multisets: each circuit's ``devices`` entries, the
fragments of ``badnets`` and ``badelements`` with each side's nets or instances and each one's
entries, the ``properties`` pairs with their key/value lists, and the ``pins`` pairings (the two pin
lists are zipped into pairs, then the pairs are sorted). A file that is not JSON compares as
``<not json>`` followed by its bytes.

Example (Q-LVS-2): netgen writes ``pfet (2)`` before ``nfet (2)``, Hefesto ``nfet (2)`` first; both
canonicalize to the same summary, while ``nfet (3) | nfet (2) **Mismatch**`` against ``nfet (2) |
nfet (2)`` still differs.
"""
from __future__ import annotations

import itertools
import json
import re

NAME = "lvs-canonical"

_HEADER = re.compile(r"^Circuit 1: .*\|Circuit 2: ")
_PROPERTY_BLOCK = re.compile(r"^\S.* vs\. \S.*:$")
_PROPERTY_END = ("Netlists ", "Circuits ", "Cell ", "Subcircuit ", "Final result", "Result", "NET mismatches",
                 "DEVICE mismatches")
_MESSAGE = re.compile(r"^(Flatten|Merg|Remov)")
_ABSENT = {"(no matching net)", "(no matching instance)", "(no matching element)", ""}


def _is_divider(line: str) -> bool:
    return bool(line) and set(line) <= {"-", "|"}


def _column(lines: list[str], index: int) -> int | None:
    """The ``|`` column of the header row at ``index``, or None when that line is no header row."""
    if index >= len(lines) or not _HEADER.match(lines[index]):
        return None
    return lines[index].index("|", len("Circuit 1: "))


def _split_row(line: str, column: int) -> tuple[str, str] | None:
    if len(line) <= column or line[column] != "|":
        return None
    return line[:column].rstrip(), line[column + 1:].rstrip()


def _sorted_rows(lines: list[str], start: int, stop_prefix: str | None) -> int:
    """Sorts ``lines[start:end]`` in place, where ``end`` is the first full divider (no ``|``) or the first
    line starting with ``stop_prefix``; returns ``end``."""
    end = start
    while end < len(lines) and not (_is_full_divider(lines[end]) or
                                    (stop_prefix is not None and lines[end].startswith(stop_prefix))):
        end += 1
    lines[start:end] = sorted(lines[start:end])
    return end


def _table_section(lines: list[str], index: int, stop_prefix: str | None) -> int:
    """``lines[index]`` is ``Subcircuit summary:`` or ``Subcircuit pins:``: sorts its rows."""
    if _column(lines, index + 1) is None or index + 2 >= len(lines) or not _is_divider(lines[index + 2]):
        return index + 1
    return _sorted_rows(lines, index + 3, stop_prefix)


def _fragment_key(names: list[str], entries: list[str]) -> str:
    return "\x1f".join(names + sorted(entries))


def _is_full_divider(line: str) -> bool:
    return bool(line) and set(line) == {"-"}


def _read_class(lines: list[str], index: int, column: int) -> tuple[str, int]:
    """Reads one illegal class (its empty line, a full divider, fragment rows, a closing full divider)
    starting at ``index``; returns its canonical text and the index after it."""
    sides: tuple[list[list[str]], list[list[str]]] = ([], [])
    open_fragment: list[list[str] | None] = [None, None]
    end = index + 2
    while end < len(lines) and not _is_full_divider(lines[end]):
        row = _split_row(lines[end], column)
        end += 1
        if row is None or not (row[0].strip() or row[1].strip()):
            open_fragment = [None, None]  # a blank separator row ends the fragments
            continue
        for side in (0, 1):
            open_fragment[side] = _add_cell(sides[side], row[side], open_fragment[side])
    keys = [sorted(_fragment_key(f[:1], f[1:]) for f in side) for side in sides]
    text = "\x1e".join(["L " + key for key in keys[0]] + ["R " + key for key in keys[1]])
    return "CLASS " + text, min(end + 1, len(lines))


def _add_cell(side: list[list[str]], cell: str, slot: list[str] | None) -> list[str] | None:
    """Adds ``cell`` to its column: a ``Net:``/``Instance:`` cell starts a fragment, another cell is an
    entry of the open fragment; absent markers and empty cells are dropped."""
    text = cell.strip()
    if text.startswith(("Net: ", "Instance: ")):
        side.append([text])
        return side[-1]
    if text not in _ABSENT and slot is not None:
        slot.append(text)
    return slot


def _mismatch_section(lines: list[str], index: int) -> int:
    """``lines[index]`` is a ``NET``/``DEVICE mismatches:`` line: canonicalizes its classes in place."""
    column = _column(lines, index + 1)
    if column is None:
        return index + 1
    start = end = index + 2
    classes: list[str] = []
    while end + 1 < len(lines) and lines[end] == "" and _is_full_divider(lines[end + 1]):
        text, end = _read_class(lines, end, column)
        classes.append(text)
    lines[start:end] = sorted(classes)
    return start + len(classes)


def _property_run(lines: list[str], index: int) -> int:
    """``lines[index]`` starts a property block: sorts the blocks of the run and their lines."""
    blocks: list[list[str]] = []
    end = index
    while end < len(lines) and lines[end] != "" and not lines[end].startswith(_PROPERTY_END):
        if _PROPERTY_BLOCK.match(lines[end]) or not blocks:
            blocks.append([lines[end]])
        else:
            blocks[-1].append(lines[end])
        end += 1
    canonical = sorted([block[0], *sorted(block[1:])] for block in blocks)
    lines[index:end] = [line for block in canonical for line in block]
    return end


def _message_run(lines: list[str], index: int) -> int:
    end = index
    while end < len(lines) and _MESSAGE.match(lines[end]):
        end += 1
    lines[index:end] = sorted(lines[index:end])
    return end


def canonical_report(text: str) -> str:
    """``comp.out`` with its order-insensitive sections in canonical order (module docstring)."""
    lines = text.split("\n")
    index = 0
    while index < len(lines):
        line = lines[index]
        if line == "Subcircuit summary:":
            index = _table_section(lines, index, "Number of devices:")
        elif line == "Subcircuit pins:":
            index = _table_section(lines, index, None)
        elif line.startswith(("NET mismatches:", "DEVICE mismatches:")):
            index = _mismatch_section(lines, index)
        elif _PROPERTY_BLOCK.match(line):
            index = _property_run(lines, index)
        elif _MESSAGE.match(line):
            index = _message_run(lines, index)
        else:
            index += 1
    return "\n".join(lines)


def _multiset(items: list, canonical=lambda item: item) -> list:
    return sorted((canonical(item) for item in items), key=lambda item: json.dumps(item, sort_keys=True))


def _named_entries(side: list) -> list:
    """One circuit's nets or instances: ``[name, [entry...]]`` with the entries as a multiset."""
    return _multiset(side, lambda net: [net[0], _multiset(net[1])] if isinstance(net, list) and len(net) == 2
                     and isinstance(net[1], list) else net)


def _fragments(value: list) -> list:
    return _multiset(value, lambda fragment: [_named_entries(side) if isinstance(side, list) else side
                                              for side in fragment] if isinstance(fragment, list) else fragment)


def _properties(value: list) -> list:
    return _multiset(value, lambda pair: [[inst[0], _multiset(inst[1])] if isinstance(inst, list) and len(inst) == 2
                                          and isinstance(inst[1], list) else inst for inst in pair]
                     if isinstance(pair, list) else pair)


def _pins(value: list) -> list:
    if len(value) == 2 and all(isinstance(side, list) for side in value):
        # An unpaired pin of the longer list is kept, paired with None, so a renamed extra pin differs.
        pairs = itertools.zip_longest(*value, fillvalue=None)
        return _multiset([list(pair) for pair in pairs]) + [len(value[0]), len(value[1])]
    return value


def _devices(value: list) -> list:
    return [_multiset(side) if isinstance(side, list) else side for side in value]


_SET_KEYS = {"devices": _devices, "badnets": _fragments, "badelements": _fragments, "properties": _properties,
             "pins": _pins}


def canonical_json(value: object) -> object:
    """The parsed ``comp.json`` with its order-insensitive lists as sorted multisets (module docstring)."""
    if not isinstance(value, list):
        return value
    pairs = []
    for pair in value:
        if not isinstance(pair, dict):
            pairs.append(pair)
            continue
        pairs.append([[key, _SET_KEYS[key](item) if key in _SET_KEYS and isinstance(item, list) else item]
                      for key, item in pair.items()])
    return pairs


def canonicalize(data: bytes) -> bytes:
    text = data.decode("utf-8", errors="replace")
    if not text.lstrip().startswith("["):
        return canonical_report(text).encode("utf-8")
    try:
        value = json.loads(text)
    except ValueError:
        return b"<not json>\n" + data
    return (json.dumps(canonical_json(value), indent=1, sort_keys=True) + "\n").encode("utf-8")
