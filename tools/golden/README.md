# Golden capture and replay

These tools implement the golden compatibility suite:

- capability `hefesto-testing-strategy`, requirement "Golden compatibility suite"
- capability `hefesto-output-determinism`, requirement "Canonical comparison against Magic"
- decisions X-1 and X-6, and design D8 of `openspec/changes/m0-foundations`

Golden cases are captured once from the pinned reference tools and committed. CI then replays them through `hefesto-cli`. Replay needs Python 3.11+ and the committed `expected/` directory only. It does not need a container or any reference tool.

```
tools/golden/
  pins.toml              reference-tool pins (single source of truth)
  container/Containerfile pinned headless Magic / netgen / ngspice / xschem image
  capture.py             run the reference tool, write expected/ + provenance.json
  replay.py              run hefesto-cli on each case and compare (run.py = alias)
  compare/               canonical comparators and time-field masks (python3 -m compare)
  goldenlib/             case model, pins, reference runners, hash-table model, external inputs
  authoring/             generator of the M1 cases (m1_cases.py) and the paint/erase table dump
  harness/magic_hash/    driver around Magic's own utils/hash.c (magic-hash-harness)
  tests/                 unit tests (CTest label golden-tools)
```

## Case layout

Each case lives in its own directory, `tests/golden/<AREA>/cases/<case-id>/`. The case ID comes from that area's `CASES.md`, or from a new row added there:

```
case.toml        manifest: capability, requirements, tool, outputs, comparators, masks
input/           tech file, layout, reference script (commands.tcl), Hefesto script (commands.hef)
expected/        captured outputs + provenance.json   (written by capture.py only)
```

The `case.toml` schema is documented at the top of `goldenlib/case.py`.

