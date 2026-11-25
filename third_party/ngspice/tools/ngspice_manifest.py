#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Build manifest, licence check and source archive of libngspice-hefesto.

Capability simulation-engine-embedding, "ngspice build configuration and license
hygiene" (scenarios "No GPL code linked" and "Patch list in the manifest");
openspec/changes/m9-lvs-simulation design D2 ("Replaceability", "MPL-2.0") and D9.

    ngspice_manifest.py manifest <common> --out ngspice-manifest.json
    ngspice_manifest.py archive  <common> --wrapper third_party/ngspice --out x.tar.gz
    ngspice_manifest.py check    --manifest ngspice-manifest.json

<common> is --pin pin.json --series patches/series --sources-cmake cmake/sources.cmake
--src <patched ngspice tree> --code-models <build>/code_models.txt (the CMake wrapper
passes them). `manifest` fails when a compiled file carries a GPL notice.
"""
from __future__ import annotations

import argparse
import fnmatch
import gzip
import hashlib
import io
import json
import re
import sys
import tarfile
from pathlib import Path

SCHEMA = 1
GROUP_RE = re.compile(r"ngspice_group\((\w+) \w+ \w+\n  DEFINES .*\n  INCLUDES .*\n  SOURCES\n((?:    .*\n)*)\)")
GPL_PATHS = ("src/tclspice.c", "src/xspice/icm/table/", "src/xspice/verilog/", "src/xspice/vhdl/")
BISON_EXCEPTION = "special exception, you may create a larger work that contains"

# Components with a licence other than ngspice's own (COPYING of the pinned tree,
# section "files with licenses different to 'Modified BSD'"), in the spec's order.
# `copying` must appear in COPYING verbatim, so an upstream change is noticed.
COMPONENTS = [
    ("KLU", "LGPLv2", "all files in ngspice/src/maths/KLU",
     ["src/maths/KLU/klu*.c", "src/include/ngspice/klu.h", "src/include/ngspice/klu-binding.h"]),
    ("AMD", "LGPLv2", "all files in ngspice/src/maths/KLU",
     ["src/maths/KLU/amd_*.c", "src/maths/KLU/amd_internal.h", "src/include/ngspice/amd.h"]),
    ("COLAMD", "LGPLv2", "all files in ngspice/src/maths/KLU",
     ["src/maths/KLU/colamd*.c", "src/include/ngspice/colamd.h"]),
    ("BTF", "LGPLv2", "all files in ngspice/src/maths/KLU",
     ["src/maths/KLU/btf_*.c", "src/maths/KLU/btf_internal.h", "src/include/ngspice/btf.h"]),
    ("SuiteSparse_config", "LGPLv2", "all files in ngspice/src/maths/KLU",
     ["src/include/ngspice/UFconfig.h"]),
    ("numparam", "LGPLv2 or newer", "ngspice/src/frontend/numparam", ["src/frontend/numparam/*"]),
    ("OSDI", "MPL-2.0", "all files in ngspice/src/osdi",
     ["src/osdi/*", "src/include/ngspice/osdiitf.h"]),
    ("cppduals", "MPL-2.0", None, ["src/include/cppduals/duals/dual"]),
    ("sparse", "MIT (unnamed)", "all files in ngspice/src/maths/sparse", ["src/maths/sparse/*"]),
    ("XSPICE code models", "public domain", "ngspice/src/xspice",
     ["src/xspice/icm/analog/*", "src/xspice/icm/digital/*", "src/xspice/icm/spice2poly/*",
      "src/xspice/icm/xtradev/*", "src/xspice/icm/dlmain.c"]),
]
NOTICE_RE = re.compile(r"(Lesser\s+General\s+Public\s+License[^\n]*|Mozilla[\s*/]+Public[\s*/]+License[^\n]*|"
                       r"No\s+licensing\s+restrictions[^\n]*|See\s+\S+\s+for\s+License[^\n]*|"
                       r"public\s+domain[^\n]*)", re.IGNORECASE)
FEATURES = {
    "shared-library API (SHARED_MODULE)": "on: libngspice-hefesto",
    "XSPICE": "on",
    "XSPICE code models": "analog, digital (without d_cosim), spice2poly, xtradev; compiled in statically",
    "table code model library": "off",
    "d_cosim, GHDL and Icarus co-simulation shims": "off",
    "OSDI": "on; built-in registry (patch 0001)",
    "KLU": "on, bundled copy",
    "CIDER, NDEV": "off",
    "FFTW3": "off (built-in FFT)",
    "libsndfile / libsamplerate": "off",
    "readline/editline, X11 plotting, help browser": "off",
    "OpenMP": "off",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_series(series: Path) -> list[dict]:
    """Patches in order, each with the comment block above it as justification."""
    patches, comment = [], []
    for line in series.read_text(encoding="utf-8").splitlines():
        if line.startswith("#"):
            comment.append(line[1:].strip())
        elif line.strip():
            title = comment[0] if comment else ""
            patches.append({"file": line.strip(), "title": re.sub(r"^\d{4} ", "", title).rstrip("."),
                            "justification": " ".join(c for c in comment[1:] if c),
                            "sha256": sha256(series.parent / line.strip())})
            comment = []
        else:
            comment = []
    return patches


def compiled_files(sources_cmake: Path, variant: str = "shared") -> list[str]:
    files = set()
    for m in GROUP_RE.finditer(sources_cmake.read_text(encoding="utf-8")):
        if m.group(1) == variant:
            files.update(m.group(2).split())
    return sorted(files)


def code_models(listing: Path) -> list[dict]:
    rows = []
    for line in listing.read_text(encoding="utf-8").splitlines():
        if line.strip():
            path, function = line.split()
            rows.append({"model": path, "function": function})
    return rows


def code_model_inputs(models: list[dict]) -> list[str]:
    files = ["src/xspice/icm/dlmain.c"]
    for row in models:
        base = f"src/xspice/icm/{row['model']}"
        files += [f"{base}/cfunc.mod", f"{base}/ifspec.ifs"]
    return files


def headers(src: Path, files: list[str]) -> list[str]:
    dirs = {str(Path(f).parent) for f in files} | {"src/include"}
    found = set()
    for d in dirs:
        pattern = "**/*" if d == "src/include" else "*.h"
        found.update(p.relative_to(src).as_posix() for p in (src / d).glob(pattern) if p.is_file())
    return sorted(found)


def gpl_notice(text: str) -> bool:
    """True for a GPL notice that is not the LGPL and not Bison's exception."""
    if BISON_EXCEPTION in text:
        return False
    for m in re.finditer(r"General Public\s+License", text):
        before = text[max(0, m.start() - 12):m.start()]
        if "Lesser" not in before and "Library" not in before:
            return True
    return bool(re.search(r"SPDX-License-Identifier:\s*GPL", text))


