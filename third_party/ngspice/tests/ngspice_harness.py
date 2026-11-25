#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Drives libngspice-hefesto through its C API (ctypes), one scenario per process.

    ngspice_harness.py <path to libngspice-hefesto> <scenario>

ngspice keeps its state in process globals, so every scenario runs in a fresh
process (test_engine.py starts them). The result is one JSON object on stdout:
{"output": [lines the engine printed], "vectors": {name: [values]}}.
The harness initialises the engine as the adapter will (design D9): no spinit,
no .spiceinit, then the fixed command list.
"""
from __future__ import annotations

import ctypes
import json
import sys

SEND_CHAR = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
SEND_STAT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
CONTROLLED_EXIT = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_bool, ctypes.c_bool,
                                   ctypes.c_int, ctypes.c_void_p)
SEND_DATA = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p)
SEND_INIT_DATA = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)
BG_RUNNING = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_bool, ctypes.c_int, ctypes.c_void_p)


class VectorInfo(ctypes.Structure):
    _fields_ = [("v_name", ctypes.c_char_p), ("v_type", ctypes.c_int), ("v_flags", ctypes.c_short),
                ("v_realdata", ctypes.POINTER(ctypes.c_double)), ("v_compdata", ctypes.c_void_p),
                ("v_length", ctypes.c_int)]


class Engine:
    def __init__(self, path: str, spiceinit: bool = False):
        self.lib = ctypes.CDLL(path)
        self.output: list[str] = []
        self._callbacks = (SEND_CHAR(self._char), SEND_STAT(lambda *_: 0), CONTROLLED_EXIT(self._exit),
                           SEND_DATA(lambda *_: 0), SEND_INIT_DATA(lambda *_: 0), BG_RUNNING(lambda *_: 0))
        self.lib.ngGet_Vec_Info.restype = ctypes.POINTER(VectorInfo)
        self.lib.ngSpice_nospinit()
        if not spiceinit:
            self.lib.ngSpice_nospiceinit()
        self.lib.ngSpice_Init(*self._callbacks, None)
        for command in ("set noaskquit", "set nomoremode", "set filetype=binary"):
            self.command(command)

    def _char(self, text, _ident, _user):
        line = text.decode("utf-8", "replace")
        self.output.append(line.split(" ", 1)[1] if " " in line else line)
        return 0

    def _exit(self, status, _unload, _quit, _ident, _user):
        self.output.append(f"controlled exit {status}")
        return 0

    def command(self, text: str) -> int:
        return self.lib.ngSpice_Command(text.encode())

    def circuit(self, lines: list[str]) -> int:
        array = (ctypes.c_char_p * (len(lines) + 1))(*[line.encode() for line in lines], None)
        return self.lib.ngSpice_Circ(array)

    def vector(self, name: str) -> list[float]:
        info = self.lib.ngGet_Vec_Info(name.encode())
        if not info or not info.contents.v_realdata:
            return []
        return [info.contents.v_realdata[i] for i in range(info.contents.v_length)]


TRNOISE = ["* trnoise", "V1 n 0 dc 0 trnoise(1m 1n 0 0)", "R1 n 0 1k", ".tran 1n 100n", ".end"]


def seed(path):
    e = Engine(path)
    e.command("setseed 1")
    e.command("let v = vector(4)")
    e.command("let r = sunif(v)")
    return e, {"r": e.vector("r")}


def trnoise_twice(path):
    e = Engine(path)
    runs = []
    for _ in range(2):
        e.command("setseed 1")
        e.circuit(TRNOISE)
        e.command("run")
        runs.append(e.vector("n"))
        e.command("remcirc")
        e.command("destroy all")
    return e, {"first": runs[0], "second": runs[1]}


def spiceinit(path):
    e = Engine(path, spiceinit=False)
    e.command("echo probe=$?hefesto_probe")
    return e, {}


def codemodel(path):
    e = Engine(path)
    e.command("codemodel /nonexistent/analog.cm")
    e.circuit(["* gain code model", "V1 in 0 dc 1", "a1 in out amp", ".model amp gain(gain=2.5)",
               "R1 out 0 1k", ".op", ".end"])
    e.command("run")
    return e, {"out": e.vector("out")}


def osdi_static_only(path):
    e = Engine(path)
    e.command("set hefesto_osdi_static_only")
    e.command("osdi /nonexistent/psp103_nqs.osdi")
    return e, {}


def osdi_dynamic(path):
    e = Engine(path)
    e.command("osdi /nonexistent/psp103_nqs.osdi")
    return e, {}


def devices(path):
    e = Engine(path)
    e.command("devhelp")
    return e, {}


SCENARIOS = {f.__name__: f for f in (seed, trnoise_twice, spiceinit, codemodel, osdi_static_only, osdi_dynamic,
                                     devices)}


def main(argv: list[str]) -> int:
    engine, vectors = SCENARIOS[argv[1]](argv[0])
    json.dump({"output": engine.output, "vectors": vectors}, sys.stdout)
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
