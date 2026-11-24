# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Pairwise comparator ``raw-tolerance`` for ngspice raw files (ASCII and binary).

Capability simulation-analyses-and-results (raw files: golden comparisons ignore ``Date:`` and
``Command:``, Q-SIM-17) and capability simulation-engine-embedding, Requirement "Cross-platform result
consistency" (the tolerances, Q-SIM-10); m9-lvs-simulation design D19.

A raw file is one or more plots. Each plot is a header (``Title:``, ``Date:``, ``Plotname:``, ``Flags:``,
``No. Variables:``, ``No. Points:``, other ``Key: value`` lines, ``Variables:`` and one line per
vector ``<index> <name> <type> ...``) followed by ``Values:`` (ASCII: per point its index, then one
value per vector, complex values as ``re,im``) or ``Binary:`` (little-endian binary64, point by point,
two numbers per vector when the plot's ``Flags:`` say ``complex``).

Two raw files match when:

1. they have the same number of plots;
2. each plot's header lines are equal byte for byte (after trailing white space is removed), except the
   ``Date:`` and ``Command:`` lines, which are ignored;
3. the first vector (the scale: time, frequency, sweep) agrees point by point within
   ``1e-3 × max(|a|, |b|) + 1e-9 × span``, where span is the expected scale's range;
4. every other vector agrees at the expected file's scale points, the actual vector being linearly
   interpolated onto them when the scales differ, within ``|a − b| ≤ 1e-3 × max(|a|, |b|) + abs``, with
   abs = 1e-4 for type ``voltage``, 1e-9 for type ``current``, 1e-12 (of the vector's unit, on the
   complex magnitude of the difference) for a complex plot, and 1e-12 for any other type.

A file that cannot be parsed compares byte for byte. The difference names the plot, the vector, the
point and both values. The canonical form (for display) is the header without ``Date:`` and
``Command:`` and the values in ASCII with 17 significant digits.

Example (Q-SIM-17): two transient raw files differing only in ``Date:`` match; ``v(out)`` 1.800 V
against 1.818 V (1 %) does not: the tolerance is 1.9e-3 V.
"""
from __future__ import annotations

import bisect
import struct
from dataclasses import dataclass, field

NAME = "raw-tolerance"

REL = 1e-3
ABS = {"voltage": 1e-4, "current": 1e-9}
ABS_COMPLEX = 1e-12
ABS_OTHER = 1e-12
SCALE_SPAN = 1e-9
IGNORED = ("Date:", "Command:")


class RawError(ValueError):
    """The data is not a raw file this module reads."""


@dataclass
class Plot:
    header: list[str] = field(default_factory=list)  # compared lines (Date: and Command: removed)
    names: list[str] = field(default_factory=list)
    types: list[str] = field(default_factory=list)
    complex: bool = False
    points: int = 0
    values: list[list[complex]] = field(default_factory=list)  # one list per vector


def _read_line(data: bytes, pos: int) -> tuple[str, int]:
    end = data.find(b"\n", pos)
    end = len(data) if end < 0 else end
    return data[pos:end].decode("latin-1").rstrip(), end + 1


def _header_field(plot: Plot, line: str) -> None:
    key, _, value = line.partition(":")
    if key == "Flags":
        plot.complex = "complex" in value.lower().split()
    elif key == "No. Points":
        plot.points = int(value)


def _read_header(data: bytes, pos: int) -> tuple[Plot, int, str]:
    plot = Plot()
    in_variables = False
    while pos < len(data):
        line, pos = _read_line(data, pos)
        if line in ("Values:", "Binary:"):
            if not plot.names or plot.points <= 0:
                raise RawError("raw header without variables or points")
            return plot, pos, line
        if not line.startswith(IGNORED):
            plot.header.append(line)
        if line == "Variables:":
            in_variables = True
        elif in_variables and line[:1] in (" ", "\t") and len(line.split()) >= 3:
            plot.names.append(line.split()[1])
            plot.types.append(line.split()[2])
        elif not line.strip():
            continue
        else:
            in_variables = False
            _header_field(plot, line)
    raise RawError("raw header without Values: or Binary:")


def _number(token: str, is_complex: bool) -> complex:
    if is_complex:
        re_part, _, im_part = token.partition(",")
        return complex(float(re_part), float(im_part or 0.0))
    return complex(float(token), 0.0)


def _read_ascii(data: bytes, pos: int, plot: Plot) -> int:
    count = len(plot.names)
    values: list[list[complex]] = [[] for _ in range(count)]
    tokens: list[str] = []
    while len(tokens) < plot.points * (count + 1) and pos < len(data):
        line, pos = _read_line(data, pos)
        tokens += line.split()
    if len(tokens) < plot.points * (count + 1):
        raise RawError("raw values end early")
    for point in range(plot.points):
        row = tokens[point * (count + 1) + 1:(point + 1) * (count + 1)]
        for vector, token in enumerate(row):
            values[vector].append(_number(token, plot.complex))
    plot.values = values
    return pos


def _read_binary(data: bytes, pos: int, plot: Plot) -> int:
    count = len(plot.names)
    width = 2 if plot.complex else 1
    size = plot.points * count * width * 8
    if pos + size > len(data):
        raise RawError("raw binary data ends early")
    numbers = struct.unpack_from(f"<{plot.points * count * width}d", data, pos)
    values: list[list[complex]] = [[] for _ in range(count)]
    for index in range(plot.points * count):
        pair = numbers[index * width:(index + 1) * width]
        values[index % count].append(complex(pair[0], pair[1] if width == 2 else 0.0))
    plot.values = values
    return pos + size


def parse(data: bytes) -> list[Plot]:
    """The plots of a raw file; RawError when it is not one."""
    plots = []
    pos = 0
    while pos < len(data):
        if not data[pos:].strip():
            break
        plot, pos, kind = _read_header(data, pos)
        pos = _read_ascii(data, pos, plot) if kind == "Values:" else _read_binary(data, pos, plot)
        plots.append(plot)
    if not plots:
        raise RawError("no plot")
    return plots


def _interpolate(xs: list[float], ys: list[complex], x: float) -> complex:
    """``ys`` at ``x``, linearly interpolated over the increasing or decreasing scale ``xs``."""
    ordered = xs if xs[0] <= xs[-1] else xs[::-1]
    values = ys if xs[0] <= xs[-1] else ys[::-1]
    right = min(max(bisect.bisect_left(ordered, x), 1), len(ordered) - 1)
    x0, x1 = ordered[right - 1], ordered[right]
    if x1 == x0:
        return values[right]
    t = (x - x0) / (x1 - x0)
    return values[right - 1] + (values[right] - values[right - 1]) * t


def _tolerance(plot: Plot, vector: int, a: complex, b: complex) -> float:
    absolute = ABS_COMPLEX if plot.complex else ABS.get(plot.types[vector], ABS_OTHER)
    return REL * max(abs(a), abs(b)) + absolute


def _scale_difference(index: int, exp: Plot, act: Plot) -> str | None:
    xs, ys = [v.real for v in exp.values[0]], [v.real for v in act.values[0]]
    span = (max(xs) - min(xs)) * SCALE_SPAN
    for point, (a, b) in enumerate(zip(xs, ys)):
        if abs(a - b) > REL * max(abs(a), abs(b)) + span:
            return f"plot {index} scale {exp.names[0]} point {point}: expected {a!r}, got {b!r}"
    return None


def _vector_difference(index: int, vector: int, exp: Plot, act: Plot) -> str | None:
    xs, axs = [v.real for v in exp.values[0]], [v.real for v in act.values[0]]
    same_scale = xs == axs
    for point, a in enumerate(exp.values[vector]):
        b = act.values[vector][point] if same_scale else _interpolate(axs, act.values[vector], xs[point])
        tolerance = _tolerance(exp, vector, a, b)
        if abs(a - b) > tolerance:
            return (f"plot {index} vector {exp.names[vector]} point {point}: expected {_show(a)}, "
                    f"got {_show(b)} (tolerance {tolerance:.3g})")
    return None


def _show(value: complex) -> str:
    return repr(value.real) if value.imag == 0 else f"{value.real!r},{value.imag!r}"


def _plot_difference(index: int, exp: Plot, act: Plot) -> str | None:
    if exp.header != act.header:
        for number, (a, b) in enumerate(zip(exp.header, act.header), 1):
            if a != b:
                return f"plot {index} header line {number}: expected {a!r}, got {b!r}"
        return f"plot {index} header: expected {len(exp.header)} lines, got {len(act.header)}"
    difference = _scale_difference(index, exp, act)
    for vector in range(1, len(exp.names)):
        difference = difference or _vector_difference(index, vector, exp, act)
    return difference


def compare(expected: bytes, actual: bytes) -> str | None:
    """None when the raw files match (module docstring), else the first difference."""
    try:
        exp_plots, act_plots = parse(expected), parse(actual)
    except (RawError, ValueError, struct.error) as error:
        return None if expected == actual else f"not comparable as raw files ({error}); bytes differ"
    if len(exp_plots) != len(act_plots):
        return f"expected {len(exp_plots)} plots, got {len(act_plots)}"
    for index, (exp, act) in enumerate(zip(exp_plots, act_plots), 1):
        difference = _plot_difference(index, exp, act)
        if difference:
            return difference
    return None


def canonicalize(data: bytes) -> bytes:
    """Display form: headers without ``Date:``/``Command:``, values in ASCII with 17 significant digits."""
    try:
        plots = parse(data)
    except (RawError, ValueError, struct.error):
        return data
    out = []
    for plot in plots:
        out += plot.header + ["Values:"]
        for point in range(plot.points):
            cells = [f"{v.real:.17g},{v.imag:.17g}" if plot.complex else f"{v.real:.17g}"
                     for v in (vector[point] for vector in plot.values)]
            out.append(f"{point}\t" + "\t".join(cells))
    return ("\n".join(out) + "\n").encode("utf-8")

