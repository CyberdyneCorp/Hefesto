# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonical SPICE comparator (openspec/changes/m8-schematic design D10, Q-SCHEM-15).

Capability ``schematic-netlisting``, Requirement "Flat netlist": a flat netlist is not compared by
bytes. Both netlists are parsed into devices and compared electrically. This module is Hefesto code
written from SPICE semantics only.

Parsing (case-insensitive, as SPICE reads a netlist):

- CR is dropped; a line starting with ``+`` continues the previous line; empty lines and lines starting
  with ``*`` are skipped; ``.end`` ends the netlist; a ``.control`` ... ``.endc`` block is skipped.
- A line is split into fields at whitespace outside ``{...}``, ``'...'`` and ``"..."``; ``k = v`` with
  spaces around ``=`` is one field ``k=v``.
- ``.subckt <name> <ports...> [params:] [k=v...]`` ... ``.ends`` defines a subcircuit; ``.global <nets>``
  declares global nets. Every other dot line (``.include``, ``.lib``, ``.param``, ``.option``,
  ``.model``, ...) is ignored: only devices are compared.
- A device line is ``<name> <positional fields...> <k=v fields...>``. Its first letter is its kind, and
  its nodes are its first positional fields: 2 for R, C, L, D, V, I, B, F and H; 3 for J; 3 or 4 for Q
  (4 when it has 5 or more positional fields); 4 for M, E and G; for X and any other kind every
  positional field but the last. The remaining positional fields (model, value, source names) and the
  ``k=v`` parameters are the device's arguments. A value is compared as a number when it reads as one
  (SPICE scale suffixes T G MEG K M MIL U N P F A, trailing unit letters ignored, compared to 12
  significant digits), else as lower-case text.

Expansion: an X device whose last positional field names a subcircuit defined in the netlist is replaced
by the subcircuit's devices, recursively: ports map to the instance's nodes, other nodes and device names
are prefixed ``<instance>.``, global nets and ``0`` keep their names, and a value that is exactly a
subcircuit parameter (``p``, ``{p}`` or ``'p'``) takes the instance's value, else the default. X devices
of undefined subcircuits (PDK models) stay devices. When the netlist has no top-level device and exactly
one subcircuit that no device instantiates (an LVS netlist, whose top is a ``.subckt``), that
subcircuit is the top and its ports keep their names.

Comparison: ``0``, the global nets and the top subcircuit's ports are fixed names; every other node is
matched by connectivity. Two netlists are equal when a bijection of the other nodes maps one device
multiset onto the other: same kinds, same arguments, same ordered nodes after mapping. With
``names=True`` (the default) device names must also be equal (case-insensitively) and the mapping is
forced device by device; with ``names=False`` devices are matched by colour refinement of the
device-net graph and a backtracking search, so device names are free. The first difference is reported.

Usage: ``python3 -m compare.spice_canonical a.spice b.spice [--no-names]``.
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from dataclasses import dataclass, field

MAX_DEPTH = 64
_NUMBER = re.compile(r"^([+-]?(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?)(meg|mil|[tgkmunpfa])?[a-z]*$")
_SCALE = {None: 1.0, "t": 1e12, "g": 1e9, "meg": 1e6, "k": 1e3, "mil": 25.4e-6, "m": 1e-3, "u": 1e-6,
          "n": 1e-9, "p": 1e-12, "f": 1e-15, "a": 1e-18}
_TWO_NODES = set("rcldvibfh")
_FOUR_NODES = set("meg")
_OPEN, _CLOSE = "{'\"", "}'\""


class SpiceError(ValueError):
    """The netlist cannot be parsed or expanded."""


@dataclass(frozen=True)
class Device:
    name: str                      # lower case, hierarchical names joined with '.'
    kind: str                      # first letter of the name
    nodes: tuple[str, ...]
    args: tuple[str, ...]          # normalized positional arguments, then sorted k=v parameters

    def signature(self) -> tuple:
        return (self.kind, self.args, len(self.nodes))

    def describe(self) -> str:
        return f"{self.name} {' '.join(self.nodes)} {' '.join(self.args)}".strip()


@dataclass
class Subckt:
    name: str
    ports: tuple[str, ...]
    params: dict[str, str]
    lines: list[list[str]] = field(default_factory=list)


@dataclass
class Netlist:
    top: list[list[str]] = field(default_factory=list)
    subckts: dict[str, Subckt] = field(default_factory=dict)
    globals: set[str] = field(default_factory=lambda: {"0"})