Pinned data that is never committed (design D1: PDK data, and Magic's generated `scmos.tech`) is declared as `[[external]]` inputs (`goldenlib/external.py`): `source = "pdk"` copies `<pdk-root>/ciel/sky130/versions/<pin of scripts/fetch_pdk.py>/<path>` (root `$HEFESTO_PDK_ROOT` or `.cache/pdks`), `source = "scmos"` copies `$HEFESTO_SCMOS_TECH` or a `scmos.tech` generated once into `.cache/golden/` from `build/ref/magic`, and `source = "magic-sys"` copies `<path>` of the pinned Magic checkout (`$HEFESTO_MAGIC_SRC` or `build/ref/magic`, e.g. `graphics/FreeSans.pt3`) into `<work>/cadroot/magic/sys/`, the files Magic installs in `$CAD_ROOT/magic/sys`. Both tools read the copy in `<work>/input/`. `replay.py` always runs hefesto-cli with `CAD_ROOT=<work>/cadroot`, so a replay never depends on a developer's `CAD_ROOT`; `authoring/casegen.py` adds the `magic-sys` input of each default outline font (FreeSans, FreeSerif, FreeMono) a case's input files name. A `.mag` output gets the `mag-checkpaint` mask unless an input file has a `<< checkpaint >>` section (the case then checks that section is kept). A missing external input fails a capture and skips a replay; `golden-capture.yml` and `ci.yml` fetch these data sets (`scripts/fetch_pdk.py`, cached like `ci.yml`). Each output declares where it comes from (`stdout`, `stderr` or a file path in the work directory), a comparator and the masks to apply.

`provenance.json` records the following:

- the tool and its repository, version and full commit
- the runner (`container`, `local` or `model`)
- the image reference and image ID
- the exact command
- the environment (`TZ=UTC`, `LC_ALL=C`)
- the capture time (`SOURCE_DATE_EPOCH` is honoured)
- the SHA-256 of every output
- `reference_captured`, which is `false` only for the `model` runner

## Building the reference image (CI, not developer machines)

```sh
docker build -f tools/golden/container/Containerfile -t hefesto-golden:local tools/golden
```

The image is reproducible in four ways:

- The base image is pinned by tag and by manifest-list digest.
- APT packages come from `snapshot.debian.org` at the timestamp in `pins.toml`, so every package version is frozen without listing each one.
- Every tool is checked out at a full commit SHA, and the build verifies HEAD after checkout.
- Capture runs with `--network=none`.

`tests/test_pins.py` fails if the `ARG` defaults in the Containerfile drift from `pins.toml`.

| Tool | Pin | How it was resolved |
|---|---|---|
| Magic 8.3.694 | `fc5daa9e2856b89a0a3056f296ded1fd4474e39e` | `git ls-remote`: `refs/tags/8.3.694^{}` |
| netgen 1.5.327 | `93dcda14d36349d0b0ca63168d3cbbab20fe2ece` | `git ls-remote`: `refs/heads/master`. The tag `1.5.327` is a *different* commit and is not used. |
| ngspice | `7aa4716494c01ac1d3120211316e898539a6cb4c` | Not a ref tip. Resolved with `git clone --bare` + `git rev-parse 7aa47164`; it is on branch `pre-master-48`. |
| xschem | `4142e6d6f1eb5cd0e271bb183f14202af374ce27` | Full SHA taken from the specs. Not a ref tip, so `fetch_commit` verifies it at build time. |
| base | `debian:trixie-20261020-slim@sha256:62bd423a…` | Docker Hub registry API |

A short SHA can only be expanded from a clone, because `git ls-remote` lists ref tips only. To expand one, run `git clone --bare <repo> x.git && git -C x.git rev-parse <short>`.

The `pre-master-*` branches of ngspice are rebased. If upstream drops `7aa47164`, the image build fails at checkout instead of silently building something else.

## Capturing

```sh
cd tools/golden
python3 capture.py --check  ../../tests/golden/<AREA>/cases/<id>      # validate only
python3 capture.py --image hefesto-golden:local ../../tests/golden/<AREA>/cases/<id>
python3 capture.py --verify ../../tests/golden/<AREA>/cases/<id>      # re-run, compare, write nothing
```

Runners (`--runner`):

- `container` (default): runs the pinned image; `--image` or `$HEFESTO_GOLDEN_IMAGE`, `--engine docker|podman`. Default tool command lines: `magic -dnull -noconsole [-T tech] script` (the script must end with `quit -noprompt`), `netgen -batch source script`, `ngspice -b script`, `xschem -q -x --no_x --script script` (unverified until the first xschem case), `magic-hash-harness < script`. `[reference] argv` overrides them.
- `local` (magic-hash-harness only): downloads Magic's `utils/hash.c`, `hash.h` and `malloc.h` at the pinned commit, checks them against the SHA-256 values in `pins.toml` (or reads them from `--magic-src <checkout>`), and compiles them with the host `cc -fsigned-char`. This compiles and runs code fetched from the network, so only run it deliberately.
- `model` (magic-hash-harness only): `goldenlib/magic_hash_model.py`, a transcription of the requirement text. Its output is **provisional**: `reference_captured: false`, and replay labels the case "(provisional reference)". Replace it with a `container` or `local` capture as soon as possible; `--verify` shows whether the two agree.

## Replaying

```sh
cd tools/golden
python3 replay.py --hefesto-cli ../../build/macos-debug/src/cli/hefesto-cli            # all captured cases
python3 replay.py --hefesto-cli … --json summary.json --output-dir out ../../tests/golden/CORE/cases/CORE-HASH-01
HEFESTO_CLI=… python3 run.py                                                           # same as replay.py
```

Each case runs in a scratch copy of `input/`, with `TZ=UTC` and `LC_ALL=C`. `{input}` and `{work}` in `[hefesto] args` are replaced with that copy and the work directory. The tool prints one `PASS`/`FAIL`/`SKIP`/`ERROR` line per case and a JSON summary, and exits non-zero on any failure.

`--output-dir` writes `<dir>/<AREA>/<case-id>/<file>`, which is the input format of `scripts/compare_golden_outputs.py` for cross-platform determinism checks.

A case that lists `[hefesto] requires_commands` is **skipped**, not failed, when hefesto-cli rejects one of those commands with `CMD-DISPATCH-001`. CTest reports the skip through exit code 77.

CTest:

```sh
ctest --preset macos-debug -L golden-tools   # comparator, pipeline and pins tests; case validation
ctest --preset macos-debug -L '^golden$'     # one test per captured case (needs hefesto-cli)
```

`-L golden` alone also matches `golden-tools`, because CTest treats labels as regular expressions.

## Comparators and masks

Comparators are canonicalizers, each applied to both sides after masking (`compare/__init__.py`):

- `bytes`: byte for byte. Use it for tile-order and name-keyed hash-order records, which Hefesto reproduces exactly.
- `ext-cap`: `.ext` cap records are reordered so the smaller terminal name comes first and the records are sorted. Every other line, including `merge` groups, is compared byte for byte and in place. The rules are in `compare/ext_cap.py`.

Masks (`compare/masks.py`) fix fields that are documented as time-dependent: `gds-dates` (BGNLIB/BGNSTR dates), `mag-timestamp`, `ext-timestamp`, and `regex:<pattern>` for one-off fields. Two more remove Magic output of features outside M1 (design.md Non-Goals: DRC, extraction, mask output): `magic-tech-sections` drops exactly Magic's cifinput/cifoutput/extract messages printed by `tech load` (each pattern with its Magic source line), and `mag-checkpaint` drops the `<< checkpaint >>` section of a `.mag` file (areas Magic's DRC still has to check). A mask touches only its field, so it cannot hide a layout difference.

## Documented deviations

`tests/golden/deviations.toml` lists the captured cases whose Magic output shows behavior Hefesto deliberately does not reproduce: a Magic bug fixed under decision X-2, or another resolved open question. Each entry names the decisions (headings in `docs/decisions/`), the outputs allowed to differ and the reason (`goldenlib/deviations.py`):

```toml
[TECH-G43]
decisions = ["Q-TECH-25"]
outputs = ["stdout.txt"]
reason = "tech revert * re-locks the layers locked in the technology file"
```