def gpl_check(src: Path, files: list[str]) -> list[str]:
    bad = [f for f in files if f.startswith(GPL_PATHS)]
    for f in files:
        text = (src / f).read_text(encoding="utf-8", errors="replace")
        if gpl_notice(text):
            bad.append(f)
    return sorted(set(bad))


def component_entry(src: Path, copying: str, comp: tuple, built: set[str]) -> dict:
    name, licence, statement, globs = comp
    files = sorted(f for f in built if any(fnmatch.fnmatch(f, g) for g in globs))
    if statement and statement not in copying:
        raise SystemExit(f"ngspice_manifest: COPYING no longer says '{statement}' ({name})")
    notices = set()
    for f in files:
        found = NOTICE_RE.findall((src / f).read_text(errors="replace"))
        notices.update(" ".join(n.split()) for n in found)
    return {"component": name, "license": licence, "copying_statement": statement,
            "file_notices": sorted(notices), "files": files}


def licences(src: Path, built: list[str]) -> list[dict]:
    copying = (src / "COPYING").read_text(encoding="utf-8", errors="replace")
    built_set = set(built)
    rows = [{"component": "ngspice", "license": "Modified BSD (BSD-3-Clause)",
             "copying_statement": "The ngspice license is the `Modified BSD' license.",
             "file_notices": [], "files": []}]
    rows += [component_entry(src, copying, comp, built_set) for comp in COMPONENTS]
    missing = [r["component"] for r in rows[1:] if not r["files"]]
    if missing:
        raise SystemExit(f"ngspice_manifest: no built file found for {', '.join(missing)}")
    return rows


def built_inputs(args) -> tuple[list[str], list[dict]]:
    models = code_models(args.code_models)
    files = compiled_files(args.sources_cmake) + code_model_inputs(models)
    return sorted(set(files)), models