def normalize_value(value: str) -> str:
    """A field value as compared: a number to 12 significant digits, else lower-case text."""
    low = value.lower()
    match = _NUMBER.match(low)
    if match:
        return format(float(match.group(1)) * _SCALE[match.group(2)], ".12g")
    return low


def split_fields(line: str) -> list[str]:
    """Fields at whitespace outside braces and quotes, with ``k = v`` joined into ``k=v``."""
    fields, current, closer = [], "", None
    for char in line:
        if closer is not None:
            current += char
            if char == closer:
                closer = None
        elif char in _OPEN:
            current += char
            closer = _CLOSE[_OPEN.index(char)]
        elif char.isspace():
            if current:
                fields.append(current)
            current = ""
        else:
            current += char
    if current:
        fields.append(current)
    return _join_equals(fields)


def _join_equals(fields: list[str]) -> list[str]:
    out: list[str] = []
    for item in fields:
        if out and (item.startswith("=") or out[-1].endswith("=")):
            out[-1] += item
        else:
            out.append(item)
    return out


def logical_lines(text: str) -> list[list[str]]:
    """The netlist's lines as field lists: continuations joined, comments and control blocks dropped."""
    joined: list[str] = []
    for raw in text.replace("\r", "").split("\n"):
        stripped = raw.strip()
        if stripped.startswith("+"):
            if joined:
                joined[-1] += " " + stripped[1:]
            continue
        joined.append(stripped)
    lines, in_control = [], False
    for line in joined:
        if not line or line.startswith("*"):
            continue
        word = line.split()[0].lower()
        if word == ".control":
            in_control = True
        elif word == ".endc":
            in_control = False
        elif word == ".end":
            break
        elif not in_control:
            lines.append(split_fields(line))
    return lines


def parse(text: str) -> Netlist:
    netlist, current = Netlist(), None
    for fields in logical_lines(text):
        word = fields[0].lower()
        if word == ".subckt":
            if current is not None:
                raise SpiceError(f"nested .subckt {fields[1:2]}")
            current = _subckt_header(fields)
        elif word == ".ends":
            if current is None:
                raise SpiceError(".ends without .subckt")
            netlist.subckts[current.name] = current
            current = None
        elif word == ".global":
            netlist.globals.update(f.lower() for f in fields[1:])
        elif word.startswith("."):
            continue
        elif current is not None:
            current.lines.append(fields)
        else:
            netlist.top.append(fields)
    if current is not None:
        raise SpiceError(f".subckt {current.name} has no .ends")
    return netlist


def _subckt_header(fields: list[str]) -> Subckt:
    if len(fields) < 2:
        raise SpiceError(".subckt without a name")
    ports, params = [], {}
    for item in fields[2:]:
        if item.lower() == "params:":
            continue
        if "=" in item:
            key, value = item.split("=", 1)
            params[key.lower()] = value
        else:
            ports.append(item.lower())
    return Subckt(fields[1].lower(), tuple(ports), params)


def _node_count(kind: str, positional: list[str]) -> int:
    if kind in _TWO_NODES:
        return 2
    if kind in _FOUR_NODES:
        return 4
    if kind == "j":
        return 3
    if kind == "q":
        return 4 if len(positional) >= 5 else 3
    return max(len(positional) - 1, 0)


def _split_device(fields: list[str]) -> tuple[str, list[str], list[str], dict[str, str]]:
    name = fields[0].lower()
    positional = [f for f in fields[1:] if "=" not in f]
    params = {}
    for item in fields[1:]:
        if "=" in item:
            key, value = item.split("=", 1)
            params[key.lower()] = value
    count = min(_node_count(name[0], positional), len(positional))
    return name, [p.lower() for p in positional[:count]], positional[count:], params


def _substitute(value: str, params: dict[str, str]) -> str:
    bare = value.strip("{}'").lower()
    if bare in params and value.lower() in (bare, "{" + bare + "}", "'" + bare + "'"):
        return params[bare]
    return value


def _make_device(name: str, nodes: list[str], rest: list[str], params: dict[str, str]) -> Device:
    args = tuple(normalize_value(v) for v in rest)
    args += tuple(f"{k}={normalize_value(v)}" for k, v in sorted(params.items()))
    return Device(name, name.rsplit(".", 1)[-1][:1], tuple(nodes), args)