`replay.py` reports such a case as `DEVIATION`, counted like a skip (CTest shows it as skipped). It still fails when another output differs, and it fails when the case passes, so a stale entry is removed. `tests/test_deviations.py` checks that every entry names a captured case, its outputs and existing decisions. A Hefesto bug is never listed: fix it.

To compare two files by hand:

```sh
python3 -m compare expected.ext actual.ext --comparator ext-cap
```

## The M0 case: CORE-HASH-01

Design D8 asks for a first case on requirement "Magic string hash table". That requirement is `exact` and is owned by an M0 capability. Two facts constrain the case:

- No Magic command prints a hash table's raw iteration order without layout support. For example, `cellname list allcells` sorts the names (`database/DBcellname.c:812`).
- M0 Hefesto has no layout database.

So the case uses the options D8 allows:

- **Reference side:** `magic-hash-harness`, which is Magic's own `utils/hash.c` at `fc5daa9e`, compiled unmodified with `-fsigned-char` behind `harness/magic_hash/driver.c`. In the image it is built from the Magic checkout.
- **Hefesto side:** the temporary, M0-only command `hefesto::debug::hashorder <requested-size> (+key|-key)...`. It returns the keys in iteration order as a list, which hefesto-cli prints as one line with the keys separated by spaces.
- **One shared script:** both sides read the same `input/commands.hef`. The keys never need Tcl list quoting, and no table is empty.

**Current status:** `expected/stdout.txt` was produced with the `model` runner, so it is provisional. Compiling Magic's sources was not possible in the session that created the case. The model reproduces every spec scenario (u1 u2 u3; b a; bucket 1 for "ab"). Run `python3 capture.py --runner local --verify ../../tests/golden/CORE/cases/CORE-HASH-01`, or the container runner, to confirm it against Magic's code. Then re-capture with that runner so that provenance says `reference_captured: true`.

## M1 case generator

`authoring/m1_cases.py` writes the generated TECH, MODEL, CELL and viewer-related UI cases from the specs in `authoring/{tech,model,cell,ui}_cases.py` (never `expected/`); `--not-authored` lists the CASES.md rows left out and why (commands hefesto-cli does not have, environment-dependent setups, screenshots). Each command becomes `catch {cmd} result; puts $result` in `commands.tcl` and the same line in `commands.hef`; `.mag` outputs are compared byte for byte with the `mag-timestamp` and `mag-checkpaint` masks, and the console streams with `magic-tech-sections`. The paint/erase table dumps (TECH-G20, G23, G24, G25, G28) paint every type pair on its own 1x1 box and save one `.mag` per table (`authoring/paint_dump.py`, the method TECH/CASES.md asks to record). Edit a spec, re-run the generator, then `capture.py --check`.

## Adding a case

1. Add or pick the row in `tests/golden/<AREA>/CASES.md`.
2. Create `cases/<id>/case.toml` and `input/`, then run `capture.py --check`.
3. Capture with the container runner and commit `expected/`.
4. Re-run CMake, which registers one CTest per captured case.

## M1: cases compared with Magic's console

Most M1 cases (TECH, CELL, MODEL, UI) run a Tcl script in Magic and the equivalent `commands.hef` in `hefesto-cli`, and compare stdout and stderr:

- Every command line of `commands.tcl` is `puts [cmd]` or `catch {cmd} result; puts $result`, so each prints exactly one result line; `commands.hef` has the same commands, one per line, and `quit -noprompt` only in the Tcl script.
- `[hefesto] args` include `--diagnostics=magic`: `hefesto-cli` then prints each script command's result (an empty line for an empty result). As in Magic's Tcl build, a failed command's error is TxError text on stderr and its result stays empty, except for an explicit result (`tech load` failures: `Technology file does not exist or is not readable\n`, `Error in loading technology file\n`) or a Tcl error (unknown command, raised usage error), whose text is the result. It renders diagnostics as Magic's text (`diag::format_magic`: technology-file messages get TechPrintLine's `<file>: line <n>: section <s>:` line; informational messages go to stdout like TxPrintf, the rest to stderr like TxError).
- Both streams use the `magic-console` comparator, which removes Magic's leading startup preamble (banner, NULL graphics, startup files) and compares everything else byte for byte. The patterns, each with its Magic source line, are in `compare/magic_console.py`; extend them only when a capture shows another preamble message.

Capture in CI (`.github/workflows/golden-capture.yml`):

- `capture.py --only-captured` / `--only-missing` keep the cases that have, or lack, `expected/provenance.json`; an empty selection succeeds.
- Pull requests that add or edit a `case.toml` or `input/` run both steps: a fresh capture of the captured cases must match `expected/` (verify), and the cases without `expected/` are captured and uploaded as the `golden-expected` artifact (paths under `tests/golden/`), to review and commit. `SOURCE_DATE_EPOCH` is the commit time, so provenance is reproducible.
- `workflow_dispatch` takes `mode` (`both`, `verify`, `capture`) and an optional list of case directories; the weekly schedule verifies only.
