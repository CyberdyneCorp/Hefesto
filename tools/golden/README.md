# Golden capture and replay

These tools implement the golden compatibility suite:

- capability `hefesto-testing-strategy`, requirement "Golden compatibility suite"
- capability `hefesto-output-determinism`, requirement "Canonical comparison against Magic"
- decisions X-1 and X-6, and design D8 of `openspec/changes/m0-foundations`

Golden cases are captured once from the pinned reference tools and committed. CI then replays them through `hefesto-cli`. Replay needs Python 3.11+ and the committed `expected/` directory only. It does not need a container or any reference tool.

```
tools/golden/
  pins.toml              reference-tool pins (single source of truth)
  container/Containerfile pinned headless Magic / netgen / ngspice / xschem / KLayout / gdstk image
  capture.py             run the reference tool, write expected/ + provenance.json
  replay.py              run hefesto-cli on each case and compare (run.py = alias)
  ext_inputs.py          M6: run the .ext probe in the image (golden-capture.yml mode ext-inputs)
  oasis_validate.py      M6: check Hefesto's OASIS files with KLayout and gdstk (mode oasis-validate)
  compare/               canonical comparators and time-field masks (python3 -m compare)
  goldenlib/             case model, pins, reference runners, hash-table model, external inputs, GDS and OASIS decoders
  authoring/             generators of the M1-M6 cases (m1_cases.py ... m6_cases.py; M6 OASIS: oasis_cases.py), the paint/erase table dump, the GDS writer gdsgen.py, the OASIS writer oasgen.py and the GDS equivalent of an OASIS file oasequiv.py
  harness/magic_hash/    driver around Magic's own utils/hash.c (magic-hash-harness)
  harness/ext_probe/     M6: the .ext probe Magic runs (extract all)
  harness/oasis_validate/ M6: the KLayout and gdstk scripts of oasis_validate.py
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

Pinned data that is never committed (design D1: PDK data, and Magic's generated `scmos.tech`) is declared as `[[external]]` inputs (`goldenlib/external.py`): `source = "pdk"` copies `<pdk-root>/ciel/<family>/versions/<pin>/<path>` of the data set named by `set` (`sky130`, the default, `gf180mcu` or `ihp`, or one of the M5 library GDS sets `sky130-gds`, `gf180mcu-gds`, `ihp-gds` and their standard-cell subsets `*-gds-sc`, which live under `<pdk-root>/gds`; family and pin from `DATA_SETS` of `scripts/fetch_pdk.py`, m3-drc design D15 and m5-mask-gds-cif D19; root `$HEFESTO_PDK_ROOT` or `.cache/pdks`), `source = "scmos"` copies `$HEFESTO_SCMOS_TECH` or a `scmos.tech` generated once into `.cache/golden/` from `build/ref/magic`, and `source = "magic-sys"` copies `<path>` of the pinned Magic checkout (`$HEFESTO_MAGIC_SRC` or `build/ref/magic`, e.g. `graphics/FreeSans.pt3`) into `<work>/cadroot/magic/sys/`, the files Magic installs in `$CAD_ROOT/magic/sys`, and `source = "magic-tcl"` copies such a file (e.g. `tcltk/strip_reflibs.tcl`) into `<work>/cadroot/magic/tcl/`, where Magic installs its auxiliary Tcl scripts, and `source = "golden"` copies `tests/golden/<path>`, a committed file several cases share (M6: the captured sky130 OASIS file of OASIS-020/021). Both tools read the copy in `<work>/input/`. `replay.py` always runs hefesto-cli with `CAD_ROOT=<work>/cadroot`, so a replay never depends on a developer's `CAD_ROOT`; `authoring/casegen.py` adds the `magic-sys` input of each default outline font (FreeSans, FreeSerif, FreeMono) a case's input files name. A `.mag` output gets the `mag-checkpaint` mask unless an input file has a `<< checkpaint >>` section (the case then checks that section is kept). A missing external input fails a capture and skips a replay; `golden-capture.yml` and `ci.yml` fetch these data sets (`scripts/fetch_pdk.py`, cached like `ci.yml`). Each output declares where it comes from (`stdout`, `stderr`, a file path in the work directory, or a derived source `derived:<reducer>:<glob>` built from several files of the work directory, `goldenlib/derived.py`: `mag-drc-sections` keeps the `checkpaint`/`error_*` sections of every matching `.mag` file, `digest` one SHA-256 line per file), a comparator and the masks to apply. A manifest with `slow = true` (the standard-cell corpus cases of m3-drc design D14) is registered with the CTest label `golden-slow` instead of `golden`.

`provenance.json` records the following:

- the tool and its repository, version and full commit
- the runner (`container`, `local` or `model`)
- the image reference and image ID
- the exact command
- the environment (`TZ=UTC`, `LC_ALL=C`)
- the capture time (`SOURCE_DATE_EPOCH` is honoured)
- the SHA-256 of every output
- `reference_captured`, which is `false` only for the `model` runner

## M4: cases run by both tools from one Tcl script

From M4 hefesto-cli embeds Tcl, so a case can have a single `input/commands.tcl` that Magic and `hefesto-cli --no-log --diagnostics=magic [--trust-pdk <pdk>] --magic <Magic options>` both run (`authoring/tclcase.py`, driver `authoring/m4_cases.py`; m4-scripting design D16). The manifest supports:

- `[env]`: environment of both runs; `{work}` is the work directory (`/work` in the container). Replay writes its own work directory as `/work` in every output, so absolute paths compare.
- `[reference] display = "xvfb"`: Magic's Tk wrapper under `xvfb-run` (the container gets `--init`; xvfb-run as PID 1 never returns).
- `[reference] stdin` / `[hefesto] stdin`: a file fed on standard input.
- `[reference] allow_nonzero_exit = true`: keep the output of a run killed by `timeout` (SCRIPT-API-11, Q-SCRIPT-3).
- `[[external]]` `to` may be a relative path and a `pdk` `path` may be a directory (a PDK tree under `input/pdk` for `PDK_ROOT`).
- `derived:files:<glob>`: every matching file preceded by `file <path>` (cells named only at run time).
- `derived:gds-digest:<glob>` and `derived:gds-summary:<glob>`: a GDS output too large to commit, as the SHA-256 of its decompressed, `gds-mod-dates`-masked bytes and as its decoded summary (the `MASK-RT-*` cases).
- `[reference] timeout_s` / `[hefesto] timeout_s`: seconds before `capture.py` (default 600) or `replay.py` (default 300) abandons the run, at most 6 hours.

## M5: the use-order probe (MASK-UO-*)

`harness/use_order/use_order_probe.tcl` records the order in which Magic enumerates a cell's uses (m5-mask-gds-cif task 1.5, design D7, Q-MASK-204). It does this after controlled placements, area searches, exact-match lookups, removals and child bounding-box changes, and after plain fresh-session GDS and CIF writes. INDEX's trace test (task 5.1) replays the result. The probe is not built into the image. `authoring/use_order_cases.py` copies it into the `input/` of the 12 cases `tests/golden/MASK/cases/MASK-UO-*`, so capture (Magic in the pinned image, `--network=none`) and replay (`hefesto-cli --magic`) run the same file. Each case's compared output is `trace.txt`, whose format is in `tests/data/db/use_index/README.md`.

```sh
python3 authoring/use_order_cases.py              # write the cases and tests/data/db/use_index/*.tcl
python3 authoring/use_order_cases.py --dispatch   # the golden-capture.yml dispatch command (mode capture)
python3 authoring/use_order_cases.py --sync       # copy captured expected/trace.txt to tests/data/db/use_index/
```

`tests/test_use_order_probe.py` runs the probe's GDS and CIF decoders on hand-built files. It also runs every case script under `tclsh` against `tests/fake_magic_useorder.tcl`, a stand-in for the Magic commands with no bplane model. Both are skipped when `tclsh` is missing.

## M5: the MASK cases (mask generation, GDS and CIF)

`authoring/m5_cases.py` writes the MASK cases of `tests/golden/MASK/CASES.md` (m5-mask-gds-cif design D18, task 13.4) from `authoring/mask_cases.py` (framework; output styles MASK-OS, operators MASK-OP, hierarchical generation MASK-HG) and `authoring/mask_io_cases.py` (input styles MASK-CI, CIF MASK-CF, GDS read MASK-GR, GDS write MASK-GW). Each case is a `TclCase` on a small synthetic technology (the MODEL technology `mt` with the family's cifoutput and cifinput sections, committed per case), with Magic's session state pinned: a fresh session, `drc off` first, `gds datestamp 0` before a GDS write (creation dates are then compared under the image's `TZ=UTC`; `gds-mod-dates` masks only the modification dates) and one write per case, unless the case is a *history case*, whose `case.toml` names its writes. A read case saves every cell it created (`writeall force`) and compares them as one file (`derived:files:*.mag`). Outputs are compared by kind: GDS byte for byte (display `gds-records`), compressed GDS through `gzip`, CIF with `cif-preamble`, `feedback save` files with `polygon-set`.

```sh
python3 authoring/m5_cases.py                 # write the cases (never expected/)
python3 authoring/m5_cases.py --not-authored  # rows left out: reason and target milestone
python3 authoring/m5_cases.py --deviations    # rewrite the generated deferral block of deviations.toml
python3 authoring/m5_cases.py --dispatch      # the golden-capture.yml capture command
python3 authoring/m5_cases.py --dispatch-uncaptured  # the same, for the cases without expected/
```

The driver also writes the M5 cases outside the 233 MASK rows (task 13.6, 13.7): the rows of other areas that waited for GDS (`m5_specs()` of `misc_cases.py` and `cell_cases.py`: MISC-MH-03..06, MISC-FL-05, CELL-TS-07, on the same framework; CELL-TS-07 without `gds datestamp 0`, which would replace the content-hash stamps it checks), and the cases on the pinned PDKs of `authoring/mask_pdk_cases.py`:

- `MASK-RT-<lib>` and `MASK-RT-<lib>-fNN`, the library round trips of design D19, one fresh session per library GDS file (89). Each copies `harness/gds_rt/gds_rt.tcl` into its `input/` and stages the library's `libs.ref/<lib>/gds` directory as `input/lib` from the GDS data set; `rt::run_library` picks the file by its index in sorted order (and checks the file count), reads it (an `*_ef_io*` file reads the library's `*_fd_io` file first), places every new top structure in `mask_rt_wrapper` (sorted by name, at the origin), pins the wrapper's timestamp to 0 and writes with `gds library yes`. `out.gds` is compared through `derived:gds-digest:out.gds` (SHA-256 of the decompressed, `gds-mod-dates`-masked stream) and `derived:gds-summary:out.gds` (`goldenlib/gds.py` `summary`: library name and creation date, one line per structure with its creation date and record count). The SRAM libraries set `timeout_s` in `[reference]` and `[hefesto]` (the runners' defaults are 600 and 300 seconds). `tests/test_gds_rt.py` runs the harness under `tclsh` against `tests/fake_magic_gds_rt.tcl`.
- `MASK-PROBE-<variant>`: each output style's CIF layer list (`cif see` of an unknown layer prints it) and each input style's `Input style` line, for the PDK unit tests of tasks 3.7 and 7.4.

The round trips' review tool is `gds_rt_report.py` (design D19's equivalence report): for each `MASK-RT-*` case it runs the case with hefesto-cli, reads `out.gds` back in a fresh session with the same round trip (`rt::roundtrip out.gds out2`) and lists the order-insensitive differences between the two writes (structures, SREF/AREF multisets, each layer's area as canonical rectangles, other polygons as normalized rings) in Markdown and the job summary. It never fails on differences (`--strict` fails when a pass writes nothing); `perf.yml`'s `gds-roundtrip` job runs it.

```sh
python3 gds_rt_report.py --hefesto-cli ../../build/macos-debug/src/cli/hefesto-cli \
  ../../tests/golden/MASK/cases/MASK-RT-sky130_fd_sc_hd