def _expand(lines, netlist: Netlist, prefix: str, node_map: dict[str, str], values: dict[str, str],
            depth: int, out: list[Device]) -> None:
    if depth > MAX_DEPTH:
        raise SpiceError(f"subcircuit nesting deeper than {MAX_DEPTH} at {prefix!r}")

    def node(n: str) -> str:
        if n in netlist.globals:
            return n
        return node_map.get(n, prefix + n)

    for fields in lines:
        name, nodes, rest, params = _split_device(fields)
        rest = [_substitute(v, values) for v in rest]
        params = {k: _substitute(v, values) for k, v in params.items()}
        mapped = [node(n) for n in nodes]
        sub = netlist.subckts.get(rest[-1].lower()) if name.startswith("x") and rest else None
        if sub is None:
            out.append(_make_device(prefix + name, mapped, rest, params))
            continue
        if len(sub.ports) != len(mapped):
            raise SpiceError(f"{prefix + name}: {len(mapped)} nodes for .subckt {sub.name} with {len(sub.ports)} ports")
        inner_values = {**{k: _substitute(v, values) for k, v in sub.params.items()}, **params}
        _expand(sub.lines, netlist, f"{prefix}{name}.", dict(zip(sub.ports, mapped)), inner_values, depth + 1, out)


def devices(text: str) -> tuple[list[Device], set[str]]:
    """The expanded device list of a netlist and its fixed node names (``0``, globals, top ports)."""
    netlist = parse(text)
    top, fixed = netlist.top, set(netlist.globals)
    if not top:
        used = {_split_device(f)[2][-1].lower() for s in netlist.subckts.values() for f in s.lines
                if f[0].lower().startswith("x") and _split_device(f)[2]}
        roots = [s for s in netlist.subckts.values() if s.name not in used]
        if len(roots) == 1:
            top = roots[0].lines
            fixed |= set(roots[0].ports)
            values = dict(roots[0].params)
            out: list[Device] = []
            _expand(top, netlist, "", {p: p for p in roots[0].ports}, values, 0, out)
            return out, fixed
    out = []
    _expand(top, netlist, "", {}, {}, 0, out)
    return out, fixed


# --- comparison ------------------------------------------------------------------------------------

def compare(expected: str, actual: str, names: bool = True) -> str | None:
    """None when the netlists are electrically equal, else the first difference."""
    try:
        exp_devices, exp_fixed = devices(expected)
        act_devices, act_fixed = devices(actual)
    except SpiceError as error:
        return f"cannot parse: {error}"
    difference = _signature_difference(exp_devices, act_devices)
    if difference:
        return difference
    if names:
        return _compare_by_name(exp_devices, act_devices, exp_fixed | act_fixed)
    return _compare_by_structure(exp_devices, act_devices, exp_fixed | act_fixed)


def _signature_difference(exp: list[Device], act: list[Device]) -> str | None:
    exp_count, act_count = Counter(d.signature() for d in exp), Counter(d.signature() for d in act)
    if exp_count == act_count:
        return None
    for device in exp:
        if exp_count[device.signature()] > act_count[device.signature()]:
            return f"expected device not found: {device.describe()}"
    for device in act:
        if act_count[device.signature()] > exp_count[device.signature()]:
            return f"unexpected device: {device.describe()}"
    return "device multisets differ"


class _Mapping:
    """A node bijection that keeps fixed names fixed."""

    def __init__(self, fixed: set[str]) -> None:
        self.fixed, self.forward, self.backward = fixed, {}, {}

    def bind(self, a: str, b: str) -> bool:
        if (a in self.fixed or b in self.fixed) and a != b:
            return False
        if self.forward.get(a, b) != b or self.backward.get(b, a) != a:
            return False
        self.forward[a], self.backward[b] = b, a
        return True


def _compare_by_name(exp: list[Device], act: list[Device], fixed: set[str]) -> str | None:
    by_name = {d.name: d for d in act}
    if len(by_name) != len(act) or len({d.name for d in exp}) != len(exp):
        return "duplicate device names"
    mapping = _Mapping(fixed)
    for device in exp:
        other = by_name.get(device.name)
        if other is None:
            return f"expected device not found: {device.describe()}"
        if other.signature() != device.signature():
            return f"{device.name}: expected {device.describe()!r}, got {other.describe()!r}"
        for pin, (a, b) in enumerate(zip(device.nodes, other.nodes)):
            if not mapping.bind(a, b):
                return (f"{device.name} pin {pin + 1}: node {a!r} cannot map to {b!r} "
                        f"(already {mapping.forward.get(a, mapping.backward.get(b))!r})")
    return None


# Structural matching: colour refinement on the bipartite device-net graph, then backtracking.

