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
    tool = "magic"                   # magic | netgen | ngspice | xschem | magic-hash-harness | oasis-spec
                                     # (oasis-spec: the expected bytes are the spec listings that
                                     # authoring/oasgen.py assembles into input/spec/; the run copies them)
    description = "..."
    slow = true                      # optional: CTest label golden-slow instead of golden
    gds_rt = true                    # optional: CTest label golden-gds-rt; replay.py skips the case
                                     # unless --include-gds-rt (m5-mask-gds-cif D19, the MASK-RT cases)
    capture_blocked = "..."          # optional: why the reference tool cannot capture the case yet
                                     # (it crashes, its output is unstable, an input is missing);
                                     # capture.py skips the case while it has no expected/

    [reference]                      # how capture.py runs the reference tool
    script = "input/commands.tcl"    # fed to the tool (path relative to the case)
    tech = "input/min.tech"          # optional (magic: passed with -T)
    argv = ["magic", "-dnull", ...]  # optional; overrides the tool's default argv
    display = "xvfb"                 # optional: run under a virtual X server (GUI cases, Tk)
    image = "magic-gl"               # optional: the image variant to run in (pins.toml image_variants;
                                     # magic-gl: Magic with OpenGL, the MISC-3D-* cases of m10-3d-view
                                     # D13; needs tool magic and display "xvfb")
    gl_capture = true                # optional (image magic-gl only): preload the polygon-recording
                                     # shim (tools/golden/gl_capture/, m10-3d-view D13 task 12.4) into
                                     # Magic; it writes the file named by [env] HEFESTO_GL_CAPTURE
    stdin = "input/commands.tcl"     # optional: the tool's standard input (path relative to the case)
    timeout_s = 3600                 # optional: seconds before the run is abandoned (default 600)
    xschem_sets = ["sky130-xschem"]  # optional (tool xschem): xschem data sets of scripts/fetch_pdk.py,
                                     # mounted read-only at /pdk/<variant>, PDK_ROOT=/pdk (m8-schematic D16)

    [env]                            # optional: environment of BOTH runs (reference and replay);
    PDK_ROOT = "{work}/input/pdk"    # {work} = the work directory (/work in the container)

    [hefesto]                        # how replay.py runs hefesto-cli
    args = ["{input}/commands.hef"]  # {input} = work copy of input/, {work} = work dir
    requires_commands = []           # optional: commands the case needs (skip if unknown)
    stdin = "input/commands.tcl"     # optional: hefesto-cli's standard input (default: none)
    m0_only = false                  # optional: uses a temporary M0-only command
    timeout_s = 3600                 # optional: seconds before replay.py abandons the run (default 300)

    [[external]]                     # optional: pinned data copied into input/ (goldenlib/external.py)
    source = "pdk"                   # pdk | scmos | magic-sys | magic-tcl
    path = "sky130A/libs.tech/magic/sky130A.tech"
    to = "sky130A.tech"

    [reference] mode = "external-command"   # optional: run the standalone program that
                                     # input/command.toml names instead of a script
                                     # (goldenlib/external_command.py; NL-CHK-01, m7-extraction D15)

    [[collect]]                      # optional: outputs gathered by file-name pattern, each compared as
    pattern = "*.ext"                # expected/collected/<path> (goldenlib/collect.py; m7-extraction D21)
    comparator = "ext-cap"
    masks = ["ext-timestamp"]

    [[outputs]]                      # one per compared output (at least one [[outputs]] or [[collect]])
    name = "stdout.txt"              # file name under expected/
    source = "stdout"                # stdout | stderr | exitcode (the exit status as one line)
                                     # | a path relative to the work dir
                                     # | derived:mag-drc-sections:<glob> (goldenlib/derived.py)
    comparator = "bytes"             # see compare/ (bytes, ext-cap)
    masks = []                       # time-dependent field masks (see compare/masks.py)
    display = "gds-records"          # optional: view that renders a difference (compare/__init__.py)