```

A MASK-RT difference is bisected from the raw outputs, which expected/ does not keep (only the digest and the summary). `capture.py` and `replay.py` take `--keep-raw DIR` (`goldenlib/rawkeep.py`): each run's `stdout.txt`, `stderr.txt` and every file it wrote outside `input/` go to `DIR/<case-id>/`, a GDS file gzipped (`files/out.gds.gz`). A file over 256 MiB as stored, or one that would take the files kept under `DIR` past 2 GiB, is not kept; `skipped.txt` names it. `--feedback-probe` also inserts `catch {feedback save out.feedback.tcl}` before the work copy's last `quit -noprompt`, so the run leaves its feedback list (the CIF errors with cell, layer and area, e.g. "parent and child disagree on CIF"); `feedback save` prints nothing, so a `--verify` run still compares. `--probe-tcl FILE` runs FILE's Tcl at the same place (after the feedback save), e.g. a `save` of the cells under suspicion so that the two tools' reads can be compared as .mag files, or a `cif see` and `feedback save` of one layer; its output reaches the streams. Both probes change the run, so `capture.py` accepts them only with `--verify`. `raw_diff.py MAGIC_DIR HEFESTO_DIR` (which reads `out.gds` or `out.gds.gz`) prints the first position where the structure order differs, each structure's first differing element in write order, the order-insensitive differences of `gds_rt_report.py` and the feedback entries only one side has, grouped by cell, layer and message. In CI, dispatch `golden-capture.yml` with `keep-raw` (and `feedback-probe`, `probe-tcl`) on: each `capture-gds-rt` shard uploads `golden-raw-gds-rt-<shard>` (the shard then runs under its `timeout_raw`, `scripts/gds_rt_shards.py`, and only the verify step runs the probes) with `magic/<case-id>/`, `hefesto/<case-id>/` (hefesto-cli built in the job) and `<case-id>.diff.txt`:

```sh
gh workflow run golden-capture.yml --ref <branch> -f mode=verify -f keep-raw=true -f feedback-probe=true \
  -f cases="tests/golden/MASK/cases/MASK-RT-sky130_fd_io-f01"
