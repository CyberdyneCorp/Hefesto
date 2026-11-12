# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Golden case model: loading and validating ``case.toml`` (design D8).

A case lives in ``tests/golden/<AREA>/cases/<case-id>/``::

    case.toml        manifest (schema below)
    input/           tech file, layout, reference script, Hefesto script
    expected/        captured outputs + provenance.json (written by capture.py)

``case.toml`` schema (version 1)::

    schema = 1
    id = "CORE-HASH-01"              # must equal the directory name
    area = "CORE"                    # must equal the <AREA> directory
    capability = "hefesto-output-determinism"
    requirements = ["Magic string hash table"]
    tool = "magic"                   # magic | netgen | ngspice | xschem | magic-hash-harness
    description = "..."
    slow = true                      # optional: CTest label golden-slow instead of golden

    [reference]                      # how capture.py runs the reference tool
    script = "input/commands.tcl"    # fed to the tool (path relative to the case)
    tech = "input/min.tech"          # optional (magic: passed with -T)
    argv = ["magic", "-dnull", ...]  # optional; overrides the tool's default argv
    display = "xvfb"                 # optional: run under a virtual X server (GUI cases, Tk)
    stdin = "input/commands.tcl"     # optional: the tool's standard input (path relative to the case)

    [env]                            # optional: environment of BOTH runs (reference and replay);
    PDK_ROOT = "{work}/input/pdk"    # {work} = the work directory (/work in the container)

    [hefesto]                        # how replay.py runs hefesto-cli
    args = ["{input}/commands.hef"]  # {input} = work copy of input/, {work} = work dir
    requires_commands = []           # optional: commands the case needs (skip if unknown)
    stdin = "input/commands.tcl"     # optional: hefesto-cli's standard input (default: none)
    m0_only = false                  # optional: uses a temporary M0-only command

    [[external]]                     # optional: pinned data copied into input/ (goldenlib/external.py)
    source = "pdk"                   # pdk | scmos | magic-sys | magic-tcl
    path = "sky130A/libs.tech/magic/sky130A.tech"
    to = "sky130A.tech"

    [[outputs]]                      # one per compared output
    name = "stdout.txt"              # file name under expected/
    source = "stdout"                # stdout | stderr | a path relative to the work dir
                                     # | derived:mag-drc-sections:<glob> (goldenlib/derived.py)
    comparator = "bytes"             # see compare/ (bytes, ext-cap)
    masks = []                       # time-dependent field masks (see compare/masks.py)
"""
from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

from .derived import is_derived
from .derived import parse as parse_derived
from .external import External, parse_external

REFERENCE_TOOLS = ("magic", "netgen", "ngspice", "xschem", "magic-hash-harness")
STREAM_SOURCES = ("stdout", "stderr")


class CaseError(ValueError):
    """A case directory or manifest is malformed."""


@dataclass(frozen=True)
class Output:
    name: str
    source: str
    comparator: str
    masks: tuple[str, ...] = ()


@dataclass(frozen=True)
class Case:
    root: Path
    id: str
    area: str
    capability: str
    requirements: tuple[str, ...]
    tool: str
    outputs: tuple[Output, ...]
    reference: dict = field(default_factory=dict)
    hefesto: dict = field(default_factory=dict)
    description: str = ""
    externals: tuple[External, ...] = ()
    env: dict = field(default_factory=dict)

    @property
    def input_dir(self) -> Path:
        return self.root / "input"

    @property
    def expected_dir(self) -> Path:
        return self.root / "expected"

    @property
    def is_captured(self) -> bool:
        return (self.expected_dir / "provenance.json").is_file()


def _require(data: dict, key: str, kind: type, where: str):
    value = data.get(key)
    if not isinstance(value, kind):
        raise CaseError(f"{where}: '{key}' must be a {kind.__name__}")
    return value


def _parse_output(raw: dict, index: int, known_comparators) -> Output:
    where = f"outputs[{index}]"
    name = _require(raw, "name", str, where)
    if "/" in name or name in ("", ".", "..", "provenance.json"):
        raise CaseError(f"{where}: bad output name '{name}'")
    comparator = _require(raw, "comparator", str, where)
    if known_comparators is not None and comparator not in known_comparators:
        raise CaseError(f"{where}: unknown comparator '{comparator}'")
    masks = tuple(raw.get("masks", []))
    source = _require(raw, "source", str, where)
    if is_derived(source):
        try:
            parse_derived(source)
        except ValueError as error:
            raise CaseError(f"{where}: {error}") from None
    return Output(name, source, comparator, masks)


def _check_paths(case: Case) -> None:
    for table, key in (("reference", "script"), ("reference", "tech"), ("reference", "stdin"), ("hefesto", "stdin")):
        rel = getattr(case, table).get(key)
        if rel is not None and not (case.root / rel).is_file():
            raise CaseError(f"{case.id}: {table}.{key} '{rel}' does not exist")
    if not case.input_dir.is_dir():
        raise CaseError(f"{case.id}: missing input/ directory")


ENV_NAME = re.compile(r"^[A-Z_][A-Z0-9_]*$")
DISPLAYS = ("xvfb",)


def _parse_env(data: dict, where: str) -> dict:
    """The ``[env]`` table: upper-case names, string values (``{work}`` substituted per run)."""
    raw = data.get("env", {})
    if not isinstance(raw, dict):
        raise CaseError(f"{where}: 'env' must be a table")
    for name, value in raw.items():
        if not ENV_NAME.match(name) or not isinstance(value, str):
            raise CaseError(f"{where}: env.{name} must be an upper-case name with a string value")
    return dict(raw)


def substitute_env(env: dict, work: str) -> dict:
    """``env`` with ``{work}`` replaced by the run's work directory."""
    return {name: value.replace("{work}", work) for name, value in env.items()}


