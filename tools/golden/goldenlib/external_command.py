# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The external-command case mode (m7-extraction design D15 and D21, task 2.3).

Most cases run a Tcl script through Magic and through ``hefesto-cli --magic``. A few run a standalone
program instead: NL-CHK-01 runs Magic's ``extcheck``, which Hefesto ships as ``hefesto-extcheck``
(Open Question 6 (a)). Such a case sets ``[reference] mode = "external-command"`` and describes the
run in ``input/command.toml``::

    schema = 1
    role = "extcheck"            # the program, by role (ROLES below)
    args = ["-c", "0", "inv"]    # its arguments, the same for both runs
    reads = ["inv.ext"]          # the files of input/ it reads (each must exist)
    cwd = "input"                # optional: run directory, relative to the work directory

A case that runs the program several times (NL-CHK-01: default thresholds, ``-c 0 -r 0``, ``-C -r 0``
and the usage error) lists ``[[run]]`` tables instead of ``args``::

    [[run]]
    name = "default"             # [A-Za-z0-9_-]+, unique in the case
    args = ["-p", "input", "top"]

Each run starts in ``cwd`` and writes ``<name>.stdout``, ``<name>.stderr`` and ``<name>.exit`` (the
exit status as one decimal line) in the work directory; the case lists those files as plain path
outputs. The runs are independent: a non-zero status of one does not stop the next.

The role is mapped to a program per side: ``extcheck`` runs Magic's ``extcheck`` (built in the
capture image, container/build_extcheck.sh) on capture and ``hefesto-extcheck`` on replay. Both runs
get ``TZ=UTC`` and ``LC_ALL=C``. stdout, stderr and the exit status are captured as the usual run
sources (``stdout``, ``stderr``, ``exitcode``); a non-zero exit status is an output, not a failure.
Masks that apply: ``prog-name`` (the program name a usage line starts with becomes ``<prog>`` on
both sides) and ``extcheck-memory`` (the informative ``Memory used:`` value), compare/masks.py.

