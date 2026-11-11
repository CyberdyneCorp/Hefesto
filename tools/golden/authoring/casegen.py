# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Writes golden case directories from compact specs (authoring aid, design D8 of m1, D9 of m2).

A spec is one :class:`Spec`: the commands, the input files and the outputs. ``write``
turns it into ``tests/golden/<AREA>/cases/<ID>/`` with ``case.toml`` and
``input/commands.tcl`` (README.md, "M1: cases compared with Magic's console"), the one
Tcl script both tools run: Magic as ``magic -dnull -noconsole input/commands.tcl`` and,
since hefesto-cli embeds Tcl (m4-scripting design D16, tasks.md 13.2), hefesto-cli as
``--magic -dnull -noconsole -norc input/commands.tcl`` (``-norc``: no developer
``~/.magicrc`` in a replay; the reference image has none either). The ``.hef`` script
of M1-M3 is gone; ``write`` deletes a stale one:

* each command line becomes ``catch {cmd} result; puts $result`` in Tcl, so every
  command prints exactly one result line and a Tcl error never stops the script;
* with ``streams=False`` (table dumps) the Tcl lines are ``catch {cmd}`` and only the
  file outputs are compared, so thousands of empty result lines are not recorded;
* every ``.mag`` output is compared byte for byte with the ``mag-timestamp`` mask
  (wall-clock stamps, cell-timestamps), unless the spec lists other masks, and with
  ``mag-checkpaint`` (the DRC check areas Magic's edits create; M1 has no DRC) unless an
  input file has a ``<< checkpaint >>`` section, which the case then checks is kept, the
  spec sets ``keep_checkpaint`` (the M3 DRC cases) or ``CHECKPAINT_MASK_RETIRED`` is set
  (m3-drc design D17);
* ``stream_comparator`` picks the console comparator (``drc-console`` for DRC cases),
  ``output_names`` names derived outputs (goldenlib/derived.py) and ``slow`` labels a
  case ``golden-slow``;
* a case whose input files name one of Magic's default outline fonts gets that font
  as a ``magic-sys`` external input (spec ``fonts`` adds fonts for external inputs);
* the console streams always carry ``magic-tech-sections`` (Magic's messages from the
  cifinput/cifoutput/extract technology sections, which M1 does not interpret).

Only the case directories named by specs are written; ``expected/`` is never touched.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
GOLDEN = REPO / "tests" / "golden"
HEADER = "# SPDX-License-Identifier: Apache-2.0\n# Copyright 2026 The Hefesto Authors\n"


@dataclass
class Spec:
    id: str
    capability: str
    requirements: list[str]
    description: str
    cmds: list[str]
    files: dict[str, str | bytes] = field(default_factory=dict)
    outputs: list[str] = field(default_factory=list)        # files written in the work dir
    output_masks: dict[str, list[str]] = field(default_factory=dict)
    output_comparators: dict[str, str] = field(default_factory=dict)  # default "bytes"
    stream_masks: list[str] = field(default_factory=list)
    streams: bool = True
    external: list[dict] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)
    requires: list[str] = field(default_factory=list)
    fonts: list[str] = field(default_factory=list)         # default fonts named by external inputs
    driver: str = "m1_cases.py"                            # the authoring driver that writes the case
    stream_comparator: str = "magic-console"               # comparator of stdout and stderr
    keep_checkpaint: bool = False                          # compare .mag checkpaint sections (DRC cases)
    output_names: dict[str, str] = field(default_factory=dict)  # output name per source (default: derived)
    slow: bool = False                                     # CTest label golden-slow (corpus cases)

    @property
    def area(self) -> str:
        return self.id.split("-")[0]