class _Graph:
    def __init__(self, devs: list[Device], fixed: set[str]) -> None:
        self.devices = devs
        self.nodes = sorted({n for d in devs for n in d.nodes})
        self.index = {n: i for i, n in enumerate(self.nodes)}
        self.pins: list[list[tuple[int, int]]] = [[] for _ in self.nodes]  # node -> (device, pin)
        for di, dev in enumerate(devs):
            for pin, n in enumerate(dev.nodes):
                self.pins[self.index[n]].append((di, pin))
        self.initial = [("fixed", n) if n in fixed else ("net",) for n in self.nodes]


def _refine(graphs: tuple[_Graph, _Graph], colours: tuple[list, list]) -> tuple[list, list]:
    """Colour refinement of both graphs with one shared colour table."""
    current = colours
    while True:
        table: dict = {}
        new = []
        for graph, node_colours in zip(graphs, current):
            dev_colours = [(d.signature(), tuple(node_colours[graph.index[n]] for n in d.nodes))
                           for d in graph.devices]
            keys = [(node_colours[i], tuple(sorted((dev_colours[di], pin) for di, pin in graph.pins[i])))
                    for i in range(len(graph.nodes))]
            new.append(keys)
        for key in sorted({k for keys in new for k in keys}, key=repr):
            table[key] = len(table)
        refined = tuple([table[k] for k in keys] for keys in new)
        if all(len(set(r)) == len(set(c)) for r, c in zip(refined, current)):
            return refined
        current = refined


def _search(graphs: tuple[_Graph, _Graph], colours: tuple[list, list], fixed: set[str]) -> dict | None:
    exp_colours, act_colours = _refine(graphs, colours)
    if Counter(exp_colours) != Counter(act_colours):
        return None
    counts = Counter(exp_colours)
    open_classes = [c for c, k in counts.items() if k > 1]
    if not open_classes:
        mapping = {graphs[0].nodes[i]: graphs[1].nodes[act_colours.index(c)] for i, c in enumerate(exp_colours)}
        return mapping if _verify(graphs, mapping, fixed) else None
    target = min(open_classes, key=lambda c: (counts[c], c))
    first = exp_colours.index(target)
    unique = max(max(exp_colours), max(act_colours)) + 1
    for j, colour in enumerate(act_colours):
        if colour != target:
            continue
        exp_try, act_try = list(exp_colours), list(act_colours)
        exp_try[first], act_try[j] = unique, unique
        found = _search(graphs, ([(c,) for c in exp_try], [(c,) for c in act_try]), fixed)
        if found is not None:
            return found
    return None


def _verify(graphs: tuple[_Graph, _Graph], mapping: dict, fixed: set[str]) -> bool:
    if any(a != b and (a in fixed or b in fixed) for a, b in mapping.items()):
        return False
    exp = Counter((d.signature(), tuple(mapping[n] for n in d.nodes)) for d in graphs[0].devices)
    act = Counter((d.signature(), d.nodes) for d in graphs[1].devices)
    return exp == act


def _compare_by_structure(exp: list[Device], act: list[Device], fixed: set[str]) -> str | None:
    graphs = (_Graph(exp, fixed), _Graph(act, fixed))
    if len(graphs[0].nodes) != len(graphs[1].nodes):
        return f"node count differs: expected {len(graphs[0].nodes)}, got {len(graphs[1].nodes)}"
    sys.setrecursionlimit(max(sys.getrecursionlimit(), 10 * len(graphs[0].nodes) + 1000))
    if _search(graphs, (graphs[0].initial, graphs[1].initial), fixed) is None:
        return "no node mapping makes the device connections equal"
    return None


def canonical_text(data: bytes) -> bytes:
    """A readable listing of the expanded devices, sorted (the comparator's display; not the verdict)."""
    try:
        devs, _ = devices(data.decode("utf-8", errors="replace"))
    except SpiceError as error:
        return f"<cannot parse: {error}>\n".encode()
    return "".join(sorted(d.describe() + "\n" for d in devs)).encode()


def main(argv: list[str]) -> int:
    paths = [a for a in argv if not a.startswith("--")]
    if len(paths) != 2:
        print("usage: python3 -m compare.spice_canonical A B [--no-names]", file=sys.stderr)
        return 2
    texts = [open(p, encoding="utf-8", errors="replace").read() for p in paths]
    difference = compare(texts[0], texts[1], names="--no-names" not in argv)
    print(difference or "equal")
    return 1 if difference else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
