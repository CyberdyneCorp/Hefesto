#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Stand-in for hefesto-cli in replay tests: `fake_hefesto_cli.py [--opt...] SCRIPT`.

Runs `hefesto::debug::hashorder` lines with the requirement model and prints
each result on stdout. Any other command fails like the real dispatcher, with
diagnostic CMD-DISPATCH-001. FAKE_HEFESTO_MODE=no-debug makes the debug
command unknown, FAKE_HEFESTO_MODE=wrong reverses every result.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from goldenlib.magic_hash_model import COMMAND_NAME, run_command  # noqa: E402


def main(argv: list[str]) -> int:
    mode = os.environ.get("FAKE_HEFESTO_MODE", "")
    script = next(arg for arg in argv if not arg.startswith("--"))  # options are ignored
    for line in Path(script).read_bytes().splitlines():
        words = line.split()
        if not words or words[0].startswith(b"#"):
            continue
        if words[0] != COMMAND_NAME.encode() or mode == "no-debug":
            name = words[0].decode(errors="replace")
            print(f'error: CMD-DISPATCH-001: unknown command "{name}"', file=sys.stderr)
            return 1
        result = run_command(words)
        if mode == "wrong":
            result = b" ".join(reversed(result.split())) + b"\n"
        sys.stdout.buffer.write(result)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
