#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Inventory of what the PDK xschem libraries need from xschem (m8-schematic design D18).

Reads only the three xschem data sets of scripts/fetch_pdk.py (sky130-xschem, gf180mcu-xschem,
ihp-xschem; Apache-2.0 PDK data) and writes tests/data/schem/inventory.json. Per PDK it lists, sorted:

``devices``                 symbols of xschem's system library the PDK files reference: every
                            ``devices/<name>`` reference, and every bare reference (no ``/``) that is not
                            found in the PDK's own library directories (xschem searches ``devices`` first)
``unresolved``              references with a directory part that resolve nowhere in the PDK
``generators``              Tcl generator scripts referenced as ``<script>.tcl(<arguments>)`` (Q-SCHEM-11)
``tcleval_commands``        Tcl commands called inside ``tcleval(`` values and ``@tcleval(`` tokens
``tcleval_variables``       ``$::`` variables read there
``rc_commands``             Tcl commands of the ``xschemrc`` files and of the PDK files they ``source``
``rc_variables``            ``$::`` variables read there
``pdk_procs``               procedures those files define (provided by the PDK, not by Hefesto)
``xschem_subcommands``      ``xschem <subcommand>`` calls in tcleval values, launcher ``tclcommand``
                            attributes, other attribute values and the rc files
``multiline_templates``     subcircuit symbols whose ``template`` value contains a line feed (Q-SCHEM-21)

The files are read with the record and property grammars of capabilities ``schematic-file-format``
(Requirements "Record grammar", "Brace strings", "Property strings") and the Tcl word rules; Tcl is never
run. Bodies are scanned as Tcl text, so a command name built at run time (``[$cmd]``) is not listed.

Usage:
    scripts/schem_inventory.py [--root DIR] [--output FILE]   write the inventory
    scripts/schem_inventory.py --check [--root DIR]           exit 1 when it differs from the committed file
Exit status: 0 ok, 1 difference (--check), 2 a data set is missing or does not verify.
"""
from __future__ import annotations

import argparse
import difflib
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fetch_pdk  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = REPO / "tests" / "data" / "schem" / "inventory.json"


@dataclass(frozen=True)
class PdkTrees:
    """One PDK's xschem data set and its library directories (relative to the variant directory)."""

    pdk: str
    set_name: str
    # The PDK directories of the xschemrc's XSCHEM_LIBRARY_PATH, in order; the xschem system library
    # (devices, xschem_library) and the user directories precede or follow them and are not PDK data.
    search_dirs: tuple[str, ...]


PDKS = (
    PdkTrees("sky130", "sky130-xschem", ("libs.tech/xschem",)),
    PdkTrees("gf180mcu", "gf180mcu-xschem", ("libs.tech/xschem/tests", "libs.tech/xschem")),
    PdkTrees("ihp", "ihp-xschem", ("libs.tech/xschem", "libs.tech/xschem/sg13g2_pr",
                                   "libs.ref/sg13g2_stdcell/sym/xschem")),
)


# --- schematic-file-format: records, brace strings, property strings -------------------------------


def read_brace(text: str, i: int) -> tuple[str, int]:
    """The brace string starting at ``text[i] == '{'`` (decoded) and the index after its ``}``."""
    out, i = [], i + 1
    while i < len(text):
        ch = text[i]
        if ch == "\\" and i + 1 < len(text):
            out.append(text[i + 1])
            i += 2
            continue
        if ch == "}":
            return "".join(out).replace("\r", ""), i + 1
        out.append(ch)
        i += 1
    return "".join(out).replace("\r", ""), i  # end of file inside the string: malformed, keep what was read


def records(text: str) -> list[tuple[str, list[str]]]:
    """Each record as (tag, brace-string fields), in file order; numeric fields are dropped."""
    result, i, n = [], 0, len(text)
    while i < n:
        while i < n and text[i] in " \t\r\n":
            i += 1
        if i >= n:
            break
        tag, fields = text[i], []
        i += 1
        while i < n and text[i] != "\n":
            if text[i] == "{" and tag != "#":
                value, i = read_brace(text, i)
                fields.append(value)
            else:
                i += 1
        result.append((tag, fields))
    return result


SEPARATORS = " \t\n;"


def properties(props: str) -> list[tuple[str, str]]:
    """The (key, decoded value) attributes of a property string, in order."""
    attrs, i, n = [], skip_separators(props, 0), len(props)
    while i < n:
        key, i = read_token(props, i, stop_at_equal=True)
        j = skip_separators(props, i)
        value = ""
        if j < n and props[j] == "=":
            value, i = read_token(props, skip_separators(props, j + 1), stop_at_equal=False)
        attrs.append((key, value))
        i = skip_separators(props, i)
    return attrs


def skip_separators(props: str, i: int) -> int:
    while i < len(props) and props[i] in SEPARATORS:
        i += 1
    return i


def read_token(props: str, i: int, stop_at_equal: bool) -> tuple[str, int]:
    """A key or value from ``i``: quotes toggle quoted mode, a backslash escapes; returns it decoded."""
    out, quoted, n = [], False, len(props)
    while i < n:
        ch = props[i]
        if ch == "\\" and i + 1 < n:
            out.append(props[i + 1])
            i += 2
            continue
        if ch == '"':
            quoted = not quoted
        elif not quoted and (ch in SEPARATORS or (stop_at_equal and ch == "=")):
            break
        else:
            out.append(ch)
        i += 1
    return "".join(out), i


# --- Tcl words (the Tcl syntax rules; nothing is evaluated) ----------------------------------------


@dataclass
class Word:
    text: str
    braced: bool = False
    scripts: list[list[list[Word]]] = field(default_factory=list)  # parsed [..] substitutions


class TclScanner:
    """Splits Tcl text into commands of words, parsing ``[...]`` command substitutions."""

    def __init__(self, text: str):
        self.text, self.n = text, len(text)

    def script(self, i: int, close: str | None = None) -> tuple[list[list[Word]], int]:
        """The commands from ``i`` up to ``close`` (``]``) or the end; returns them and the index after."""
        commands = []
        while i < self.n:
            i = self.skip_separators(i, close)
            if i >= self.n or (close and self.text[i] == close):
                return commands, i + 1
            if self.text[i] == "#":
                i = self.skip_comment(i)
                continue
            command, i = self.command(i, close)
            if command:
                commands.append(command)
        return commands, i

    def skip_separators(self, i: int, close: str | None) -> int:
        while i < self.n and (self.text[i] in " \t\r\n;" or self.text.startswith("\\\n", i)):
            i += 2 if self.text[i] == "\\" else 1
        return i

    def skip_comment(self, i: int) -> int:
        while i < self.n and self.text[i] != "\n":
            i += 2 if self.text[i] == "\\" else 1
        return i

    def command(self, i: int, close: str | None) -> tuple[list[Word], int]:
        words = []
        while i < self.n:
            while i < self.n and (self.text[i] in " \t" or self.text.startswith("\\\n", i)):
                i += 2 if self.text[i] == "\\" else 1
            if i >= self.n or self.text[i] in "\r\n;" or (close and self.text[i] == close):
                break
            word, i = self.word(i, close)
            words.append(word)
        return words, i

    def word(self, i: int, close: str | None) -> tuple[Word, int]:
        if self.text.startswith("{*}", i) and i + 3 < self.n and self.text[i + 3] not in " \t\r\n;":
            i += 3  # argument expansion: the word that follows is the word
        if self.text[i] == "{":
            end = self.brace_end(i)
            return Word(self.text[i + 1:end - 1], braced=True), end
        quoted = self.text[i] == '"'
        return self.substituted_word(i + 1 if quoted else i, quoted, close)

    def brace_end(self, i: int) -> int:
        depth = 0
        while i < self.n:
            ch = self.text[i]
            if ch == "\\":
                i += 2
                continue
            depth += {"{": 1, "}": -1}.get(ch, 0)
            i += 1
            if depth == 0:
                return i
        return self.n

    def substituted_word(self, i: int, quoted: bool, close: str | None) -> tuple[Word, int]:
        start, scripts = i, []
        while i < self.n:
            ch = self.text[i]
            if ch == "\\":
                i += 2
                continue
            if ch == "[":
                nested, i = self.script(i + 1, "]")
                scripts.append(nested)
                continue
            if self.word_ends(ch, quoted, close):
                break
            i += 1
        end = i
        return Word(self.text[start:end], scripts=scripts), i + 1 if quoted else i

    @staticmethod
    def word_ends(ch: str, quoted: bool, close: str | None) -> bool:
        if quoted:
            return ch == '"'
        return ch in " \t\r\n;" or (close is not None and ch == close)


def subst_scripts(text: str) -> list[list[list[Word]]]:
    """The command substitutions of a ``subst`` body (a tcleval value): every unescaped ``[...]``."""
    scanner, found, i = TclScanner(text), [], 0
    while i < len(text):
        if text[i] == "\\":
            i += 2
        elif text[i] == "[":
            nested, i = scanner.script(i + 1, "]")
            found.append(nested)
        else:
            i += 1
    return found


def body_indexes(name: str, args: list[Word]) -> list[int]:
    """Indexes in ``args`` of the words that a control command evaluates as scripts."""
    count = len(args)
    if name == "if":
        return if_bodies(args)
    simple = {"while": [1], "for": [0, 2, 3], "foreach": [count - 1], "proc": [2], "catch": [0],
              "time": [0], "uplevel": [count - 1], "after": [count - 1] if count > 1 else [], "bind": [count - 1],
              "eval": [0] if count == 1 else []}
    if name == "namespace" and count >= 3 and args[0].text == "eval":
        return [count - 1]
    indexes = simple.get(name, [])
    indexes += [k + 1 for k, word in enumerate(args[:-1]) if word.text == "-command"]
    return [k for k in indexes if 0 <= k < count]


def expression_indexes(name: str, args: list[Word]) -> list[int]:
    """Indexes in ``args`` of the words that a command evaluates as expressions."""
    if name == "if":
        return [0] + [k + 1 for k, word in enumerate(args[:-1]) if word.text == "elseif"]
    return {"while": [0], "for": [1], "expr": list(range(len(args)))}.get(name, [])


def if_bodies(args: list[Word]) -> list[int]:
    bodies, k = [], 1  # args[0] is the first condition
    while k < len(args):
        if args[k].text == "then":
            k += 1
        bodies.append(k)
        k += 1
        if k < len(args) and args[k].text == "elseif":
            k += 2
        elif k < len(args) and args[k].text == "else":
            bodies.append(k + 1)
            break
        else:
            break
    return bodies


IDENTIFIER = re.compile(r"^(::)?[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_][A-Za-z0-9_]*)*$")
GLOBAL_VARIABLE = re.compile(r"\$(?:\{::([A-Za-z_][A-Za-z0-9_:]*)|::([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*))")


@dataclass
class TclUse:
    """Commands, ``$::`` variables, xschem subcommands and procedures found in Tcl text."""

    commands: set[str] = field(default_factory=set)
    variables: set[str] = field(default_factory=set)
    subcommands: set[str] = field(default_factory=set)
    procs: set[str] = field(default_factory=set)

    def add_script(self, commands: list[list[Word]]) -> None:
        for words in commands:
            self.add_command(words)

    def add_command(self, words: list[Word]) -> None:
        name = words[0].text
        if not words[0].braced and IDENTIFIER.match(name):
            self.commands.add(name.removeprefix("::"))
            self.note_special(name.removeprefix("::"), words[1:])
        script_args = set(body_indexes(name, words[1:]))
        expression_args = set(expression_indexes(name, words[1:]))
        for k, word in enumerate(words[1:]):
            for nested in word.scripts:
                self.add_script(nested)
            if k in script_args:
                self.add_script(TclScanner(word.text).script(0)[0])
            elif word.braced and k in expression_args:
                for nested in subst_scripts(word.text):  # a braced expression still substitutes commands
                    self.add_script(nested)
        for nested in words[0].scripts:
            self.add_script(nested)

    def note_special(self, name: str, args: list[Word]) -> None:
        if name == "xschem" and args and IDENTIFIER.match(args[0].text):
            self.subcommands.add(args[0].text)
        if name == "proc" and args and IDENTIFIER.match(args[0].text):
            self.procs.add(args[0].text.removeprefix("::"))

    def add_variables(self, text: str) -> None:
        for braced, plain in GLOBAL_VARIABLE.findall(text):
            self.variables.add((braced or plain).split("(")[0])


def tcleval_bodies(value: str) -> list[str]:
    """The Tcl bodies of a decoded value: the whole value when it starts with ``tcleval(`` (up to its last
    ``)``), and every ``@tcleval(...)`` token (up to the matching parenthesis)."""
    bodies = []
    stripped = value.lstrip()
    if stripped.startswith("tcleval("):
        bodies.append(stripped[len("tcleval("):stripped.rfind(")")].replace("\\}", "}"))
    for match in re.finditer(r"@tcleval\(", value):
        start = depth = match.end()
        depth = 1
        k = start
        while k < len(value) and depth:
            depth += {"(": 1, ")": -1}.get(value[k], 0)
            k += 1
        bodies.append(value[start:k - 1])
    return bodies


# --- one PDK ----------------------------------------------------------------------------------------


@dataclass
class PdkInventory:
    devices: set[str] = field(default_factory=set)
    unresolved: set[str] = field(default_factory=set)
    generators: set[str] = field(default_factory=set)
    tcleval: TclUse = field(default_factory=TclUse)
    rc: TclUse = field(default_factory=TclUse)
    attribute_subcommands: set[str] = field(default_factory=set)
    multiline_templates: set[str] = field(default_factory=set)

    def as_json(self) -> dict:
        return {
            "devices": sorted(self.devices),
            "unresolved": sorted(self.unresolved),
            "generators": sorted(self.generators),
            "tcleval_commands": sorted(self.tcleval.commands),
            "tcleval_variables": sorted(self.tcleval.variables),
            "rc_commands": sorted(self.rc.commands),
            "rc_variables": sorted(self.rc.variables),
            "pdk_procs": sorted(self.rc.procs),
            "xschem_subcommands": sorted(self.tcleval.subcommands | self.rc.subcommands
                                         | self.attribute_subcommands),
            "multiline_templates": sorted(self.multiline_templates),
        }


def resolve(reference: str, variant_dir: Path, trees: PdkTrees) -> Path | None:
    name = with_suffix(reference)
    for sub in trees.search_dirs:
        candidate = variant_dir / sub / name
        if candidate.is_file():
            return candidate
    return None


def with_suffix(reference: str) -> str:
    """A reference without an extension names a ``.sym`` file."""
    return reference if Path(reference).suffix else reference + ".sym"


def scan_reference(reference: str, variant_dir: Path, trees: PdkTrees, inv: PdkInventory) -> None:
    if "(" in reference:  # a Tcl generator call, `<script>.tcl(<arguments>)` (Q-SCHEM-11)
        script = reference.split("(", 1)[0]
        target = inv.generators if resolve(script, variant_dir, trees) else inv.unresolved
        target.add(script)
    elif reference.startswith("devices/"):
        inv.devices.add(with_suffix(reference.removeprefix("devices/")))
    elif resolve(reference, variant_dir, trees) is None:
        (inv.unresolved if "/" in reference else inv.devices).add(with_suffix(reference))


def scan_tcl_text(body: str, inv: PdkInventory) -> None:
    """A ``subst`` body: its command substitutions and ``$::`` variables."""
    for nested in subst_scripts(body):
        inv.tcleval.add_script(nested)
    inv.tcleval.add_variables(body)


def scan_value(value: str, inv: PdkInventory) -> None:
    for body in tcleval_bodies(value):
        scan_tcl_text(body, inv)


def scan_attributes(props: str, inv: PdkInventory) -> None:
    for key, value in properties(props):
        scan_value(value, inv)
        if key == "tclcommand" or re.search(r"(^|[\[;{\s])xschem\s", value):
            use = TclUse()
            use.add_script(TclScanner(value).script(0)[0])
            for nested in subst_scripts(value):
                use.add_script(nested)
            inv.attribute_subcommands |= use.subcommands


TOKEN = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)")


def first_values(props: str) -> dict[str, str]:
    """Attribute lookup: the first occurrence of a key wins."""
    return dict(reversed(properties(props)))


def tcl_tokens(attrs: dict[str, str]) -> set[str]:
    """The ``@attribute`` tokens inside the tcleval bodies of these values: their values become Tcl text."""
    return {name for value in attrs.values() for body in tcleval_bodies(value) for name in TOKEN.findall(body)}


@dataclass
class SchematicScanner:
    """Scans the ``.sym`` files first, so that an instance knows which of its attributes its symbol
    substitutes into a ``tcleval`` body (``format="tcleval( @value )"`` and the like)."""

    variant_dir: Path
    trees: PdkTrees
    inv: PdkInventory
    symbol_tcl: dict[Path, tuple[set[str], dict[str, str]]] = field(default_factory=dict)

    def scan(self, paths: list[Path]) -> None:
        for path in sorted(paths, key=lambda p: (p.suffix != ".sym", p)):
            self.scan_file(path)

    def scan_file(self, path: Path) -> None:
        for tag, fields in records(path.read_text(encoding="utf-8", errors="replace")):
            if tag == "C" and len(fields) > 1:
                self.scan_instance(fields[0], fields[1])
            if tag == "T" and fields:
                scan_value(fields[0], self.inv)
            for props in fields[1:] if tag in "CT" else fields:
                scan_attributes(props, self.inv)
            if path.suffix == ".sym" and tag in "KG" and fields:
                self.note_symbol(path, fields[0])

    def note_symbol(self, path: Path, props: str) -> None:
        attrs = first_values(props)
        if attrs.get("type") == "subcircuit" and "\n" in attrs.get("template", ""):
            self.inv.multiline_templates.add(path.relative_to(self.variant_dir).as_posix())
        tokens = tcl_tokens(attrs)
        if tokens:
            self.symbol_tcl[path] = (tokens, first_values(attrs.get("template", "")))

    def scan_instance(self, reference: str, props: str) -> None:
        scan_reference(reference, self.variant_dir, self.trees, self.inv)
        attrs = first_values(props)
        tokens, defaults = self.symbol_tcl.get(resolve(reference, self.variant_dir, self.trees), (set(), {}))
        for name in sorted(tokens | tcl_tokens(attrs)):
            scan_tcl_text(attrs.get(name, defaults.get(name, "")), self.inv)


def rc_files(variant_dir: Path, subset_dirs: list[Path]) -> list[Path]:
    """Every ``xschemrc`` of the set, then each PDK file they ``source`` (found under ``libs.tech/xschem``)."""
    rcs = sorted(p for top in subset_dirs for p in top.rglob("xschemrc") if p.is_file())
    sourced = []
    for rc in rcs:
        for match in re.finditer(r"^\s*source\s+\S*libs\.tech/xschem/(\S+)\s*$", rc.read_text(errors="replace"), re.M):
            candidate = variant_dir / "libs.tech" / "xschem" / match.group(1)
            if candidate.is_file() and candidate not in sourced:
                sourced.append(candidate)
    return rcs + sorted(sourced)


def scan_pdk(trees: PdkTrees, root: Path) -> dict:
    data = fetch_pdk.DATA_SETS[trees.set_name]
    version_dir = data.version_dir(root)
    variant_dir = version_dir / data.variant
    subset_dirs = [version_dir / sub for sub in data.subset]
    inv = PdkInventory()
    files = [p for top in subset_dirs for p in top.rglob("*") if p.suffix in (".sch", ".sym") and p.is_file()]
    SchematicScanner(variant_dir, trees, inv).scan(files)
    for path in rc_files(variant_dir, subset_dirs):
        text = path.read_text(encoding="utf-8", errors="replace")
        inv.rc.add_script(TclScanner(text).script(0)[0])
        inv.rc.add_variables(text)
    return inv.as_json()


def build(root: Path, pdks: tuple[PdkTrees, ...] = PDKS) -> dict:
    result = {"data_sets": {}, "pdks": {}}
    for trees in pdks:
        data = fetch_pdk.DATA_SETS[trees.set_name]
        result["data_sets"][trees.set_name] = {"version": data.version, "tree_sha256": data.tree_sha256}
        result["pdks"][trees.pdk] = scan_pdk(trees, root)
    return result


def render(inventory: dict) -> str:
    return json.dumps(inventory, indent=1, sort_keys=True) + "\n"


def missing_sets(root: Path, pdks: tuple[PdkTrees, ...] = PDKS) -> list[str]:
    """The data sets that are absent or whose digest differs from the pin."""
    return [t.set_name for t in pdks
            if fetch_pdk.set_digest(fetch_pdk.DATA_SETS[t.set_name], root) != fetch_pdk.DATA_SETS[t.set_name].tree_sha256]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--root", type=Path, default=REPO / ".cache" / "pdks", help="PDK root of fetch_pdk.py")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="inventory file")
    parser.add_argument("--check", action="store_true", help="compare with --output instead of writing it")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    missing = missing_sets(root)
    if missing:
        print(f"schem_inventory: data sets missing or not verified: {', '.join(missing)} "
              f"(run scripts/fetch_pdk.py {' '.join(missing)})", file=sys.stderr)
        return 2
    text = render(build(root))
    if not args.check:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
        return 0
    committed = args.output.read_text() if args.output.is_file() else ""
    if committed == text:
        print(f"schem_inventory: {args.output} is up to date")
        return 0
    sys.stdout.writelines(difflib.unified_diff(committed.splitlines(True), text.splitlines(True),
                                               str(args.output), "inventory of the data sets"))
    print(f"schem_inventory: {args.output} differs from the data sets; rerun scripts/schem_inventory.py",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
