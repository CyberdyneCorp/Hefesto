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
GUARD = (f'command -v "$0" >/dev/null 2>&1 || {{ echo "{MISSING_MARKER} $0" >&2; exit {MISSING_STATUS}; }}; '
         'exec "$0" "$@"')


class CommandError(ValueError):
    """input/command.toml is malformed."""


@dataclass(frozen=True)
class Command:
    role: str
    args: tuple[str, ...]
    reads: tuple[str, ...]
    cwd: str = "input"

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
    return Command(role, _strings(data, "args", where), reads, cwd)


def capture_argv(command: Command) -> list[str]:
    """The reference command line inside the image, through the missing-program guard."""
    return ["sh", "-c", GUARD, command.program.capture, *command.args]


def recorded_argv(command: Command) -> list[str]:
    """The command line provenance.json records (without the guard)."""
    return [command.program.capture, *command.args]


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