"""
from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass, field, replace
from pathlib import Path

from . import collect as collect_mod
from . import external_command
from .collect import Collect
from .derived import is_derived
from .derived import parse as parse_derived
from .external import External, parse_external

REFERENCE_TOOLS = ("magic", "netgen", "ngspice", "xschem", "magic-hash-harness", "oasis-spec")
STREAM_SOURCES = ("stdout", "stderr")
EXIT_SOURCE = "exitcode"  # the tool's exit status as one decimal line (the SCHEM cases, m8-schematic D16)
RUN_SOURCES = (*STREAM_SOURCES, EXIT_SOURCE)


def run_output(source: str, stdout: bytes, stderr: bytes, returncode: int) -> bytes:
    """The output of a run source: a stream, or the exit status written as ``<n>`` LF."""
    if source == EXIT_SOURCE:
        return f"{returncode}\n".encode()
    return stdout if source == "stdout" else stderr


DISPLAY_VIEWS = ("gds-records", "oasis-records")  # the display views of compare/__init__.py (DISPLAYS)


class CaseError(ValueError):
    """A case directory or manifest is malformed."""


@dataclass(frozen=True)
class Output:
    name: str
    source: str
    comparator: str
    masks: tuple[str, ...] = ()
    display: str | None = None


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
    slow: bool = False
    gds_rt: bool = False
    capture_blocked: str = ""  # why capture.py cannot capture the case yet (empty: it can)
    collect: tuple[Collect, ...] = ()  # [[collect]] entries (goldenlib/collect.py)
    command: external_command.Command | None = None  # input/command.toml of an external-command case

    @property
    def label(self) -> str:
        """The CTest label: ``golden-gds-rt``, ``golden-slow`` or ``golden``."""
        if self.gds_rt:
            return "golden-gds-rt"
        return "golden-slow" if self.slow else "golden"

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
    display = raw.get("display")
    if display is not None and display not in DISPLAY_VIEWS:
        raise CaseError(f"{where}: unknown display view '{display}'")
    return Output(name, source, comparator, masks, display)


def _check_paths(case: Case) -> None:
    for table, key in (("reference", "script"), ("reference", "tech"), ("reference", "stdin"), ("hefesto", "stdin")):
        rel = getattr(case, table).get(key)
        if rel is not None and not (case.root / rel).is_file():
            raise CaseError(f"{case.id}: {table}.{key} '{rel}' does not exist")
    if not case.input_dir.is_dir():
        raise CaseError(f"{case.id}: missing input/ directory")


ENV_NAME = re.compile(r"^[A-Z_][A-Z0-9_]*$")
DISPLAYS = ("xvfb",)
IMAGE_VARIANTS = ("magic-gl",)  # pins.toml [image_variants]


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


def _flag(data: dict, key: str, where: str) -> bool:
    value = data.get(key, False)
    if not isinstance(value, bool):
        raise CaseError(f"{where}: '{key}' must be true or false")
    return value


def _reason(data: dict, key: str, where: str) -> str:
    value = data.get(key, "")
    if not isinstance(value, str) or (key in data and not value.strip()):
        raise CaseError(f"{where}: '{key}' must be a non-empty string (the reason)")
    return value


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
    raw_outputs = data.get("outputs", [])
    if not isinstance(raw_outputs, list):
        raise CaseError(f"{where}: 'outputs' must be an array of tables")
    collect = _parse_collect(data, where, known_comparators)
    if not raw_outputs and not collect:
        raise CaseError(f"{where}: at least one [[outputs]] or [[collect]] entry is required")
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
        slow=_flag(data, "slow", where),
        gds_rt=_flag(data, "gds_rt", where),
        capture_blocked=_reason(data, "capture_blocked", where),
        collect=collect,
    )
    display = case.reference.get("display")
    if display is not None and display not in DISPLAYS:
        raise CaseError(f"{where}: reference.display must be one of {', '.join(DISPLAYS)}")
    _check_xschem_sets(case, where)
    _check_image(case, where)
    _check_gl_capture(case, where)
    if case.id != root.name:
        raise CaseError(f"{where}: id '{case.id}' does not match directory '{root.name}'")
    if root.parent.name != "cases" or root.parent.parent.name != case.area:
        raise CaseError(f"{where}: case must live in tests/golden/{case.area}/cases/{case.id}")
    _check_paths(case)
    for table in ("reference", "hefesto"):
        _check_timeout(getattr(case, table), table, where)
    return _with_command(case, where)


def _parse_collect(data: dict, where: str, known_comparators) -> tuple[Collect, ...]:
    raw = data.get("collect", [])
    if not isinstance(raw, list):
        raise CaseError(f"{where}: 'collect' must be an array of tables")
    try:
        return tuple(collect_mod.parse(entry, i, known_comparators) for i, entry in enumerate(raw))
    except ValueError as error:
        raise CaseError(f"{where}: {error}") from None


def _with_command(case: Case, where: str) -> Case:
    """``case`` with its input/command.toml when ``[reference] mode`` is external-command."""
    mode = case.reference.get("mode")
    if mode is None:
        return case
    if mode != external_command.MODE:
        raise CaseError(f"{where}: reference.mode must be \"{external_command.MODE}\"")
    if "script" in case.reference or "argv" in case.reference:
        raise CaseError(f"{where}: an external-command case takes its command from {external_command.COMMAND_FILE}")
    try:
        command = external_command.load(case.root)
    except (external_command.CommandError, ValueError) as error:
        raise CaseError(str(error)) from None
    return replace(case, command=command)


XSCHEM_SETS = ("sky130-xschem", "gf180mcu-xschem", "ihp-xschem")


def _check_xschem_sets(case: Case, where: str) -> None:
    sets = case.reference.get("xschem_sets", [])
    if not isinstance(sets, list) or not all(s in XSCHEM_SETS for s in sets) or len(set(sets)) != len(sets):
        raise CaseError(f"{where}: reference.xschem_sets must list distinct sets of {', '.join(XSCHEM_SETS)}")
    if sets and case.tool != "xschem":
        raise CaseError(f"{where}: reference.xschem_sets applies to tool xschem only")


def _check_image(case: Case, where: str) -> None:
    image = case.reference.get("image")
    if image is None:
        return
    if image not in IMAGE_VARIANTS:
        raise CaseError(f"{where}: reference.image must be one of {', '.join(IMAGE_VARIANTS)}")
    if case.tool != "magic" or case.reference.get("display") != "xvfb":
        raise CaseError(f"{where}: reference.image {image} needs tool magic and display \"xvfb\"")


def _check_gl_capture(case: Case, where: str) -> None:
    value = case.reference.get("gl_capture", False)
    if not isinstance(value, bool):
        raise CaseError(f"{where}: reference.gl_capture must be true or false")
    if value and (image_variant(case) != "magic-gl" or "HEFESTO_GL_CAPTURE" not in case.env):
        raise CaseError(f"{where}: reference.gl_capture needs image \"magic-gl\" and env.HEFESTO_GL_CAPTURE")


def gl_capture(case: Case) -> bool:
    """Whether Magic runs with the polygon-recording shim preloaded (``[reference] gl_capture``)."""
    return case.reference.get("gl_capture", False) is True


def image_variant(case: Case) -> str | None:
    """The image variant a case runs in (``[reference] image``), or None for the default image."""
    return case.reference.get("image")


MAX_TIMEOUT_S = 6 * 3600  # the longest capture shard (golden-capture.yml) is 200 minutes


def _check_timeout(table: dict, name: str, where: str) -> None:
    value = table.get("timeout_s")
    if value is not None and (isinstance(value, bool) or not isinstance(value, int)
                              or not 0 < value <= MAX_TIMEOUT_S):
        raise CaseError(f"{where}: {name}.timeout_s must be an integer of seconds in 1..{MAX_TIMEOUT_S}")


def run_timeout(table: dict, default: int) -> int:
    """The ``timeout_s`` of a ``[reference]`` or ``[hefesto]`` table (validated by load_case), else
    ``default``: the library round trips (MASK-RT-*, m5-mask-gds-cif D19) run for up to 35 minutes."""
    return int(table.get("timeout_s", default))


def discover(golden_root: Path) -> list[Path]:
    """All case directories under ``<golden_root>/<AREA>/cases/``, sorted."""
    return sorted(p.parent for p in Path(golden_root).glob("*/cases/*/case.toml"))