```

Hand-built GDS inputs (bad headers, damaged records, every PATHTYPE, AREFs, properties, odd TEXT) come from `authoring/gdsgen.py`, a record writer with its decoder in `goldenlib/gds.py` (`tests/test_gdsgen.py`). Hand-built OASIS inputs (M6, m6-lef-def-oasis design D19: every real form, non-minimal integers, every repetition and point-list type, forward references, CBLOCKs that split a record, bad signatures) come from `authoring/oasgen.py`, a clean-room record writer built from `openspec/specs/oasis-format/spec.md`; `Builder` tracks the file offsets for table offsets and `S_CELL_OFFSET` and writes the 256-byte END with its signature, and `reference_419()` rebuilds the spec's 419-byte reference file (`tests/test_oasgen.py` checks it byte for byte against the listing, CRC32 `0xEE7E3BD3`). Its decoder is `goldenlib/oasis.py`, also clean-room (`tests/test_oasis.py`, the spec's scenarios; `python3 -m goldenlib.oasis FILE` prints a file's records with their offsets). Inputs under 64 KB are committed as binary. Until a case is captured and its workstream lands, `deviations.toml` lists it as `deferred = "M5"` in the block `m5_cases.py --deviations` generates; a workstream whose implementation makes cases replay adds them to `REPLAYED` in `m5_cases.py` and re-runs `--deviations`.

## M6: the LEFDEF cases

`authoring/m6_cases.py` writes the 143 rows of `tests/golden/LEFDEF/CASES.md` and the library round trips `LEFDEF-RT-<lib>` (m6-lef-def-oasis design D19, tasks 15.1 and 15.5): the framework is `authoring/lefdef_cases.py` (synthetic technology `lefdef.tech` per case with a `lef` section, DRC rules, an output style of 10 nm per lambda and an extract style; LEF/DEF text builders; the session pinning `drc off` and `lef datestamp 100`), the rows are in `lefdef_lef_cases.py`, `lefdef_def_cases.py` and `lefdef_pdk_cases.py`. LEF files are compared with `lef-text`, DEF files with `def-text`, the console with `lef-text`, saved cells as one `derived:files:**/*.mag` output. The round trips read the LEF sets of `scripts/fetch_pdk.py` and compare their written LEF through `derived:digest` and `derived:lef-summary` (one `macro <name> <lines> <sha256/16>` line per macro, so a differing digest is located). `--deviations` keeps every case `deferred = "M6"` (owner workstream in the reason) until its workstream adds it to `REPLAYED`; `--exit-check` fails while an M6 deferral or an open M6 decision is left. A blocker Magic cannot capture yet is written with `capture_blocked` (below), so capture skips it. The DEF-write net cases are captured after the `ext-inputs` sync below (`--dispatch` lists them last).

```sh
python3 authoring/m6_cases.py && python3 authoring/m6_cases.py --deviations
python3 authoring/m6_cases.py --dispatch      # the golden-capture.yml capture command
```

## M6: the `.ext` probe (mode `ext-inputs`)

The DEF-write net cases (LEFDEF-317..328 and 330..334) read `.ext` files, which Hefesto cannot write before M7 (m6-lef-def-oasis task 1.6, Open Question 1 (a)). Magic writes them: `authoring/lefdef_cases.py --ext-plan FILE` lists the targets (every authored DEF-write net case whose `input/` has one `*.tech` file and whose `commands.tcl` loads the root cell, plus the smoke layout `tests/data/extflat/smoke/input/`), and `ext_inputs.py` runs `harness/ext_probe/ext_probe.tcl` (`extract all` into `ext/`) on each in the image. `goldenlib/extfile.py` checks every `.ext` file (ASCII, the six header records first, whole records only, no `error` line) and the probe's log (it finished, `extract all` returned 0, no feedback, no error line). `--sync-ext` copies the checked files into each case's `input/` and into `tests/data/extflat/<target>/` (format in its README.md).

```sh
gh workflow run golden-capture.yml --ref <branch> -f mode=ext-inputs   # artifact ext-inputs
python3 authoring/lefdef_cases.py --sync-ext <artifact dir>
```

`tests/test_ext_inputs.py` runs the probe under `tclsh` against a stand-in for the Magic commands, and the runner and the sync on hand-made files.

## M6: OASIS validation with KLayout and gdstk (mode `oasis-validate`)

The image has KLayout and gdstk as reference tools of the OASIS cases (m6-lef-def-oasis design D18). KLayout is the Debian package of the APT snapshot, which has no strm2gds/strmxor/strmcmp programs: their operations run through KLayout's scripting API in `harness/oasis_validate/klayout_check.py` (`klayout -b`). gdstk is a pinned wheel in a virtual environment (`$HEFESTO_GDSTK_PYTHON`, `harness/oasis_validate/gdstk_check.py`). Both run as separate programs only; Hefesto never links or calls them.

`oasis_validate.py produce` runs hefesto-cli (built in the job) on the cases of rows OASIS-050..063 (a row's cases are `OASIS-0NN` and `OASIS-0NN-<layout>`) and collects their `.oas` outputs, plus the committed `tests/data/oasis/codec/reference_419.oas`. Each file gets the default checks, the expectations of its output in `OUTPUT_CHECKS` (OASIS-050's 419 bytes, OASIS-057's strict and non-strict reads, OASIS-058's signatures, OASIS-061's strict read), and, once the write cross-check is captured, a KLayout XOR against Magic's GDS in the case's `expected/`. `INPUT_CHECKS` hands committed read-case inputs to the readers (task 15.6, readers only): OASIS-013's 419-byte file (LayoutDiff against its GDS equivalent) and OASIS-030's trapezoids (XOR against the GDS equivalent Magic reads). The captured-input plan of `authoring/oasis_captured_cases.py` goes into the manifest too: `check` has gdstk write OASIS-012's validation set and KLayout convert the sky130 standard-cell GDS (`strm2oas`, then `strm2gds` of the result, gzip-compressed) into `captured/<case>/`, which `authoring/oasis_cases.py --sync-captured <artifact dir>` commits under `tests/golden/OASIS/captured/` with `captured.json` (tool versions, SHA-256). The PDK source is copied into the artifact by `produce`; without the `sky130-gds-sc` set the step is skipped. A case not authored yet, or a run that stops on `CMD-DISPATCH-001` or `CORE-NOTIMPL-*`, is skipped with its reason. `oasis_validate.py check`, in the image, makes the smoke files (gdstk: GDS and OASIS with CRC32 and CHECKSUM32, and a copy with one byte changed; KLayout: strict and non-strict strm2oas of the GDS) and then checks every file: KLayout read with `expect_strict_mode` and no warning, strm2gds, strmxor (empty on every layer), strmcmp; gdstk `read_oas` and `oas_validate`; the 419-byte size. Two negative controls must fail: a strict file read as non-strict must warn, and the changed copy must fail `oas_validate`. The verdicts and the tool versions go to the job summary and the artifact `oasis-validate` (`verdicts.json`, `summary.md`, the files).

```sh
gh workflow run golden-capture.yml --ref <branch> -f mode=oasis-validate
python3 oasis_validate.py check --dir out --klayout-python <py> --gdstk-python <py>   # local, with pip's klayout and gdstk
```

## M6: the OASIS cases

`authoring/oasis_cases.py` (the OASIS section of the M6 driver `authoring/m6_cases.py`, with the families `oasis_format_cases.py`, `oasis_read_cases.py`, `oasis_write_cases.py` and `oasis_captured_cases.py` on the framework `oasis_common.py`) writes the rows of `tests/golden/OASIS/CASES.md` (m6-lef-def-oasis tasks 15.2 and 15.6). Magic has no OASIS, so the reference of each case is one of:

- a **read cross-check**: Magic reads `<stem>.gds`, the GDS equivalent of the hand-built `<stem>.oas` as oasis-read defines it (`authoring/oasequiv.py`: one GDS element per OASIS element instance, regular placement repetitions as AREFs, circles as the spec's vertex polygon, files whose decoding stops reduced to their complete cells), and Hefesto reads `<stem>.oas` with its own script (`input/hefesto.tcl`, `tclcase.TclCase.hefesto_body`). Both pin the read datestamp, save every cell and append the same queries to `report.txt`; those are compared, the console is not (`streams=False`). A report line that only an OASIS requirement fixes (`OASIS_FILE`, `oasis compress`) is written by Magic's script as the spec value (`specrep`);
- a **write cross-check**: both build the layout; Magic writes `out.oas` with `gds write` (GDS bytes), Hefesto with `oasis write`; compared with `stream-geometry`, `stream-hierarchy` and, for `repetitions no` (OASIS-052), `stream-elements`;
- **Hefesto self-golden** (reference tool `oasis-spec`): the expected bytes are the spec listings, assembled by `oasgen.py` (`oasis_write_cases.model_file`: file layout, name tables, modal omission and repetition grouping for one-cell rectangle layouts) into `input/spec/`; the reference run copies them (`goldenlib/refrunners.py`, any runner). `tests/test_oasis_cases.py` checks them against the listings, and that the committed case directories are what the scripts write.

`--not-authored` lists the rows not written (the captured-input rows until their sync, OASIS-092 with its reason); `--deviations` lists the approved deviations recorded in case notes and CASES.md rows, not in `deviations.toml` (OASIS-012, 030, 061: Q-OASIS-2, no commercial OASIS files). `m6_cases.py --exit-check` prints them with every M6 deferral of `deviations.toml` and every unwritten row, and fails while a deferral or an unwritten row is left; `m6_cases.py --deviations` writes the `deferred = "M6"` block for captured cases whose workstream has not landed.

```sh
python3 authoring/m6_cases.py                       # write the cases
python3 authoring/m6_cases.py --dispatch            # the capture dispatch
gh workflow run golden-capture.yml --ref <branch> -f mode=oasis-validate   # captured inputs and the tools' verdicts
python3 authoring/oasis_cases.py --sync-captured <artifact dir> && python3 authoring/m6_cases.py
```

## M8: the SCHEM cases (xschem)

`authoring/schem_cases.py` writes the 41 cases of `tests/golden/SCHEM/CASES.md` (m8-schematic tasks 3.2 and 3.3, design D16), captured from xschem 3.4.8RC in the pinned image. The image installs `gawk` and makes it `awk`; its build fails when `awk --version` is not GNU Awk (task 3.1, Q-SCHEM-18), and `golden-capture.yml` logs the version.

- **Clean room (J-1, design D2).** Inputs are PDK files or Hefesto-authored files written from the `schematic-*` specs; each `case.toml` names its `provenance` (`"hefesto"` or `"pdk:<set>:<path>"`), which `scripts/check_cleanroom.py` checks. xschem's system symbols (`res.sym`, `lab_pin.sym`, ...) are referenced by name and come from the image; no file of xschem's distribution is committed.
- **PDK data.** `[reference] xschem_sets` names the xschem data sets of `scripts/fetch_pdk.py` a case reads. The container runner mounts each one read-only at `/pdk/<variant>` with `PDK_ROOT=/pdk` (and `PDK=<variant>` for a single set), so PDK files are read in place and each PDK's own `xschemrc` finds its tree. `replay.py` links the same sets as `<work>/pdk/<variant>`. A missing set fails a capture and skips a replay.
- **rc fragment.** `harness/schem/xschemrc` is copied into every case as `input/xschemrc` and passed with `--rcfile`: it sources the PDK's `xschemrc` (or uses xschem's system symbol folder), appends the case's input folder to the library path, sources `input/case.xschemrc` when present, sets the netlist folder to `<work>/out` and turns `xschem_execute_scripts` on, so `tcleval` values are evaluated.
- **Drivers.** Netlist cases run `input/run.sh`: one fresh `xschem -n -s` per netlist, its exit status printed after it. Tcl cases run `xschem --script input/case.tcl`. Every case captures its files, stdout, stderr and the exit status (output source `exitcode`), with `allow_nonzero_exit` (a netlist with a short exits 10).
- **Comparison.** Netlists and streams use the `schem-paths` mask (Q-SCHEM-13); `schem-version` masks the writer version of a version record (Q-SCHEM-1). The flat netlist of SCHEM-NL-21 is compared by `spice-canonical` (`compare/spice_canonical.py`, design D10): devices with subcircuits expanded, matched through a node bijection, names compared case-insensitively.
- **Replay.** Hefesto runs `input/hefesto.hef` (the `schematic` command) or `input/case.tcl` (the `xschem` compatibility command); `deviations.toml` defers every SCHEM case to M8 until its workstream replays it (`schem_cases.REPLAYED`, then `--deviations`).

```sh
python3 authoring/schem_cases.py               # write the cases (never expected/)
python3 authoring/schem_cases.py --deviations  # rewrite the SCHEM block of deviations.toml
python3 authoring/schem_cases.py --dispatch    # the golden-capture.yml capture command
```

## M10: the 3D-view cases and the `magic-gl` image (MISC-3D-*)

Magic's 3D window (`specialopen wind3d`) exists only in a Magic built with OpenGL and running on the OpenGL display (`-d OGL`); the default image's Magic has neither. Under m10-3d-view design D13 (Open Question 3, decided (a)), the `MISC-3D-*` cases run in the **`magic-gl` image variant**. This is the same Containerfile and the same pins built with `--build-arg WITH_OPENGL=1`. Its last step installs Mesa from the APT snapshot (`libgl-dev`, `libglu1-mesa-dev`, `libgl1-mesa-dri`, `libglx-mesa0`, `mesa-utils`), then builds the pinned Magic commit again with `--with-opengl --with-cairo=no` and installs it over the plain build. That build fails in four cases: `-DOGL`/`-DTHREE_D` are missing from `defs.mak` (configure drops OpenGL silently when the headers are missing), `glxinfo -B` under Xvfb does not report llvmpipe, `container/magic_gl_smoke.tcl` cannot open a 3D window the way Magic's wrapper does (`toplevel .render`, then `specialopen wind3d <cell> .render.magic`, as `magic::render3d`), or a rotation in it does not answer `.render.magic view`. The `WITH_OPENGL` ARG is declared after every other step, so the two images share every layer before it. `pins.toml` lists the variant under `[image_variants.magic-gl]`, and `tests/test_pins.py` checks that its build argument defaults to the plain image.

A case selects the variant with `[reference] image = "magic-gl"`, which `goldenlib/case.py` accepts only for tool `magic` with `display = "xvfb"`. `capture.py` runs such a case in the image named by `--gl-image` (`$HEFESTO_GOLDEN_GL_IMAGE`). Without that option it prints `skipped, needs the magic-gl image` and does not fail, so a run that builds only the default image stays green. The container runner wraps the run in `xvfb-run -s "-screen 0 1280x1024x24 +extension GLX"` and sets `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe`.

`authoring/m10_cases.py` writes MISC-3D-01..04 (`TclCase(gl=True)`: Magic's Tk wrapper on `-d OGL -noconsole -norc`, and hefesto-cli with the same options). Each case gets the technology `input/v3d.tech` (`render MET1 metal1 1.376 0.36` in an output style of scalefactor 10), the 100 x 50 cell `input/c3d.mag`, and the OpenGL display-style, colour-map and glyph files as `magic-sys` inputs for the replay's CAD_ROOT. They compare stdout and stderr only (command results and error texts); the geometry comparison of D13 is the GL shim of task 12.4. `deviations.toml` defers them to M10 until WIND3D's client replays them (`m10_cases.REPLAYED`, then `--deviations`).

```sh
docker build -f tools/golden/container/Containerfile --build-arg WITH_OPENGL=1 -t hefesto-golden-gl:local tools/golden
python3 capture.py --gl-image hefesto-golden-gl:local ../../tests/golden/MISC/cases/MISC-3D-01
python3 authoring/m10_cases.py                # write the cases (never expected/)
python3 authoring/m10_cases.py --deviations   # rewrite the M10 block of deviations.toml
python3 authoring/m10_cases.py --dispatch     # the golden-capture.yml capture command
```

## Building the reference image (CI, not developer machines)

```sh
docker build -f tools/golden/container/Containerfile -t hefesto-golden:local tools/golden
docker build -f tools/golden/container/Containerfile --build-arg WITH_OPENGL=1 -t hefesto-golden-gl:local tools/golden  # magic-gl (M10)
```

The image is reproducible in four ways:

- The base image is pinned by tag and by manifest-list digest.
- APT packages come from `snapshot.debian.org` at the timestamp in `pins.toml`, so every package version is frozen without listing each one.
- Every built tool is checked out at a full commit SHA, and the build verifies HEAD after checkout. The installed tools (KLayout's `.deb`, gdstk's wheel) are checked against a SHA-256.
- Capture runs with `--network=none`.

`tests/test_pins.py` fails if the `ARG` defaults in the Containerfile drift from `pins.toml`.

| Tool | Pin | How it was resolved |
|---|---|---|
| Magic 8.3.694 | `fc5daa9e2856b89a0a3056f296ded1fd4474e39e` | `git ls-remote`: `refs/tags/8.3.694^{}` |
| netgen 1.5.327 | `93dcda14d36349d0b0ca63168d3cbbab20fe2ece` | `git ls-remote`: `refs/heads/master`. The tag `1.5.327` is a *different* commit and is not used. |
| ngspice | `7aa4716494c01ac1d3120211316e898539a6cb4c` | Not a ref tip. Resolved with `git clone --bare` + `git rev-parse 7aa47164`; it is on branch `pre-master-48`. |
| xschem | `4142e6d6f1eb5cd0e271bb183f14202af374ce27` | Full SHA taken from the specs. Not a ref tip, so `fetch_commit` verifies it at build time. |
| base | `debian:trixie-20261020-slim@sha256:62bd423a…` | Docker Hub registry API |
| KLayout 0.30.0 | Debian `klayout 0.30.0-1+b1`, `.deb` SHA-256 `421e473f…` | The Packages index of trixie/main amd64 at the APT snapshot. KLayout publishes no Debian 13 `.deb`. |
| gdstk 1.0.2 | `gdstk-1.0.2-cp313-cp313-manylinux_2_24_x86_64.manylinux_2_28_x86_64.whl`, SHA-256 `0429cc3e…` | PyPI JSON API; Debian has no gdstk package. numpy is the snapshot's `python3-numpy` 2.2.4. |

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

- `container` (default): runs the pinned image; `--image` or `$HEFESTO_GOLDEN_IMAGE`, `--engine docker|podman`. Default tool command lines: `magic -dnull -noconsole [-T tech] script` (the script must end with `quit -noprompt`), `netgen -batch source script`, `ngspice -b script`, `xschem -q -x --no_x --script script` (unverified until the first xschem case), `magic-hash-harness < script`. `[reference] argv` overrides them. Tool `oasis-spec` (M6) runs under every runner: it copies `input/spec/` into the work directory (`cp -R input/spec/. .`).
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

`--output-dir` writes `<dir>/<AREA>/<case-id>/<file>`, which is the input format of `scripts/compare_golden_outputs.py` for cross-platform determinism checks. Cases with a `deferred` entry in `deviations.toml` are left out: their feature is missing, so their output can depend on the run (MASK-GR-052 prints the save time of cells its `gds read` should have stamped).

A manifest with `gds_rt = true` (the library GDS round trips `MASK-RT-*` of m5-mask-gds-cif design D19) gets the CTest label `golden-gds-rt`. `replay.py` leaves such cases out of its default selection and skips one named on the command line, unless `--include-gds-rt` is passed (its CTest passes it; `perf.yml`'s `gds-roundtrip` job runs them). `--require-inputs ID[,ID...]` (repeatable) turns a missing `[[external]]` input of the listed cases from a skip into a failure: `ci.yml`'s golden replay lists the three standard-cell `MASK-RT-*` cases, whose GDS subsets every golden leg fetches. Every listed ID must name a case directory under `--root`.

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
- `polygon-set`: runs of `box x1 y1 x2 y2` records (with the `feedback add` line `feedback save` writes after each) or `rect` lines are compared as the area each text covers, in a canonical maximal-strip decomposition, so `cif see` feedback compares whatever tiles produced it (m5-mask-gds-cif design D18). The rules are in `compare/polygon_set.py`.
- `gzip`: compressed output (`gds compress`, `.gds.gz`) is decompressed, every member, *before* the masks, then compared byte for byte (Q-MASK-223: the gzip header and deflate choices are informative). `replay.py --output-dir` saves such an output decompressed and masked.
- `oasis-records` (M6): OASIS files compared as their decoded records (`goldenlib/oasis.py` `dump`: one line per record with its fields as written, CBLOCK contents decompressed and indented under their CBLOCK, then the stored and computed validation signature, warnings and any decoding error). Also a display view for OASIS outputs compared with `bytes`.
- `stream-hierarchy` (M6): a GDSII or OASIS stream compared cell by cell without flattening: cell names, each cell's own geometry (as `stream-geometry` canonicalizes it) and texts, and one line per placed instance (AREFs and PLACEMENT repetitions expanded) with its transform and reference properties (PROPATTR/PROPVALUE, `S_GDS_PROPERTY`). The rules are in `compare/stream_hierarchy.py`.
- `stream-elements` (M6): a GDSII or OASIS stream compared as each cell's element sequence in record order (rectangles, polygons from their first vertex, paths, texts, placements with their properties, arrays as their sorted displacements); an OASIS shape repetition shows as such. The rules are in `compare/stream_elements.py`.
- `stream-geometry` (M6): a GDSII or OASIS stream, either format on either side, compared as flat per-layer geometry (m6-lef-def-oasis design D19): top cells flattened with every placement, AREF and repetition; Manhattan areas merged into the canonical form of `polygon-set` (so record order, fracturing and overlaps do not count); other polygons as a canonical polygon list; OASIS circles as `circle` lines; texts by layer, position and string; the database unit compared. Paths become their outlines. The rules are in `compare/stream_geometry.py`; arbitrary-angle cases also run KLayout `strmxor` in `oasis-validate`.
- `lef-text`, `def-text` (M6): LEF and DEF files and the console of `lef`/`def` commands, byte for byte after removing the informative progress lines (`Diagnostic:` lines, `(This can take a while!)`, LefEstimate's `Processed <n> of <m> ... (<p>%).` lines) and Magic's leading startup preamble as `magic-console` does. The rules are in `compare/lefdef_text.py`.
- `drc-console`: `magic-console`, then `drc list(all) count` pairs sorted and each run of `drc count` cell lines sorted (Magic: address-keyed hash order; Hefesto: name order, m3-drc design D12). A `drc listall why` result is compared as printed: Hefesto reproduces Magic's string-hash order of the why texts (Q-SCRIPT-8; M3 sorted them here). The rules are in `compare/drc_console.py`.

An output may name a *display view*, `display = "gds-records"` or `display = "oasis-records"`, which only renders a difference readably (GDSII decoded one record per line, `goldenlib/gds.py`; OASIS, `goldenlib/oasis.py`); pass or fail is always the comparator's (`python3 -m compare a.gds b.gds --mask gds-mod-dates --display gds-records`).

Masks (`compare/masks.py`) fix fields that are documented as time-dependent: `gds-dates` (BGNLIB/BGNSTR dates), `gds-mod-dates` (only their modification dates, the last 12 of the 24 date bytes: the creation dates are compared, m5-mask-gds-cif D12), `cif-preamble` (the six host, build and time values of a CIF preamble, Q-MASK-324), `completed-percent` (Magic's `Completed N%` progress lines, Q-MASK-414), `mag-timestamp`, `ext-timestamp`, and `regex:<pattern>` for one-off fields. Two more remove Magic output of features outside the milestone: `magic-tech-sections` drops exactly Magic's extract-section messages printed by `tech load` (each pattern with its Magic source line; extraction is M7; the cifinput `Input style` line left it in M5, which prints it, m5-mask-gds-cif task 13.5), and `mag-checkpaint` drops the `<< checkpaint >>` section of a `.mag` file (areas Magic's DRC still has to check; DRC cases never carry it, and `CHECKPAINT_MASK_RETIRED` in `authoring/casegen.py` retires it from the M1/M2 cases once the M3 engine marks every edit as Magic does, m3-drc design D17). `magic-stamp-note` drops Magic's "writing out-of-date timestamp for CELL." note, which Magic and Hefesto print only when a save lands in the same wall-clock second as the cell's previous stamp; every `magic-console` output carries it by default (`authoring/casegen.py`), because any case that saves a cell can hit it (CMDEDIT-MISC-01 and TECH-G36 did in CI). A mask touches only its field, so it cannot hide a layout difference.

## Documented deviations

`tests/golden/deviations.toml` lists the captured cases whose Magic output shows behavior Hefesto deliberately does not reproduce: a Magic bug fixed under decision X-2, or another resolved open question. Each entry names the decisions (headings in `docs/decisions/`), the outputs allowed to differ and the reason (`goldenlib/deviations.py`):

```toml
[TECH-G43]
decisions = ["Q-TECH-25"]
outputs = ["stdout.txt"]
reason = "tech revert * re-locks the layers locked in the technology file"
```

A case whose Magic output needs a feature of a later milestone (a command or engine that does not exist yet) is listed with `deferred`, the milestone of `docs/roadmap.md` that brings it, instead of or beside `decisions`; its reason names the missing feature. Every entry needs at least one decision or a `deferred` milestone:

```toml
[DRC-CIF-2]
deferred = "M5"
outputs = ["stdout.txt", "top.mag"]
reason = "cifwidth errors come from the mask-layer check of the generated MET1 layer, which arrives with mask generation"
```

A case authored before its capture (the M5 MASK cases, whose `expected/` golden-capture.yml writes later) may be listed with `deferred` while it has no `expected/`: replay reports it as `DEVIATION` ("not captured yet") when named, and the default selection never runs it. An uncaptured case without such an entry is an error when named.

`replay.py` reports such a case as `DEVIATION`, counted like a skip (CTest shows it as skipped). It still fails when another output differs, and it fails when the case passes, so a stale entry is removed. `tests/test_deviations.py` checks that every entry names a captured case, its outputs, existing decisions and a milestone heading of `docs/roadmap.md` for `deferred`. When the milestone lands, the case passes and the stale entry fails replay until it is removed. A Hefesto bug is never listed: fix it.

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

`authoring/m1_cases.py` writes the generated TECH, MODEL, CELL, viewer-related UI and MISC font-label cases from the specs in `authoring/{tech,model,cell,ui,misc}_cases.py` (never `expected/`); `--not-authored` lists the CASES.md rows left out and why (commands hefesto-cli does not have, environment-dependent setups, screenshots). Each command becomes `catch {cmd} result; puts $result` in `commands.tcl` and the same line in `commands.hef`; `.mag` outputs are compared byte for byte with the `mag-timestamp` and `mag-checkpaint` masks, and the console streams with `magic-tech-sections`. The paint/erase table dumps (TECH-G20, G23, G24, G25, G28) paint every type pair on its own 1x1 box and save one `.mag` per table (`authoring/paint_dump.py`, the method TECH/CASES.md asks to record). Edit a spec, re-run the generator, then `capture.py --check`.

`authoring/m2_cases.py` (same options) writes the M2 cases of m2-interactive-editing design D9 from `authoring/{cmdedit,ui_dispatch,view}_cases.py`: the CMDEDIT cases and the UI command-dispatch, tools, feedback, color-window and wizard rows. The M2 additions to earlier areas (MODEL undo, select and move cases, TECH-G26, UI-G-033/036) stay in their M1 modules. `common.FINE` is the `tech: fine` test technology (U = 10 nm, lambda = 2: `scalefactor 2`, `gridlimit 1` and `options set-minimum-grid`). Point selections use Magic's `select ... at X Y` form, since `-dnull` has no cursor.

`authoring/m3_cases.py` (same options) writes the M3 cases of m3-drc design D12: the 113 DRC cases of `tests/golden/DRC/CASES.md` (`authoring/drc_cases.py`, on the committed synthetic technology `authoring/drctest.tech` with each case's rule lines; the capture procedure CAP is `drc_cases.cap`), the standard-cell corpus cases DRC-STD-SKY130-HD and DRC-STD-GF180-7T5V0 (design D14; generated from the installed libraries, one command block per cell), and the `m3_specs()` of the MISC (mask hints), MODEL, TECH and CELL modules. DRC cases compare the console with `drc-console` and the saved cells without the `mag-checkpaint` mask.

## Adding a case

1. Add or pick the row in `tests/golden/<AREA>/CASES.md`.
2. Create `cases/<id>/case.toml` and `input/`, then run `capture.py --check`.
3. Capture with the container runner and commit `expected/`.
4. Re-run CMake, which registers one CTest per captured case.

## M1: cases compared with Magic's console

Most M1 cases (TECH, CELL, MODEL, UI) run a Tcl script in Magic and the equivalent `commands.hef` in `hefesto-cli`, and compare stdout and stderr:

- Every command line of `commands.tcl` is `puts [cmd]` or `catch {cmd} result; puts $result`, so each prints exactly one result line; `commands.hef` has the same commands, one per line, and `quit -noprompt` only in the Tcl script.
- `[hefesto] args` include `--diagnostics=magic`: `hefesto-cli` then prints each script command's result (an empty line for an empty result). As in Magic's Tcl build, a failed command's error is TxError text on stderr and its result stays empty, except for an explicit result (`tech load` failures: `Technology file does not exist or is not readable\n`, `Error in loading technology file\n`) or a Tcl error (unknown command, raised usage error), whose text is the result. It renders diagnostics as Magic's text (`diag::format_magic`: technology-file messages get TechPrintLine's `<file>: line <n>: section <s>:` line; informational messages go to stdout like TxPrintf, the rest to stderr like TxError).
- Like the reference script, which catches every line, `--diagnostics=magic` lets the script go on after an error (an unknown command included). As in Magic's Tcl build, script lines need exact command names (`getc` is `invalid command name "getc"`, Q-UI-41), and every command without a `setpoint` gets the point at screen (100, 100) of the window under it, as `magic -dnull` gives it (Q-UI-43).
- Both streams use the `magic-console` comparator, which removes Magic's leading startup preamble (banner, NULL graphics, startup files) and compares everything else byte for byte. The patterns, each with its Magic source line, are in `compare/magic_console.py`; extend them only when a capture shows another preamble message.

Capture in CI (`.github/workflows/golden-capture.yml`):

- `capture.py --only-captured` / `--only-missing` keep the cases that have, or lack, `expected/provenance.json`; an empty selection succeeds.
- A case whose `case.toml` sets `capture_blocked = "<reason>"` (Magic crashes on it, its output differs between runs, it writes no output) is skipped by every `capture.py` mode with its reason, so golden-capture.yml stays green; the flag is an error once the case has `expected/`. Its milestone driver keeps it deferred, so `--exit-check` lists it (M6: `m6_cases.py` `LEFDEF_CAPTURE_BLOCKED` and `oasis_cases.py` `CAPTURE_BLOCKED`).
- Pull requests that add or edit a `case.toml` or `input/` run both steps: a fresh capture of the captured cases must match `expected/` (verify), and the cases without `expected/` are captured and uploaded as the `golden-expected` artifact (paths under `tests/golden/`), to review and commit. `SOURCE_DATE_EPOCH` is the commit time, so provenance is reproducible.
- `workflow_dispatch` takes `mode` (`both`, `verify`, `capture`) and an optional list of case directories; the weekly schedule verifies only.
