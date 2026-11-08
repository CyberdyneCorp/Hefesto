# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Executable model of requirement "Magic string hash table".

Capability hefesto-output-determinism. This is a direct transcription of the
requirement text, independent of both Magic's utils/hash.c and Hefesto's C++
implementation. It serves two purposes:

* the ``model`` capture runner, which writes *provisional* expected output for
  cases whose reference tool is ``magic-hash-harness`` when Magic's code cannot
  be compiled (provenance then says ``reference_captured: false``);
* a stand-in for ``hefesto-cli`` in the replay unit tests.

The script language is the one documented in harness/magic_hash/driver.c.
"""
from __future__ import annotations

COMMAND_NAME = "hefesto::debug::hashorder"
_MASK64 = (1 << 64) - 1


def string_hash(key: bytes) -> int:
    """h = c + (h<<6) + (h<<16) - h over signed key bytes, mod 2^64."""
    h = 0
    for byte in key:
        c = byte - 256 if byte >= 128 else byte
        h = (c + (h << 6) + (h << 16) - h) & _MASK64
    return h


class MagicHashTable:
    """Chained table: head insertion, growth to 4x at 3 * size entries."""

    def __init__(self, requested: int) -> None:
        self._size, self._shift = 2, 29
        while self._size < max(2, requested):
            self._size, self._shift = self._size * 2, self._shift - 1
        self._buckets: list[list[bytes]] = [[] for _ in range(self._size)]
        self._count = 0

    def bucket(self, key: bytes) -> int:
        mixed = (string_hash(key) * 1103515245 + 12345) & _MASK64
        return (mixed >> self._shift) & (self._size - 1)

    def insert(self, key: bytes) -> None:
        if key in self._buckets[self.bucket(key)]:
            return
        if self._count >= 3 * self._size:
            self._grow()
        self._buckets[self.bucket(key)].insert(0, key)
        self._count += 1

    def remove(self, key: bytes) -> None:
        chain = self._buckets[self.bucket(key)]
        if key in chain:
            chain.remove(key)  # the growth count is not decreased

    def keys(self) -> list[bytes]:
        return [key for chain in self._buckets for key in chain]

    def _grow(self) -> None:
        old = self.keys()
        self._size, self._shift = self._size * 4, self._shift - 2
        self._buckets = [[] for _ in range(self._size)]
        for key in old:
            self._buckets[self.bucket(key)].insert(0, key)
        self._count = len(old)


def run_command(words: list[bytes]) -> bytes:
    """Run one ``hefesto::debug::hashorder`` command; return its output line."""
    if len(words) < 2 or words[0] != COMMAND_NAME.encode():
        raise ValueError(f"not a {COMMAND_NAME} command: {words[:1]!r}")
    table = MagicHashTable(int(words[1]))
    for op in words[2:]:
        if op[:1] == b"+" and len(op) > 1:
            table.insert(op[1:])
        elif op[:1] == b"-" and len(op) > 1:
            table.remove(op[1:])
        else:
            raise ValueError(f"bad op {op!r}")
    return b" ".join(table.keys()) + b"\n"


def run_script(script: bytes) -> bytes:
    """Run a whole script; one output line per command (driver.c semantics)."""
    out = []
    for line in script.splitlines():
        words = line.split()
        if words and not words[0].startswith(b"#"):
            out.append(run_command(words))
    return b"".join(out)
