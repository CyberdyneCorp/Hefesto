# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Ways of running a case's reference tool (used by capture.py).

Every runner works in a scratch directory holding a copy of the case's
``input/`` as ``<work>/input``, runs with ``TZ=UTC`` and ``LC_ALL=C``, and
returns the streams plus the facts that go into provenance.json.

``container``  the pinned reference image (container/Containerfile); the normal path.
``local``      magic-hash-harness only: downloads Magic's utils/hash.c, hash.h and
               malloc.h at the pinned commit (SHA-256 checked against pins.toml),
               or takes them from --magic-src, and compiles them with the host cc.
``model``      magic-hash-harness only: goldenlib/magic_hash_model.py, a transcription
               of the requirement text. Output is marked reference_captured=false.
"""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

from . import magic_hash_model
from .case import Case
from .pins import tool_pin

ENVIRONMENT = {"TZ": "UTC", "LC_ALL": "C"}
HARNESS_DIR = Path(__file__).resolve().parent.parent / "harness" / "magic_hash"
TIMEOUT_S = 600
INSPECT_TIMEOUT_S = 30  # a stopped container daemon makes `image inspect` hang


class RunnerError(RuntimeError):
    """The reference tool could not be run."""


@dataclass
class RunResult:
    stdout: bytes
    stderr: bytes
    returncode: int
    command: list[str]
    facts: dict = field(default_factory=dict)  # runner-specific provenance


def default_argv(case: Case) -> list[str]:
    """Command line of the reference tool, relative to the work directory."""
    ref = case.reference
    if "argv" in ref:
        return list(ref["argv"])
    script = ref.get("script")
    if script is None:
        raise RunnerError(f"{case.id}: [reference] needs 'script' or 'argv'")
    tech = ["-T", ref["tech"]] if "tech" in ref else []
    table = {
        "magic": ["magic", "-dnull", "-noconsole", *tech, script],
        "netgen": ["netgen", "-batch", "source", script],
        "ngspice": ["ngspice", "-b", script],
        "xschem": ["xschem", "-q", "-x", "--no_x", "--script", script],
        "magic-hash-harness": ["magic-hash-harness"],
    }
    return table[case.tool]


def harness_stdin(case: Case, work: Path) -> bytes:
    """magic-hash-harness reads its script on stdin; other tools get nothing."""
    if case.tool != "magic-hash-harness":
        return b""
    return (work / case.reference["script"]).read_bytes()


def _run(argv: list[str], stdin: bytes, cwd: Path, timeout: int = TIMEOUT_S) -> subprocess.CompletedProcess:
    env = {**os.environ, **ENVIRONMENT}
    try:
        return subprocess.run(argv, input=stdin, capture_output=True, cwd=cwd, env=env, timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise RunnerError(f"cannot run {argv[0]}: {error}") from error


class ContainerRunner:
    name = "container"

    def __init__(self, image: str, engine: str = "docker") -> None:
        self.image, self.engine = image, engine

    def _image_facts(self) -> dict:
        proc = _run([self.engine, "image", "inspect", "--format", "{{json .}}", self.image], b"", Path.cwd(), INSPECT_TIMEOUT_S)
        if proc.returncode != 0:
            raise RunnerError(f"image {self.image} not available: {proc.stderr.decode(errors='replace').strip()}")
        info = json.loads(proc.stdout)
        return {"image": self.image, "image_id": info.get("Id"), "image_repo_digests": info.get("RepoDigests", [])}

    def run(self, case: Case, work: Path) -> RunResult:
        facts = self._image_facts()
        env_flags = [flag for key, value in ENVIRONMENT.items() for flag in ("-e", f"{key}={value}")]
        argv = [self.engine, "run", "--rm", "-i", "--network=none", *env_flags,
                "-v", f"{work}:/work", "-w", "/work", self.image, *default_argv(case)]
        proc = _run(argv, harness_stdin(case, work), work)
        return RunResult(proc.stdout, proc.stderr, proc.returncode, default_argv(case), facts)


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class LocalHarnessRunner:
    """Builds magic-hash-harness on the host from Magic's pinned sources."""

    name = "local"

    def __init__(self, pins: dict, magic_src: Path | None = None) -> None:
        self.pins, self.magic_src = pins, magic_src

    def _source(self, rel: str, commit: str) -> bytes:
        if self.magic_src is not None:
            return (self.magic_src / rel).read_bytes()
        url = f"https://raw.githubusercontent.com/RTimothyEdwards/magic/{commit}/{rel}"
        with urllib.request.urlopen(url, timeout=60) as response:  # noqa: S310 (pinned https URL)
            return response.read()

    def _stage_sources(self, build: Path, commit: str) -> None:
        for entry in self.pins["harness"]["magic_hash"]["files"]:
            data = self._source(entry["path"], commit)
            if _sha256(data) != entry["sha256"]:
                raise RunnerError(f"{entry['path']}: SHA-256 mismatch with pins.toml")
            target = build / "magic" / entry["path"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)

    def _compile(self, build: Path) -> tuple[Path, list[str]]:
        exe = build / "magic-hash-harness"
        cc = os.environ.get("CC", "cc")
        argv = [cc, "-std=gnu99", "-O1", "-fsigned-char", "-w", "-DSUPPORT_DIRECT_MALLOC",
                f"-I{HARNESS_DIR / 'shim'}", f"-I{build / 'magic'}", "-o", str(exe),
                str(HARNESS_DIR / "driver.c"), str(build / "magic" / "utils" / "hash.c")]
        proc = _run(argv, b"", build)
        if proc.returncode != 0:
            raise RunnerError(f"harness build failed:\n{proc.stderr.decode(errors='replace')}")
        return exe, argv

    def run(self, case: Case, work: Path) -> RunResult:
        if case.tool != "magic-hash-harness":
            raise RunnerError(f"{case.id}: the local runner supports only magic-hash-harness")
        commit = tool_pin(self.pins, case.tool)["commit"]
        build = work / ".harness-build"
        build.mkdir()
        try:
            self._stage_sources(build, commit)
            exe, compile_argv = self._compile(build)
            proc = _run([str(exe)], harness_stdin(case, work), work)
        finally:
            shutil.rmtree(build, ignore_errors=True)
        facts = {"compiler_command": [Path(compile_argv[0]).name, *compile_argv[1:6]]}
        return RunResult(proc.stdout, proc.stderr, proc.returncode, ["magic-hash-harness"], facts)


class ModelRunner:
    name = "model"

    def run(self, case: Case, work: Path) -> RunResult:
        if case.tool != "magic-hash-harness":
            raise RunnerError(f"{case.id}: the model runner supports only magic-hash-harness")
        stdout = magic_hash_model.run_script(harness_stdin(case, work))
        facts = {"model": "tools/golden/goldenlib/magic_hash_model.py"}
        return RunResult(stdout, b"", 0, ["magic_hash_model.run_script"], facts)