def _toml_str(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _toml_list(items) -> str:
    return "[" + ", ".join(_toml_str(i) if not i.startswith("regex:") else f"'{i}'" for i in items) + "]"


def _output_block(name: str, source: str, comparator: str, masks: list[str]) -> str:
    return (f"\n[[outputs]]\nname = {_toml_str(name)}\nsource = {_toml_str(source)}\n"
            f"comparator = {_toml_str(comparator)}\nmasks = {_toml_list(masks)}\n")


def _output_name(source: str) -> str:
    return source.replace("/", "__")


DEFAULT_FONTS = ("FreeSans", "FreeSerif", "FreeMono")  # magic/proto.magicrc.in


def _texts(spec: Spec) -> list[str]:
    return [c for c in spec.files.values() if isinstance(c, str)]


def font_externals(spec: Spec) -> list[dict]:
    """``magic-sys`` inputs for the default fonts the case's files (or ``spec.fonts``) name."""
    named = [f for f in DEFAULT_FONTS if f in spec.fonts or any(f in t for t in _texts(spec))]
    return [{"source": "magic-sys", "path": f"graphics/{f}.pt3", "to": f"{f}.pt3"} for f in named]


# m3-drc design D12/D17: the mask is retired now that ENGINE's marking sites are complete
# (tasks.md 6.2): m1_cases.py and m2_cases.py were re-run and the 138 cases that carried
# it replay without it.
CHECKPAINT_MASK_RETIRED = True


def masks_checkpaint(spec: Spec) -> bool:
    """Whether ``.mag`` outputs get the ``mag-checkpaint`` mask: never for a case that
    checks the DRC sections (``keep_checkpaint``, or an input carrying a checkpaint
    section, which the case checks is persisted), and not once the mask is retired."""
    if CHECKPAINT_MASK_RETIRED or spec.keep_checkpaint:
        return False
    return not any("<< checkpaint >>" in t for t in _texts(spec))


def case_toml(spec: Spec) -> str:
    lines = [HEADER, "#", f"# {spec.id} ({spec.area}/CASES.md). Inputs only: expected/ is written by",
             f"# capture.py in CI (golden-capture.yml). Generated by tools/golden/authoring/{spec.driver}.",
             *(f"# {n}" for n in spec.notes), "",
             "schema = 1", f"id = {_toml_str(spec.id)}", f"area = {_toml_str(spec.area)}",
             f"capability = {_toml_str(spec.capability)}", f"requirements = {_toml_list(spec.requirements)}",
             'tool = "magic"', f"description = {_toml_str(spec.description)}",
             *(["slow = true"] if spec.slow else []), "",
             "[reference]", 'script = "input/commands.tcl"', "",
             "[hefesto]", f"args = {_toml_list(HEFESTO_ARGS)}",
             f"requires_commands = {_toml_list(spec.requires)}"]
    text = "\n".join(lines) + "\n"
    for ext in [*spec.external, *font_externals(spec)]:
        text += "\n[[external]]\n" + "".join(f"{k} = {_toml_str(v)}\n" for k, v in ext.items())
    if spec.streams:
        # The stamp note depends on wall-clock time (Magic and Hefesto print it
        # only when a save lands in the cell's previous stamp second).
        masks = spec.stream_masks if "magic-stamp-note" in spec.stream_masks else ["magic-stamp-note", *spec.stream_masks]
        stream_masks = _with_mask(masks, "magic-tech-sections")
        text += _output_block("stdout.txt", "stdout", spec.stream_comparator, stream_masks)
        text += _output_block("stderr.txt", "stderr", spec.stream_comparator, stream_masks)
    for source in spec.outputs:
        masks = spec.output_masks.get(source, ["mag-timestamp"] if source.endswith(".mag") else [])
        if source.endswith(".mag") and masks_checkpaint(spec):
            masks = _with_mask(masks, "mag-checkpaint")
        name = spec.output_names.get(source, _output_name(source))
        text += _output_block(name, source, spec.output_comparators.get(source, "bytes"), masks)
    return text


def _with_mask(masks: list[str], name: str) -> list[str]:
    """``masks`` plus ``name`` (last) unless already listed."""
    return masks if name in masks else [*masks, name]


# hefesto-cli runs the reference script itself, in Magic's batch mode.
HEFESTO_ARGS = ["--no-log", "--diagnostics=magic", "--magic", "-dnull", "-noconsole", "-norc", "input/commands.tcl"]


def tcl_line(cmd: str, streams: bool) -> str:
    return f"catch {{{cmd}}} result; puts $result" if streams else f"catch {{{cmd}}}"


def script(spec: Spec) -> str:
    """The Tcl script of a spec, run by Magic and by hefesto-cli."""
    tcl = [f"# {spec.id}: {spec.description}", "# Reference script (Magic, Tcl): one result line per command."]
    tcl += [tcl_line(c, spec.streams) for c in spec.cmds] + ["quit -noprompt"]
    return "\n".join(tcl) + "\n"


def write(spec: Spec, golden: Path = GOLDEN) -> Path:
    root = golden / spec.area / "cases" / spec.id
    inp = root / "input"
    inp.mkdir(parents=True, exist_ok=True)
    (root / "case.toml").write_text(case_toml(spec), encoding="utf-8")
    (inp / "commands.tcl").write_text(script(spec), encoding="utf-8")
    (inp / "commands.hef").unlink(missing_ok=True)  # the M1-M3 hefesto-cli script (13.2)
    for name, content in spec.files.items():
        target = inp / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            target.write_bytes(content)
        else:
            target.write_text(content, encoding="utf-8", newline="")
    return root
