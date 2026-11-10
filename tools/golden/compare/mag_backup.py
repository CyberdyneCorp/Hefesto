# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonical form of crash-backup and archive files (``mag-backup``).

Capability cell-io-safety, requirement "Crash backup file format", "Cell
order": Magic writes the cell records of a backup or ``archive write`` file in
its cell hash-table order; Hefesto writes them sorted by cell name (decision
D-9, Q-CELL-28), the reader is order-independent, and golden comparison
canonicalizes the record order.

A file is a sequence of records, each starting at a line ``file <location>``
and running up to the next such line, followed by the final ``end [<cell>]``
line. The canonical form is the records sorted by their bytes, then the
``end`` line; every byte of every record is kept, so a difference inside a
record still fails. Text before the first record (none in a well-formed file)
stays first. A file with no ``end`` line is returned unchanged.
"""
from __future__ import annotations


def canonicalize(data: bytes) -> bytes:
    lines = data.split(b"\n")
    end = next((i for i in range(len(lines) - 1, -1, -1) if lines[i] == b"end" or lines[i].startswith(b"end ")), None)
    if end is None:
        return data
    head: list[bytes] = []
    records: list[list[bytes]] = []
    for line in lines[:end]:
        if line.startswith(b"file "):
            records.append([line])
        elif records:
            records[-1].append(line)
        else:
            head.append(line)
    ordered = sorted(b"\n".join(r) for r in records)
    return b"\n".join([*head, *ordered, *lines[end:]])
