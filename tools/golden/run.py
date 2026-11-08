#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Alias of replay.py (same arguments). design.md D8 names the runner replay.py."""
from __future__ import annotations

import sys

from replay import main

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