def build_manifest(args) -> dict:
    pin = json.loads(args.pin.read_text(encoding="utf-8"))
    files, models = built_inputs(args)
    scanned = sorted(set(files + headers(args.src, files)))
    gpl = gpl_check(args.src, scanned)
    if gpl:
        raise SystemExit("ngspice_manifest: GPL code in the build: " + ", ".join(gpl))
    features = pin["configure"]["features"]
    return {
        "schema": SCHEMA,
        "name": "libngspice-hefesto",
        "version": f"{pin['release']}-hefesto{pin['hefesto_level']}",
        "source": {"name": pin["name"], "release": pin["release"], "tag": pin["tag"], "commit": pin["commit"],
                   "repo": pin["repo"], "archive": pin["archive"]},
        "patches": read_series(args.series),
        "configure": {"features": features,
                      "equivalent_configure_line": "./configure --with-ngshared " + " ".join(features),
                      "build": "Hefesto CMake wrapper third_party/ngspice/CMakeLists.txt",
                      "compiler_flags": ["-std=gnu11", "-ffp-contract=off", "-fvisibility=hidden"],
                      "openmp": False},
        "features": FEATURES,
        "code_models": {"libraries": pin["code_models"], "models": models,
                        "excluded": pin["code_models_excluded"]},
        "osdi_modules": [],
        "licenses": licences(args.src, scanned),
        "license_check": {"scanned_files": len(scanned), "gpl_files": gpl},
        "artifacts": ["libngspice-hefesto", "ngspice (capture image only)"],
    }


def tar_add(tar: tarfile.TarFile, name: str, data: bytes) -> None:
    info = tarfile.TarInfo(name)
    info.size, info.mtime, info.mode = len(data), 0, 0o644
    tar.addfile(info, io.BytesIO(data))


def archive_members(args) -> list[tuple[str, Path]]:
    files, _ = built_inputs(args)
    tree = set(files) | set(headers(args.src, files))
    tree |= {p.relative_to(args.src).as_posix() for p in (args.src / "src/osdi").rglob("*") if p.is_file()}
    tree |= {p.relative_to(args.src).as_posix() for p in (args.src / "src/xspice/cmpp").rglob("*") if p.is_file()}
    tree |= {"COPYING", "AUTHORS", "README", "src/spinit.in"}
    members = [(f"ngspice-47/{f}", args.src / f) for f in sorted(tree)]
    wrapper = [p for p in sorted(args.wrapper.rglob("*")) if p.is_file() and "__pycache__" not in p.parts]
    members += [(f"hefesto/third_party/ngspice/{p.relative_to(args.wrapper).as_posix()}", p) for p in wrapper]
    return members


def write_archive(args) -> None:
    pin = json.loads(args.pin.read_text(encoding="utf-8"))
    prefix = f"ngspice-hefesto-{pin['release']}-hefesto{pin['hefesto_level']}-src/"
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.PAX_FORMAT) as tar:
        for name, path in archive_members(args):
            tar_add(tar, prefix + name, path.read_bytes())
    with gzip.GzipFile(args.out, "wb", mtime=0) as gz:
        gz.write(raw.getvalue())


REQUIRED_COMPONENTS = ("ngspice", "KLU", "AMD", "COLAMD", "BTF", "SuiteSparse_config", "numparam",
                       "OSDI", "sparse", "XSPICE code models")
FORBIDDEN_COMPONENTS = ("fftw", "table", "readline")


def check_manifest(manifest: dict) -> list[str]:
    errors = []
    if not re.fullmatch(r"\d+(\.\d+)?-hefesto\d+", manifest.get("version", "")):
        errors.append(f"version {manifest.get('version')!r} is not <release>-hefesto<n>")
    patches = [p["file"] for p in manifest.get("patches", [])]
    if not any("portable-seeding" in p for p in patches):
        errors.append("the portable-seeding patch is not listed")
    names = [c["component"] for c in manifest.get("licenses", [])]
    errors += [f"licence of {c} not listed" for c in REQUIRED_COMPONENTS if c not in names]
    errors += [f"{n} must not be listed" for n in names if n.lower().startswith(FORBIDDEN_COMPONENTS)]
    errors += [f"no licence for {c['component']}" for c in manifest.get("licenses", []) if not c.get("license")]
    if manifest.get("license_check", {}).get("gpl_files"):
        errors.append("GPL files compiled: " + ", ".join(manifest["license_check"]["gpl_files"]))
    return errors


def parse_args(argv: list[str]):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="command", required=True)
    for name in ("manifest", "archive"):
        p = sub.add_parser(name)
        for opt in ("--pin", "--series", "--sources-cmake", "--src", "--code-models", "--out"):
            p.add_argument(opt, type=Path, required=True)
        if name == "archive":
            p.add_argument("--wrapper", type=Path, required=True)
    sub.add_parser("check").add_argument("--manifest", type=Path, required=True)
    return ap.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.command == "manifest":
        text = json.dumps(build_manifest(args), indent=2, sort_keys=False) + "\n"
        args.out.write_text(text, encoding="utf-8")
        return 0
    if args.command == "archive":
        write_archive(args)
        print(f"ngspice_manifest: wrote {args.out}")
        return 0
    errors = check_manifest(json.loads(args.manifest.read_text(encoding="utf-8")))
    for e in errors:
        print(f"ngspice-manifest: {e}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