def _parse_externals(data: dict, where: str) -> tuple[External, ...]:
    raw = data.get("external", [])
    if not isinstance(raw, list):
        raise CaseError(f"{where}: 'external' must be an array of tables")
    try:
        return tuple(parse_external(entry, i) for i, entry in enumerate(raw))
    except ValueError as error:
        raise CaseError(f"{where}: {error}") from None


def load_case(root: Path, known_comparators=None) -> Case:
    """Load and validate ``root/case.toml``; raise CaseError when malformed."""
    root = Path(root).resolve()
    manifest = root / "case.toml"
    if not manifest.is_file():
        raise CaseError(f"{root}: no case.toml")
    data = tomllib.loads(manifest.read_text(encoding="utf-8"))
    where = str(manifest)
    if data.get("schema") != 1:
        raise CaseError(f"{where}: unsupported schema {data.get('schema')!r}")
    tool = _require(data, "tool", str, where)
    if tool not in REFERENCE_TOOLS:
        raise CaseError(f"{where}: unknown tool '{tool}'")
    raw_outputs = _require(data, "outputs", list, where)
    if not raw_outputs:
        raise CaseError(f"{where}: at least one [[outputs]] entry is required")
    case = Case(
        root=root,
        id=_require(data, "id", str, where),
        area=_require(data, "area", str, where),
        capability=_require(data, "capability", str, where),
        requirements=tuple(_require(data, "requirements", list, where)),
        tool=tool,
        outputs=tuple(_parse_output(o, i, known_comparators) for i, o in enumerate(raw_outputs)),
        reference=data.get("reference", {}),
        hefesto=data.get("hefesto", {}),
        description=data.get("description", ""),
        externals=_parse_externals(data, where),
        env=_parse_env(data, where),
    )
    display = case.reference.get("display")
    if display is not None and display not in DISPLAYS:
        raise CaseError(f"{where}: reference.display must be one of {', '.join(DISPLAYS)}")
    if case.id != root.name:
        raise CaseError(f"{where}: id '{case.id}' does not match directory '{root.name}'")
    if root.parent.name != "cases" or root.parent.parent.name != case.area:
        raise CaseError(f"{where}: case must live in tests/golden/{case.area}/cases/{case.id}")
    _check_paths(case)
    return case


def discover(golden_root: Path) -> list[Path]:
    """All case directories under ``<golden_root>/<AREA>/cases/``, sorted."""
    return sorted(p.parent for p in Path(golden_root).glob("*/cases/*/case.toml"))
