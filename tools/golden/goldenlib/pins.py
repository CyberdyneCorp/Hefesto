# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Access to ``tools/golden/pins.toml``, the reference-tool pins."""
from __future__ import annotations

import tomllib
from pathlib import Path

PINS_FILE = Path(__file__).resolve().parent.parent / "pins.toml"


def load_pins(path: Path = PINS_FILE) -> dict:
    return tomllib.loads(Path(path).read_text(encoding="utf-8"))


def base_image_ref(pins: dict) -> str:
    """Fully pinned image reference: name:tag@digest."""
    base = pins["base_image"]
    return f"{base['name']}:{base['tag']}@{base['digest']}"


def tool_pin(pins: dict, tool: str) -> dict:
    """Pin of the tool that produces a case's reference output."""
    if tool == "magic-hash-harness":
        tool = pins["harness"]["magic_hash"]["tool"]
    return pins["tools"][tool]