In the image the program is started through a guard (``GUARD``): when the image does not have it
(Magic's extcheck may not link at the pin, design R7), the run ends with exit status 127 and the
marker ``MISSING_MARKER`` on stderr, and capture.py reports an error instead of capturing that.
"""
from __future__ import annotations

import re
import shlex
import subprocess
import tomllib
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

MODE = "external-command"
COMMAND_FILE = "input/command.toml"


@dataclass(frozen=True)
class Role:
    capture: str  # the program in the capture image
    replay: str   # Hefesto's program, found next to hefesto-cli or given to replay.py


ROLES: dict[str, Role] = {"extcheck": Role("extcheck", "hefesto-extcheck")}

MISSING_STATUS = 127
MISSING_MARKER = "hefesto-golden: reference program not in the image:"
# sh -c GUARD <prog> <args...>: $0 is the program; exec keeps its argv[0] as given.
GUARD_CHECK = f'command -v "$0" >/dev/null 2>&1 || {{ echo "{MISSING_MARKER} $0" >&2; exit {MISSING_STATUS}; }}'
GUARD = f'{GUARD_CHECK}; exec "$0" "$@"'


class CommandError(ValueError):
    """input/command.toml is malformed."""


RUN_NAME = re.compile(r"[A-Za-z0-9_-]+")
RUN_OUTPUTS = ("stdout", "stderr", "exit")  # <name>.<kind> per [[run]]


@dataclass(frozen=True)
class Run:
    name: str
    args: tuple[str, ...]


@dataclass(frozen=True)
class Command:
    role: str
    args: tuple[str, ...]
    reads: tuple[str, ...]
    cwd: str = "input"
    runs: tuple[Run, ...] = ()  # [[run]] tables; empty: one run with ``args``

    @property
    def program(self) -> Role:
        return ROLES[self.role]


def is_external(reference: dict) -> bool:
    return reference.get("mode") == MODE


def _strings(data: dict, key: str, where: str) -> tuple[str, ...]:
    value = data.get(key, [])
    if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
        raise CommandError(f"{where}: '{key}' must be a list of strings")
    return tuple(value)


def _relative(path: str, where: str, key: str) -> str:
    pure = PurePosixPath(path)
    if not path or pure.is_absolute() or ".." in pure.parts:
        raise CommandError(f"{where}: {key} '{path}' must be a relative path inside the case")
    return path


def _runs(data: dict, where: str) -> tuple[Run, ...]:
    raw = data.get("run", [])
    if not isinstance(raw, list) or not all(isinstance(r, dict) for r in raw):
        raise CommandError(f"{where}: 'run' must be an array of tables")
    if raw and "args" in data:
        raise CommandError(f"{where}: give either 'args' or [[run]] tables, not both")
    runs = tuple(Run(r.get("name", ""), _strings(r, "args", where)) for r in raw)
    names = [run.name for run in runs]
    for name in names:
        if not isinstance(name, str) or not RUN_NAME.fullmatch(name):
            raise CommandError(f"{where}: run name {name!r} must match {RUN_NAME.pattern}")
    if len(set(names)) != len(names):
        raise CommandError(f"{where}: run names must be unique")
    return runs


def run_outputs(command: Command) -> list[str]:
    """The work-directory files the [[run]] tables write, in order."""
    return [f"{run.name}.{kind}" for run in command.runs for kind in RUN_OUTPUTS]


def load(case_root: Path) -> Command:
    """The validated ``input/command.toml`` of a case directory."""
    path = Path(case_root) / COMMAND_FILE
    where = str(path)
    if not path.is_file():
        raise CommandError(f"{where}: missing (required by [reference] mode = \"{MODE}\")")
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != 1:
        raise CommandError(f"{where}: unsupported schema {data.get('schema')!r}")
    role = data.get("role")
    if role not in ROLES:
        raise CommandError(f"{where}: role must be one of {', '.join(sorted(ROLES))}")
    cwd = _relative(data.get("cwd", "input"), where, "cwd")
    reads = tuple(_relative(r, where, "reads") for r in _strings(data, "reads", where))
    base = Path(case_root) / cwd if cwd != "." else Path(case_root)
    for read in reads:
        if not (base / read).is_file():
            raise CommandError(f"{where}: reads '{read}' is not in {cwd}/")
    return Command(role, _strings(data, "args", where), reads, cwd, _runs(data, where))


def _to_work(command: Command) -> str:
    """The work directory relative to the run directory ``cwd`` (``""`` or ``../``...)."""
    depth = len([p for p in PurePosixPath(command.cwd).parts if p != "."])
    return "../" * depth


def multi_run_script(command: Command) -> str:
    """The shell text (after GUARD's check) that runs every [[run]] and writes its three files."""
    work = _to_work(command)
    lines = []
    for run in command.runs:
        out = shlex.quote(f"{work}{run.name}")
        args = "".join(f" {shlex.quote(a)}" for a in run.args)
        lines.append(f'"$0"{args} >{out}.stdout 2>{out}.stderr; echo $? >{out}.exit')
    return "\n".join(lines)


def capture_argv(command: Command) -> list[str]:
    """The reference command line inside the image, through the missing-program guard."""
    if command.runs:
        return ["sh", "-c", f"{GUARD_CHECK}\n{multi_run_script(command)}", command.program.capture]
    return ["sh", "-c", GUARD, command.program.capture, *command.args]


def recorded_argv(command: Command) -> list[str]:
    """The command line provenance.json records (without the guard); one ``;``-joined line per run."""
    if command.runs:
        return [part for run in command.runs for part in (";", command.program.capture, *run.args)][1:]
    return [command.program.capture, *command.args]


def replay_runs(program: Path, command: Command, work: Path, env: dict, timeout: float) -> None:
    """Hefesto's program once per [[run]], from ``cwd`` in ``work``, writing each run's three files."""
    cwd = work if command.cwd == "." else work / command.cwd
    for run in command.runs:
        proc = subprocess.run([str(program), *run.args], capture_output=True, cwd=cwd, env=env,
                              timeout=timeout, check=False)
        (work / f"{run.name}.stdout").write_bytes(proc.stdout)
        (work / f"{run.name}.stderr").write_bytes(proc.stderr)
        (work / f"{run.name}.exit").write_text(f"{proc.returncode}\n", encoding="ascii")


def is_missing(returncode: int, stderr: bytes) -> bool:
    """Whether a capture run only found that the image lacks the program."""
    return returncode == MISSING_STATUS and MISSING_MARKER.encode() in stderr


def find_replay_program(role: Role, cli: Path | None, explicit: Path | None) -> Path | None:
    """Hefesto's program of ``role``: ``explicit`` (``--hefesto-extcheck`` or ``$HEFESTO_EXTCHECK``)
    when given, else next to hefesto-cli (``<dir>/<name>`` or ``<dir>/extcheck/<name>``, the
    ``src/cli/extcheck/`` target of the build tree); None when not built."""
    if explicit is not None:
        return explicit if explicit.is_file() else None
    if cli is None:
        return None
    for candidate in (cli.parent / role.replay, cli.parent / "extcheck" / role.replay):
        if candidate.is_file():
            return candidate
    return None
