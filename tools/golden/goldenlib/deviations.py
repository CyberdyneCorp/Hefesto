# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Documented deviations of golden cases (``tests/golden/deviations.toml``).

Some captured Magic outputs show behavior Hefesto deliberately does not
reproduce: a Magic bug fixed under decision X-2, or a resolved open
question. Such a case cannot pass, and masking it would hide every other
difference in the same output. Instead it is listed here with the decision
that justifies it and the outputs allowed to differ::

    [TECH-G43]
    decisions = ["Q-TECH-25"]        # docs/decisions entries
    outputs = ["stdout.txt"]         # output names of the case (at least one)
    reason = "tech revert * re-locks the layers locked in the technology file"

A case whose Magic output needs a feature of a later milestone (a command
or engine that does not exist yet) is listed with ``deferred``, the
milestone of docs/roadmap.md that brings it, instead of or beside
``decisions``; the reason names the missing feature::

    [CMDEDIT-LBL-07]
    deferred = "M3"                  # a "## M<n>" heading of docs/roadmap.md
    outputs = ["stdout.txt"]
    reason = "goto reports the DRC check plane, which arrives with the DRC engine"

Every entry needs at least one decision or a ``deferred`` milestone.

replay.py reports such a case as ``deviation`` (skipped, not passed) when
only the listed outputs differ. It still fails when any other output
differs, and it fails when the case passes, so a stale entry is removed
instead of hiding a regression later. Every entry must name a captured case
(or, with ``deferred``, a case whose capture is pending in CI) and existing
outputs (``check``), and each decision must be a heading in ``docs/decisions``
(``tests/test_deviations.py``).
"""
from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass
from pathlib import Path

FILE_NAME = "deviations.toml"


class DeviationError(ValueError):
    """The deviations file is malformed."""


@dataclass(frozen=True)
class Deviation:
    case_id: str
    decisions: tuple[str, ...]
    outputs: tuple[str, ...]
    reason: str
    deferred: str = ""  # the roadmap milestone that brings the missing feature

    def basis(self) -> str:
        """The decisions and the deferral, e.g. ``Q-UI-12; deferred to M4``."""
        parts = [", ".join(self.decisions)] if self.decisions else []
        if self.deferred:
            parts.append(f"deferred to {self.deferred}")
        return "; ".join(parts)

    def describe(self) -> str:
        return f"documented deviation ({self.basis()}): {self.reason}"


def _strings(entry: dict, key: str, case_id: str) -> tuple[str, ...]:
    value = entry.get(key)
    if not isinstance(value, list) or not value or not all(isinstance(v, str) and v for v in value):
        raise DeviationError(f"{FILE_NAME} [{case_id}]: '{key}' must be a non-empty list of strings")
    return tuple(value)


def parse(text: str) -> dict[str, Deviation]:
    """Parse the deviations file; raise DeviationError when malformed."""
    data = tomllib.loads(text)
    out = {}
    for case_id, entry in data.items():
        if not isinstance(entry, dict):
            raise DeviationError(f"{FILE_NAME}: '{case_id}' must be a table")
        unknown = set(entry) - {"decisions", "outputs", "reason", "deferred"}
        if unknown:
            raise DeviationError(f"{FILE_NAME} [{case_id}]: unknown keys {sorted(unknown)}")
        reason = entry.get("reason")
        if not isinstance(reason, str) or not reason.strip():
            raise DeviationError(f"{FILE_NAME} [{case_id}]: 'reason' must be a non-empty string")
        deferred = _deferred(entry, case_id)
        decisions = _strings(entry, "decisions", case_id) if "decisions" in entry or not deferred else ()
        out[case_id] = Deviation(case_id, decisions, _strings(entry, "outputs", case_id), reason.strip(), deferred)
    return out


def _deferred(entry: dict, case_id: str) -> str:
    value = entry.get("deferred", "")
    if not isinstance(value, str) or (value and not re.fullmatch(r"M\d+", value)):
        raise DeviationError(f"{FILE_NAME} [{case_id}]: 'deferred' must be a milestone such as \"M3\"")
    return value


def load(golden_root: Path) -> dict[str, Deviation]:
    """The deviations of ``<golden_root>/deviations.toml`` (empty when absent)."""
    path = Path(golden_root) / FILE_NAME
    return parse(path.read_text(encoding="utf-8")) if path.is_file() else {}


def check(deviation: Deviation, output_names: list[str]) -> None:
    """Raise DeviationError when the entry names an output the case lacks."""
    missing = [name for name in deviation.outputs if name not in output_names]
    if missing:
        raise DeviationError(f"{FILE_NAME} [{deviation.case_id}]: no such outputs {missing}")


def classify(deviation: Deviation | None, failed_outputs: list[str]) -> tuple[str, str]:
    """The case status and detail for the outputs that did not match.

    ``pass`` when nothing failed and no deviation is listed; ``fail`` when an
    output outside the deviation failed, or when a listed deviation no
    longer reproduces; ``deviation`` when only listed outputs failed.
    """
    if deviation is None:
        return ("fail", "") if failed_outputs else ("pass", "")
    if not failed_outputs:
        return "fail", (f"listed in {FILE_NAME} ({deviation.basis()}) but every output "
                        f"now matches: remove the entry")
    unexpected = [name for name in failed_outputs if name not in deviation.outputs]
    if unexpected:
        return "fail", f"outputs outside the documented deviation differ: {', '.join(unexpected)}"
    return "deviation", deviation.describe()
