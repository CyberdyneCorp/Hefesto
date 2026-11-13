# Tasks

How this plan runs:
- Each task ends with how it is verified.
- Section 1 (PDK, with GOLDEN's use-order probe and INTEGRATION's round-trip capture job) lands first. Workstreams 3–14 run after the scaffold, in the dependency order of design D2, and edit only what D2 assigns to them. Shared-file needs are reported as open items.
- Golden case inputs are authored by GOLDEN from the owning workstream's requirements (design D18). `expected/` is captured in CI.
- Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`. Requirements with a magic-compat dependence get one test per mode.
- The last task of each capability workstream also runs `scripts/check_requirement_coverage.py --capability <cap>` (task 2.6) and verifies that every requirement of its capability is covered, or listed as deferred with a reason.
- Every approved decision cited below is in `docs/decisions/m5-decision-table.md`.
- The design's four Open Questions were decided on 2026-11-13: option (a), the recommended one, for each.

## 1. PDK: library GDS data, the use-order probe and the round-trip capture job (first)

- [ ] 1.1 Verify the release data (design D19). List the libraries and their assets in ciel `sky130-5740c4a8…`, `gf180mcu-5740c4a8…` and `ihp-sg13g2-5e6d592e…`. Install `<variant>/libs.ref/<lib>/gds/` of each into a scratch root with `pdk install … -only …`, and record the download size, kept size, peak disk and the GDS file names per library. Check that Magic 8.3.694 in the capture image reads each file with the PDK's rc, and time Magic's read plus write of each library with the D19 script (wall time per library, recorded for sizing the capture shards of task 1.6). Verify the facts are recorded in design D19. If the kept GDS exceeds about 4 GB cached, or a library fails in Magic, stop and raise it under Open Question 1 with the numbers.
  - Round 1 (2026-11-13): local part done. Every library of the three releases listed and installed alone into a scratch root (download, uncompressed tar, kept size, peak disk and file names); the facts are in design D19 "Task 1.1 facts" and `docs/building.md`. Kept GDS about 431 MB (about 50 MB cached), far below 4 GB, so Open Question 1 needs no change. Left: Magic reading and writing each file with the PDK's rc, and its per-file time. The D19 script is `scripts/d19/` (tests in `scripts/tests/test_gds_rt.py`, against a fake Magic); it runs in `golden-capture.yml` `d19-magic-timing`, dispatched with `mode=d19-timing` (Docker is not usable locally). The run's result is recorded below when it returns.
- [x] 1.2 Add the data sets `sky130-gds`, `gf180mcu-gds` and `ihp-gds` to `scripts/fetch_pdk.py`: the same pins, the per-library `-only` subsets, tree digests and cache keys, and `--print-digest <set>`. Add the GDS paths to `tests/unit/common/reference_data.hpp`, where tests skip cleanly when a set is missing. Update `scripts/tests/`. Verify that a fresh install of each set matches its digest and a second run is a verified no-op.
  - As landed (round 1): `sky130-gds`, `gf180mcu-gds`, `ihp-gds` plus the standard-cell subsets `sky130-gds-sc`, `gf180mcu-gds-sc`, `ihp-gds-sc`, under their own PDK root `.cache/pdks/gds` (design D19 "Task 1.1 facts" says why); keys `pdk-<set>-<pin12>-<digest16>`; `sky130-gds` runs one `pdk install` per library to bound peak disk (4.73 GB). A bare `fetch_pdk.py` still fetches only sky130, gf180mcu, ihp and magic-ref. `reference_data.hpp` gains `pinned_gds_library`, `pinned_gds` and the three standard-cell constants (nullopt when missing, so tests skip). 13 new tests in `scripts/tests/test_fetch_pdk.py`. Real check: a fresh install of all six sets matched their digests and a second run was a verified no-op. `tools/golden/goldenlib/external.py` accepts the six sets as `[[external]] set` values and resolves them under `<pdk-root>/gds` (regression test in `tools/golden/tests/test_external.py`).
- [ ] 1.3 Restore and fetch the GDS sets only in the jobs that use them (`golden-capture.yml` `capture-gds-rt`, `perf.yml` `gds-bench`/`gds-roundtrip`), each under its own key. `ci.yml`'s golden legs fetch only the three standard-cell subsets (`sky130_fd_sc_hd`, `gf180mcu_fd_sc_mcu7t5v0`, `sg13g2_stdcell`) under their own keys, for the three `MASK-RT-*` cases they replay (design D19). Verify that a cold run fetches each set once, a warm run fetches nothing, and `ci.yml`'s legs fetch the three standard-cell subsets and nothing else.
  - Round 1: `ci.yml` done: `reference-data` looks up, fetches and saves the three `*-gds-sc` sets under their own keys; the build legs restore and verify only those three. `golden-capture.yml` `capture-gds-rt` (task 1.6) and `d19-magic-timing` restore, fetch and save the full sets, each shard only its PDK's sets. Left: `perf.yml` `gds-bench`/`gds-roundtrip` (PERF, tasks 14.1 and 14.3; the reference-data steps are the ones of `capture-gds-rt`), and the cold/warm cache check in CI. A missing standard-cell `MASK-RT` input on a golden leg must fail, not skip: that is task 13.3's `--require-inputs` (GOLDEN). Note: the repository's Actions cache was at 11.2 GB on 2026-11-13 (about 9 GB ccache), above GitHub's 10 GB limit, so LRU eviction already happens; an evicted PDK set is re-fetched by the reference-data job, which affects cold-cache timings.
- [x] 1.4 Update `docs/building.md` "Reference data for tests" with the GDS sets: pins, libraries, sizes, cache keys and commands. Verify that the documented commands run as written.
  - As landed (round 1): `docs/building.md` "Library GDS data sets (M5)": pins, libraries per set, downloaded, kept, cached and peak sizes, the libraries left out and why, the separate root, the cache keys, CI use and commands. The `--cache-info`, `--print-digest` and install commands were run as written (install into a scratch `--root`).

- [x] 1.5 (GOLDEN) Add a use-order probe to the capture image: a Tcl script that records Magic's child order after controlled searches (placements, partial and surrounding searches, `DBCellFindDup`-style exact lookups), through GDS and CIF writes (the use-id properties); `cellname list childinst` is use-id hash order and is recorded only as a check. It also records the orders after a plain fresh-session `gds write` and `cif write` of a cell with 10 or more uses, one touching its bounding box (design D7). INDEX's trace test (task 5.1) replays these traces, so this task lands before INDEX starts (design D2). Verify that the recorded traces are committed under `tests/data/db/use_index/` with their scripts.
  - As landed (round 1): the probe (`tools/golden/harness/use_order/use_order_probe.tcl`, copied into each case's `input/`), 12 cases `tests/golden/MASK/cases/MASK-UO-001..012` (compared output `trace.txt`), the generator `tools/golden/authoring/use_order_cases.py` (`--sync` copies captured traces to `tests/data/db/use_index/<id>.trace`), the trace format and step-to-Magic-primitive notes in `tests/data/db/use_index/README.md`, and tests against a stand-in Magic in `tools/golden/tests/test_use_order_probe.py`. Captured by `golden-capture.yml` run 43437846983 (Magic 8.3.694 in the capture image); `expected/` and the 12 `.trace` files are committed. Every trace ends with `end` and none has an `error` line. Each MASK-UO case carries a `deferred = "M5"` deviation (outputs `trace.txt`, `stdout.txt`, `stderr.txt`) until INDEX, GDSW and the CIF writer land; INDEX removes them (task 5.1).
- [ ] 1.6 (INTEGRATION, with GOLDEN) Add the sharded `capture-gds-rt` job to `golden-capture.yml` for the `MASK-RT-*` cases: one shard per PDK, and separate shards for libraries whose task 1.1 time needs one (for example `sky130_fd_io` and the SRAM macros), each with a `timeout-minutes` sized from task 1.1's measured Magic times plus a margin. The existing `capture` job keeps its 120 minutes and excludes `MASK-RT-*`. Verify with a dispatch run that captures one library per shard within its timeout.
  - Round 1: `golden-capture.yml` gains `plan` (splits the selected cases), `capture-gds-rt` (a matrix over the shards that have `MASK-RT-*` cases; verify and capture as `capture`; artifact `golden-expected-gds-rt-<shard>`) and the dispatch-only `d19-magic-timing` (task 1.1). `capture` now drops `MASK-RT-*` cases and keeps its 120 minutes. The shard table, with each shard's data sets and `timeout-minutes`, is `scripts/gds_rt_shards.py` (tests in `scripts/tests/test_gds_rt.py`): `sky130`, `sky130-io`, `sky130-sram`, `gf180mcu`, `gf180mcu-io`, `ihp`, `ihp-io`. The timeouts are provisional (120 minutes each) until the `d19-magic-timing` run measures Magic. Left: the dispatch run that captures one library per shard, which needs GOLDEN's `MASK-RT-*` cases (task 13.7).

## 2. Scaffold

- [x] 2.1 Create the change `m5-mask-gds-cif` (proposal, design, tasks, `skip_specs: true`). Verify that `openspec validate m5-mask-gds-cif --strict` and `openspec validate --all --strict` pass.
- [x] 2.2 Add the areas `mask` (subdirectories `plane`, `style`, `input`, `ops`, `gen`), `gds` (`stream`, `read`, `write`, `command`) and `cif` (`read`, `write`, `command`), each subdirectory with its `sources.cmake` and `codes.inc`, plus the INTEGRATION-owned `{mask,gds,cif}/{sources.cmake,codes.cpp}`. Add their test directories and `docs/diagnostics/codes/{mask,gds,cif}.md` with one section per owning workstream (design D1). Verify that the build passes and the test "each area's code table matches its document and prefix" covers the three areas.
- [x] 2.3 Write the documented public headers of design "Area contracts" with stubs, and add `CORE-NOTIMPL-005`. Register `gds` and `calma` with Magic's option table, and move `cif` to `cif/command/` with every option. Unimplemented options fail with `CORE-NOTIMPL-005`, and the M4 `cif` queries keep working. Verify that the headers compile in `macos-debug` and `macos-bench`, a `[m5-scaffold]` test lists every `gds` and `cif` option, and the M4 `cif` tests pass unchanged.
  - As landed (round 1): 2.2 and 2.3 by the scaffold. Areas `mask`, `gds`, `cif` with per-subdirectory `sources.cmake`/`codes.inc` and `docs/diagnostics/codes/{mask,gds,cif}.md`; the area code-table test covers them. `gds`/`calma` registered with Magic's 29-entry table; `cif` moved to `src/core/src/cif/command/command.cpp` (`git mv`) with its 23 options; unimplemented options fail with `CORE-NOTIMPL-005` (`cmd/stubs.hpp`); `SCRIPT-BIND-002` retired (DRC-CIF-6/7's `cif ostyle` now gets CORE-NOTIMPL-005 until task 3.4). The headers were compiled standalone with Debug flags and `-Werror` and in `macos-debug`/`ci-macos`; `macos-bench` (desktop/Vulkan build) is left to CI's bench leg.
- [x] 2.4 Add the shared interfaces of design D4:
  - `mask::MaskSession` in `EditorState`, with the shared `random()` generator moved from `db/commands_misc.cpp`;
  - `MaskPlane` and `PlanePool`, `OutputStyle`, `InputStyle`, `OperatorContext` and `evaluate_layer`, `GenerationContext` and `Progress`;
  - `db::UseIndex` (`place`, `remove`, `search`, `find_equal`, `enumerate_all`, `order`), initially returning today's unbinned order;
  - `ReadSink`, `ByteSource`/`ByteSink`, `RecordReader`/`RecordWriter`;
  - the `read` entry points, and the two-phase `prepare_write`/`emit_write` entry points with `write` calling both (design D4).

  Verify that the CLI, shell and core tests pass unchanged and `random seed 1; random` gives the M4 sequence.
  - As landed (round 1): `EditorState::mask` (`mask::MaskSession`: styles, `gds`, `cif`, one shared `WarningSettings`, `random`), `util::LibcRandom` moved out of `db/commands_misc.cpp` (a process-wide fallback without an editor), and every D4 interface as a stub; `db::UseIndex` is unbinned LIFO and not yet a `CellDef` member (INDEX adds it). Differences from "Area contracts" are in design "As-landed scaffold contracts". `[m5-scaffold]`: 10 cases, 395 assertions, including `random seed 1; random` = 1804289383, 846930886, 1681692777.
- [x] 2.5 Record the ownership table and contracts in design.md ("As-landed scaffold contracts") and seed `docs/design/m5-ui-conformance.md`. Verify that `openspec validate --all --strict` passes.
  - As landed (round 1): design "As-landed scaffold contracts" (file/owner table and every difference from "Area contracts" and D4); `docs/design/m5-ui-conformance.md` seeded.
- [x] 2.6 Add `--capability <cap>` (repeatable) to `scripts/check_requirement_coverage.py`, limiting the report to those capabilities, with a test in `scripts/tests/`. Verify that `--capability gds-write` reports only `gds-write` requirements.
  - As landed (round 1): repeatable `--capability`, intersecting `--milestone`; an unknown capability is an error. `--capability gds-write` reports the 27 `gds-write` requirements only (test in `scripts/tests/test_checks.py`).

## 3. STYLES: mask-output-styles

- [ ] 3.1 Implement the shared style/variant/scale core (`StyleStateMachine`, `parse_scale`) and the cifoutput reader for every statement: style, variant, scalefactor, units, stepsize, gridlimit (Q-MASK-8), layer, templayer, labellayer, labels (Q-MASK-11), calma, render, min-width and options (Q-MASK-401). Include Magic's tech messages. Verify the `[cap:mask-output-styles]` tests for "cifoutput section grammar", "Style declaration and the style list" (Q-MASK-10), "Style loading and variant-conditional statements", "scalefactor statement", "units, stepsize and gridlimit statements", "Mask layer declarations", "Layer list syntax", "labels statement and label layer mapping", "GDS layer mapping statement" and "min-width, render and options statements".
- [ ] 3.2 Implement operator statement parsing with argument validation into `LayerOp`, and the automatic stacked-contact inclusion. Verify the `[req:Operator statement arguments]` tests (every operator, valid and invalid argument counts) and `[req:Automatic stacked-contact inclusion]`.
- [ ] 3.3 Implement unit normalisation (expander and reducer GCD with `bloat-all` counted as 1, Q-MASK-13), the CIF reducer (needed by the CIF writer, task 11.3), `OutputStyle::rescale`, the halo radius (Q-MASK-18 in both modes) and the contact cut size query. Verify the `[req:Unit normalisation]`, `[req:CIF reducer]`, `[req:squares-to-grid conversion by gridlimit]` (parsing side), `[req:Interaction radius]` and `[req:Contact cut size query]` tests, including one per mode for Q-MASK-18.
- [ ] 3.4 Implement output style selection with exact-name and base+variant matching and the warning (Q-MASK-10, fix). Move `cif ostyle` and `cif listall ostyle` onto it. Verify the `[req:Output style selection]` tests ("Unique prefix", "Ambiguous prefix"), and that the `[req:Style declaration and the style list]` scenario "Prefix shadowing" gives the same result in both modes (a fix).
- [ ] 3.5 Re-source `drc::CifCatalog` from `OutputStyle`, keeping its interface, and remove its own parse code. Read the first style's grid in `tech/loader.cpp` through the new reader. Verify that every `tests/unit/drc/rules/` catalog test, the six `tests/data/drc/rules/magic_cif/*.txt` comparisons and all DRC golden cases pass unchanged.
- [ ] 3.6 Implement mask generation error reporting (feedback counts, `options no-errors`). Verify the `[req:Mask generation error reporting]` tests.
- [ ] 3.7 Load the sky130A, gf180mcuD and ihp-sg13g2 cifoutput sections at the M4 pins without new diagnostics. Verify a unit test per PDK that compares the style list, scale and layer count with Magic's `cif listall ostyle` and `cif scale out` captures.
- [ ] 3.8 Keep every function's cognitive complexity at most 15 (flag irreducible ones). Verify with the cognitive-complexity tooling on `mask/style/`, and that `check_requirement_coverage.py --capability mask-output-styles` reports every requirement covered.

## 4. OPS: mask-output-operators

- [ ] 4.1 Implement `MaskPlane` with Magic's CIF paint and erase tables, moved from `hints/region` (the hints API is unchanged), and `PlanePool`. Verify the `[req:Geometric model of mask planes]` tests and that the `tests/unit/hints/` suites pass unchanged.
- [ ] 4.2 Implement `evaluate_layer` (the layer evaluation pipeline for an area, clipping, temp layers) and the boolean and grow group: `or`, `and`, `and-not`, `grow`, `shrink` (square and Euclidean), `grow-grid` (Q-MASK-1) and `grow-min` (Q-MASK-15 exact arithmetic). Verify the `[req:Layer evaluation pipeline for an area]`, `[req:or, and, and-not]`, `[req:grow and shrink (default, square metric)]`, `[req:Euclidean grow and shrink]`, `[req:grow-grid]` and `[req:grow-min]` tests, with both modes for Q-MASK-1.
- [ ] 4.3 Implement the contact-cut group: `squares` (Q-MASK-27), `squares-grid`, `slots`, the contact strip pre-pass, and squares-to-grid conversion by gridlimit. Verify the `[req:squares]`, `[req:squares-grid]`, `[req:slots]` and gridlimit tests, with both modes for Q-MASK-27 and negative-coordinate cases.
- [ ] 4.4 Implement the bridge and bloat group:
  - `bloat-or` (Q-MASK-20 split tiles), `bloat-min`, `bloat-max`, `bloat-all` (Q-MASK-230, 231, 232);
  - `close` (Q-MASK-2);
  - `bridge` (Q-MASK-3, and the Q-MASK-227 clamp);
  - `bridge-lim` (Q-MASK-3, 5, 228, 229).

  Verify the `[req:…]` tests of each operator, with one test per mode for every compat decision.
- [ ] 4.5 Implement the topology group:
  - `maxrect`, `orthogonal`, `not-square` (Q-MASK-7);
  - `overlapping`, `nonoverlapping`, `interacting`, `noninteracting` (Q-MASK-21);
  - `net` (Q-MASK-22), `tagged` (Q-MASK-6), `bbox` (Q-MASK-23 cell bbox), `boundary`;
  - `mask-hints`, through `hints::output_region`.

  Verify the `[req:…]` tests of each operator, with both modes for Q-MASK-6, 7 and 21.
- [ ] 4.6 Implement hierarchical-mode truncation (Q-MASK-26) and the render and min-width handling in the operator path. Verify the `[req:Hierarchical-mode truncation]` tests.
- [ ] 4.7 Pin the float paths of design D22 (Euclidean grow, grow-min `sqrt`, bridge distance) with value tests. Verify that they pass identically on every CI leg (`linux-*`, `macos-*`).
- [ ] 4.8 Add randomized property tests of the operators (`hefesto-testing-strategy` "Geometry property tests"), seeded and reproducible: `grow d` then `shrink d` contains the original; mask-plane contents are independent of paint order; `or` and `and` are commutative; `and-not` of a layer with itself is empty. Failing seeds are recorded as regression tests in `tests/unit/mask/ops/`. Verify that the property tests pass on every leg.
- [ ] 4.9 Keep every function's cognitive complexity at most 15, or at most 25 with a flag for the bridge, bridge-lim, bloat-all and squares passes that cannot be split without obscuring the port. Verify with the cognitive-complexity tooling on `mask/{plane,ops}/`, list every flagged function in this task's note, and verify that `check_requirement_coverage.py --capability mask-output-operators` reports every requirement covered (after task 4.8).

## 5. INDEX: use enumeration order (Q-MASK-204, Q-MASK-323)

- [ ] 5.1 Port bplane into `db::UseIndex`:
  - insertion LIFO, deletion, the bins (sizing, recursive sub-binning, oversized lists), the rebuild on a non-surrounding search at 10 or more unbinned uses, and on-the-fly sub-bins;
  - `search` as Magic's `BPE_OVERLAP`: the area shrunk by 1 on each side before the surround test against the index's bounding box (bpEnum.c:104-106), so a search over a cell's own bounding box rebuilds when a use touches its edge; `find_equal` as `BPE_EQUAL` (`DBCellFindDup`), which never rebuilds;
  - the bin walk, `enumerate_all` and `order(compat)`, with the reverse-insertion order for magic-compat OFF;
  - the index mutex with snapshot orders.

  Verify unit tests built from bpBins.c/bpEnum.c cases (including the shrink at the bounding-box edge and `find_equal` not rebuilding), and a trace test that replays recorded place, search and enumerate sequences against orders captured from Magic (the probe of task 1.5), including a fresh session with one GDS write and one CIF write of a cell with 10 or more uses touching its bounding box.
- [ ] 5.2 Route every use placement and removal through `UseIndex`. This covers `.mag` read, `getcell`, copy, move, delete, GDS and CIF reads, and DBReComputeBbox re-placement in the child's parent-use order. Route every area search over uses through `UseIndex::search`: DRC interactions, flattening, `select`, DBCellEnum, generation and `uses_in_search_order(def, compat)`. Verify that the M1–M4 unit suites and every golden case (including DRC-STD-SKY130-HD and DRC-STD-GF180-7T5V0) pass unchanged, with `.mag` output still alphabetical.
  - Note (round 1, from GOLDEN's probe): the MASK-UO replay matches Magic only if `expand` (DBExpandAll, one `BPE_OVERLAP` search over the box) and `select cell <id> at <pt>` (DBSeeTypesAll, a 1x1 search at the point) also go through `UseIndex::search`, as do `delete` (SelEnumCells, a 1x1 search at the use's lower-left), `getcell` (`find_equal`, then `place`) and DBReComputeBbox re-placement.
- [ ] 5.3 Implement the Q-MASK-400 OFF rule in the DRC interaction finder (`drc-engine` "Interaction area"), shared with GEN. Verify the `[cap:drc-engine][req:Interaction area]` tests in both modes ("Subcell far from paint") and that the DRC golden cases pass.
- [ ] 5.4 Add a TSan stress test with index rebuilds from the DRC worker while the command thread enumerates. Verify that the tsan leg passes.
- [ ] 5.5 Check the `drc-profile` instruction gate with the index on the DRC path (with PERF). Verify that it stays at or below 12.38 billion instructions per block pass.
- [ ] 5.6 Keep every function's cognitive complexity at most 15 (flag irreducible ones). Verify with the cognitive-complexity tooling on `db/use_index*`, and that the `[cap:gds-write][req:Structure ordering]` index tests exist for both modes (the requirement is closed by task 10.11's coverage check).

## 6. GEN: mask-hierarchical-generation

- [ ] 6.1 Implement flat generation (`generate_flat`): the yank flatten with a `HintBatch`, every layer in style order and the per-layer planes. Implement `cif see`, `cif paint`, `cif coverage` (Q-MASK-325 in both modes) and the statistics. Verify the `[cap:mask-hierarchical-generation][req:Flat region generation]`, `[req:Hierarchical layer preview]` and `[req:Generation statistics]` tests, and the `[cap:cif-format-io][req:cif see, paint, coverage and statistics options]` tests with CIF.
- [ ] 6.2 Implement per-cell output composition, the own-paint generation area, the halo radius (Q-MASK-404), the classification of hierarchically processed layers (Q-MASK-403, 405), non-local layers and the interaction search grid. Verify the `[req:…]` tests of these requirements, with both modes for each compat decision.
- [ ] 6.3 Implement interaction-area discovery (sharing the finder of task 5.3), flattening for interaction processing (vendor paint always copied, Q-MASK-401), interaction-area correction and the residual, hierarchy-stopping operators (Q-MASK-402) and parent/child disagreement reporting. Verify the `[req:…]` tests of these requirements, with both modes for Q-MASK-400 and 402.
- [ ] 6.4 Implement the array interaction geometry, regions, correction (Q-MASK-406) and replication, and sliver growth (Q-MASK-409). Verify the `[req:…]` tests, with both modes for Q-MASK-406 and 409.
- [ ] 6.5 Implement the hierarchy and array output switches (Q-MASK-410), flat CIF output (Q-MASK-321, 411), the non-manhattan merge option (Q-MASK-413), vendor cells in hierarchical generation and the known limitations. Verify the `[req:…]` tests of these requirements in both modes.
- [ ] 6.6 Implement progress and cancellation (Q-MASK-414): `Progress` rate-limited to about 5 s, a cancel token, and a temporary file renamed on success and on Magic-compatible failure but discarded on cancel (with GDSW and CIF). Verify the `[req:Progress reporting and interruptibility]` tests "Long write shows progress" (with a test clock) and "Cancelled write".
- [ ] 6.7 Wire `mask-hints` into generation as m3-drc design D13 describes (`output_region`, `flat_copy_hints`). Verify the `[cap:mask-hints]` M5 scenarios and golden MISC-MH-03..06 (with GOLDEN).
- [ ] 6.8 Keep every function's cognitive complexity at most 15 (flag irreducible ones). Verify with the cognitive-complexity tooling on `mask/gen/`, and that `check_requirement_coverage.py --capability mask-hierarchical-generation` reports every requirement covered.

## 7. INPUT: mask-input-styles and the read engine

- [ ] 7.1 Implement the cifinput reader on STYLES' shared core:
  - the section grammar ("Cifinput section grammar");
  - style declaration and the default style (Q-MASK-302) and style selection (Q-MASK-303);
  - variants, scalefactor (Q-MASK-300) and gridlimit;
  - the CIF layer name table ("CIF layer name table"), layer and templayer rules and the input operators;
  - GDS layer and datatype mapping with wildcards (Q-MASK-326);
  - label layer statements (Q-MASK-301, 311);
  - the tech messages and input-scale reporting ("Input-scale reporting").

  Mode-dependent values are resolved at selection and again when magic-compat changes. Verify the `[cap:mask-input-styles]` tests of these requirements, with one per mode for each compat decision.
- [ ] 7.2 Replace the M4 `cif` command's ad-hoc istyle parse with the reader (`cif istyle`, `cif listall istyle`, `cif scale in`, `cif lambda in`). Verify that the M4 `cif` tests and the SCRIPT golden cases using them pass unchanged.
- [ ] 7.3 Implement the read engine behind `ReadSink`: per-cell read planes, scale S, CIFScalePlanes and CIFInputRescale (mid-read subdivision under `limit`/`rescale`, `DBScaleEverything` under the exclusive lock with undo disabled), and CIFPaintCurrent through `evaluate_layer` with the per-cell layer processing order ("Per-cell layer processing order") and the painting rule results, including the half-up/half-down contact edges ("Painting rule results"). Include label placement during read ("Label placement semantics during read") and polygon to region conversion through `db::decompose_polygon` ("Polygon to region conversion"). Include the input operators: grow/shrink (Q-MASK-306), grow-grid (Q-MASK-334), not-square (Q-MASK-330), copyup (Q-MASK-309), boundary (Q-MASK-307), mask-hints (Q-MASK-307, `add_input_region`) and tagged (Q-MASK-310, 6) with explicit unit types. Verify the `[req:Per-cell layer processing order]`, `[req:Painting rule results]`, `[req:Label placement semantics during read]`, `[req:Polygon to region conversion]`, `[req:Conversion of read units to internal units]` and `[req:Mid-read subdivision of read units]` tests, and the `[req:…]` tests of each input operator in both modes.
- [ ] 7.4 Load the sky130A, gf180mcuD and ihp-sg13g2 cifinput sections at the M4 pins. Verify a unit test per PDK against Magic's `cif listall istyle` and `Input style …: scaleFactor=…` lines, and that the line is printed at tech load (with GOLDEN's mask change, task 13.5).
- [ ] 7.5 Keep every function's cognitive complexity at most 15 (flag irreducible ones). Verify with the cognitive-complexity tooling on `mask/input/`, and that `check_requirement_coverage.py --capability mask-input-styles` reports every requirement covered.

## 8. DRCCIF: drc-cif-rules

- [ ] 8.1 Implement the mask-layer check (drcCifCheck) at the end of `basic_check`. It runs only when the DRC style binds a cifoutput style and has an applicable mask-layer rule. It generates the used layers flat over the check area grown by the mask halo, with the worker's `GenerationContext`, and runs the width, spacing, area and maxwidth checks. Verify the `[cap:drc-cif-rules][req:Mask style binding]`, `[req:Mask-layer rule statements]`, `[req:Mask-layer check procedure]`, `[req:Mask area rule]` and `[req:Mask maxwidth rule]` tests.
- [ ] 8.2 Implement the error mapping to layout coordinates (Q-DRC-13: ON truncation, widening and drops; OFF outward rounding; a `DRC-MASK-*` drop-count diagnostic in both modes). Verify the `[req:Mapping mask errors to layout coordinates]` tests in both modes.
- [ ] 8.3 Make `cif ostyle` reselect the DRC catalog's style. Remove `deferred = "M5"` from DRC-CIF-4..7 (with GOLDEN). Verify that DRC-CIF-2..7 pass on every leg and DRC-CIF-1 and DRC-CIF-8 still pass.
- [ ] 8.4 Run the background checker with mask-layer rules under TSan, and check the cost with PERF. Verify that the tsan leg passes and `drc-profile` stays at or below 12.38 billion instructions per block pass, with `DrcStatistics::cif_tiles` reported.
- [ ] 8.5 Keep every function's cognitive complexity at most 15 and verify with the cognitive-complexity tooling on `drc/check/mask_check*`, and that `check_requirement_coverage.py --capability drc-cif-rules` reports every requirement covered.

## 9. GDSR: gds-read and the stream layer

- [ ] 9.1 Implement the stream layer:
  - `ByteSource` (plain and gzip, concatenated members, decompressed offsets, re-inflating seek) and `ByteSink` (plain, gzip at level z, memory);
  - `RecordReader` with one-record look-ahead, and `RecordWriter`;
  - the eight-byte real encode and decode, the character tables (Q-MASK-200), string limit (Q-MASK-207) and date encoding (Q-MASK-203, with the Q-MASK-132 pinned-zone setting).

  Verify the `[cap:gds-read][req:Record framing and primitive data types]`, `[cap:gds-write][req:Record encoding]`, `[req:Eight-byte real encoding]` (the MASK-GW-040 vectors), `[req:String records and character mapping]` and `[req:Date records]` tests, including the non-UTC zone test.
- [ ] 9.2 Implement the library grammar and header, units and input scale, coordinate conversion with automatic rescaling (with INPUT's engine), structure processing (Q-MASK-110), structure timestamps (Q-MASK-134), duplicates (Q-MASK-111), cells already in memory, element dispatch, and layer and datatype mapping. Verify the `[req:…]` tests of these requirements, with both modes for each compat decision.
- [ ] 9.3 Implement BOX, BOUNDARY with non-Manhattan decomposition (`db::decompose_polygon`), PATH elements (Q-MASK-113, 114, 115), path-to-polygon conversion (Q-MASK-116, 118) and path centerline properties (Q-MASK-119). Verify the `[req:…]` tests in both modes and the float values of design D22 on every leg.
- [ ] 9.4 Implement TEXT parsing (Q-MASK-120, 121, 122), label layer assignment, and pin shapes and port labels (Q-MASK-121, 123). Verify the `[req:…]` tests in both modes, and close the M1 gap `[cap:font-labels][req:GDS representation of font attributes]`.
- [ ] 9.5 Implement:
  - structure reference transforms, instance placement scaling and AREF construction (Q-MASK-124);
  - instance properties (Q-MASK-125), use placement rules and magnified instances;
  - post-order reading and file repositioning (Q-MASK-126 fix);
  - flattening small cells, flatglob, polygon subcells and path subcells;
  - end-of-structure and end-of-read processing.

  Verify the `[req:…]` tests of these requirements in both modes.
- [ ] 9.6 Implement read-only vendor views (Q-MASK-128, 129; the units check), content-hash timestamps (`content_stamps` with the spec's three vectors) and when they are assigned, wired into `cell/timestamps.cpp`. Verify the `[req:Read-only vendor views]`, `[req:Content-hash timestamps for read-only cells]` and `[req:When content-hash timestamps are assigned]` tests, and golden CELL-TS-07 (with GOLDEN).
- [ ] 9.7 Implement mask hints on input (`gds maskhints`, `gds_maskhints_region`), the text magnification option, the grid subdivision option (Q-MASK-130), the read option syntax and defaults, the read command with file lookup, the read diagnostics, and the host mechanisms (Q-MASK-132). Verify the `[req:…]` tests of these requirements.
- [ ] 9.8 Read each sky130, gf180mcu and IHP standard-cell library GDS of the data sets (task 1.2) without a crash, sanitizer report or unexpected diagnostic. Verify under `macos-asan` and the Linux ASan leg.
- [ ] 9.9 Add randomized property tests of the stream layer: `decode_real8(encode_real8(x))` round-trips every representable value within the format's precision, and encoding is monotonic in x; `encode_date`/decode round-trip every second in range under UTC and one pinned non-UTC zone. Failing seeds are recorded as regression tests in `tests/unit/gds/stream/`. Verify on every leg.
- [ ] 9.10 Keep every function's cognitive complexity at most 15 (record dispatch handlers flagged up to 25 as parser code). Verify with the cognitive-complexity tooling on `gds/{stream,read}/`, and that `check_requirement_coverage.py --capability gds-read` reports every requirement covered (after task 9.9).

## 10. GDSW: gds-write and the gds command

- [ ] 10.1 Implement the write command and file naming (Q-MASK-210), the write option settings and defaults, the library header and trailer (Q-MASK-201: UNITS per write), and the write preconditions (Q-MASK-219: style checked before the file opens). Implement `prepare_write` (design D4, exclusive lock): subtree loading as Magic's `DBCellReadArea` walk through `UseIndex::search`, with Magic's exact areas (the root's bounding box, the area transformed into each child's coordinates for each array element, and the array early-out of DBexpand.c:329-337), then `DBFixMismatch` with the DBReComputeBbox re-placement. Implement `emit_write` (shared lock) for the rest. Verify the `[cap:gds-write][req:…]` tests of these requirements in both modes, and an index-state test that a fresh-session write bins a root of 10 or more uses touching its bounding box, as Magic does.
- [ ] 10.2 Implement structure ordering through `UseIndex` at Magic's sampling point (SREFs before CIFGenSubcells), structure naming (Q-MASK-209 uniquification) and output scale factors (Q-MASK-222, 225). Verify the `[req:Structure ordering]` scenarios ("Canonical order with magic-compat OFF", "Binned order with magic-compat ON", "Fresh-session write of a cell whose uses touch its bounding box"), `[req:Structure naming]` and `[req:Output scale factors]` tests.
- [ ] 10.3 Implement the structure body order, mask geometry per structure (`generate_hierarchical`), tile decomposition and enumeration order, and unmerged boundary output. Verify the `[req:…]` tests of these requirements.
- [ ] 10.4 Port merged polygon output with the merge and collinear passes exactly, slits included (Q-MASK-217). Verify the `[req:Merged polygon output]` tests (point order and split points) and the merge golden cases.
- [ ] 10.5 Implement label layers and TEXT records: our own stable merge sort over the reversed label list (Q-MASK-205), port labels (Q-MASK-206), MAG and magscale (Q-MASK-221), and the font-size formulas (Q-MASK-122). Verify the `[req:Label layers]` and `[req:Label TEXT records]` tests in both modes, and the same TEXT order on Linux and macOS.
- [ ] 10.6 Implement cell reference elements and array flattening on output. Verify the `[req:Cell reference elements]` and `[req:Array flattening on output]` tests.
- [ ] 10.7 Implement:
  - verbatim copy of read-only cells (Q-MASK-216);
  - full dump of a vendor library (Q-MASK-128, 211 with the shared `random()`, 212, 214, 215);
  - addendum mode and abstract views (Q-MASK-226);
  - compressed output (Q-MASK-223);
  - the write result and error propagation.

  The writer never mutates the layout (Q-MASK-208). Verify the `[req:…]` tests of these requirements in both modes, including "Compressed and plain outputs agree".
- [ ] 10.8 Implement the `gds`/`calma` command as an option table: every option, a bare `gds` meaning write, `rescale no` (Q-MASK-130), help text and DRC queuing on read. Verify the `[req:Write option settings and defaults]` and `[cap:gds-read][req:Read option syntax and defaults]` tests and that `gds help` matches Magic's text.
- [ ] 10.9 Pass the `[req:Complete annotated example]` byte test, and the `[req:Magic write-time side effects]` test (no mutation in Hefesto). Verify both in both modes.
- [ ] 10.10 Add randomized property tests of write then read under magic-compat OFF: random hierarchies of Manhattan geometry, labels and uses, written with `merge no` and `merge yes` and read back with a matching cifinput style, give the same decoded geometry per layer (as polygon sets) and the same uses (as multisets). Failing seeds are recorded as regression tests in `tests/unit/gds/write/`. Verify on every leg.
- [ ] 10.11 Keep every function's cognitive complexity at most 15 (the merge pass and command handlers flagged at most 25 if irreducible). Verify with the cognitive-complexity tooling on `gds/{write,command}/`, and that `check_requirement_coverage.py --capability gds-write` reports every requirement covered (after task 10.10).

## 11. CIF: cif-format-io

- [ ] 11.1 Implement the CIF lexer and the table-driven parser driving `ReadSink`, covering:
  - lexical rules and input grammar;
  - symbol definitions and scale;
  - point scaling, layer selection, box (Q-MASK-315), round flash (Q-MASK-316), polygon and wire (Q-MASK-333);
  - label user extensions (Q-MASK-314 fix, Q-MASK-327);
  - end of file and top-level geometry (Q-MASK-317);
  - read diagnostics (Q-MASK-331).

  Verify the `[cap:cif-format-io][req:…]` tests of these requirements in both modes.
- [ ] 11.2 Implement the per-read `SymbolTable` (Q-MASK-318 fix: symbol n maps to a cell of this read, `n_k` on collision, no existing cell cleared; DD compat) and calls and transformations (Q-MASK-309, 335). Verify the `[req:Symbol-to-cell naming on read]` and `[req:Calls and transformations]` tests, including forward calls, DD then redefinition, and a collision with a library cell.
- [ ] 11.3 Implement the CIF writer with GDSW's two phases (design D4): `prepare_write` makes the same subtree-loading walk through `UseIndex::search` and `DBFixMismatch` as task 10.1 (CIFwrite.c:137-144, with the shared helper), then header (Q-MASK-324), symbol numbering and order through `UseIndex` at Magic's two sampling points (Q-MASK-323), the CIF reducer of task 3.3, symbol definition format, labels (Q-MASK-322), calls and arrays, and preconditions with flat output (Q-MASK-321, 411). Verify the `[req:…]` tests of these requirements in both modes.
- [ ] 11.4 Implement the remaining `cif` options:
  - read, write and flat (Q-MASK-320);
  - style, scale and lambda;
  - limit and rescale;
  - the output-format toggles;
  - drccheck and warning;
  - polygon and unfracture (Q-MASK-413);
  - `*hier`, `*array` and prefix.

  Remove `script/binding/cif_command.cpp`. Verify the `[req:cif command dispatch]` and option tests, with both modes for Q-MASK-413, and that no `CORE-NOTIMPL-005` remains in `cif`.
- [ ] 11.5 Keep every function's cognitive complexity at most 15 (the parser dispatch flagged at most 25 if irreducible). Verify with the cognitive-complexity tooling on `cif/`, and that `check_requirement_coverage.py --capability cif-format-io` reports every requirement covered.

## 12. SHELL: desktop GDS and CIF

- [ ] 12.1 Enable the wrapper's "Read GDS" and "Write GDS" items (drop the M5 entry of `later_milestone`). Complete the `gds` and `cif` kinds of `magic::promptload`/`promptsave` through `platform::FilePicker` with the root name (Q-MASK-320, 210), including the `cif_temp` sequence of `promptload cif`. Verify the view-model and prompt tests, that each item issues its logged command, and the `[cap:ui-gui-managers][req:File prompts]` scenario "GDS with a single top cell" (closing m4 task 9.5).
- [ ] 12.2 Host `mask::Progress` for long reads and writes with the existing running header and Interrupt card (design D16): a write's `prepare_write` on the command thread under the exclusive lock, then its `emit_write` on a worker under the shared lock, with edits queued until the write ends; reads on the command thread with frames pumped. Verify view-model tests for progress, cancel (no output file left) and an edit queued during a write, and a TSan test (tsan leg) that runs a write while a background DRC pass runs on the same hierarchy.
- [ ] 12.3 Record the M5 items in `docs/design/m5-ui-conformance.md` (implemented as, gaps). Report any element missing from every board as an open item for DESIGN. Verify that the document lists each item.

## 13. GOLDEN: MASK and related golden cases

- [ ] 13.1 Add `tools/golden/authoring/gdsgen.py` (a GDS record writer for hand-built inputs) with unit tests in `tools/golden/tests/`. Verify that its output reads in Magic in the capture image and round-trips through its own decoder.
- [ ] 13.2 Add the comparators and masks of design D18: `gds-mod-dates`, `gzip`, `gds-records` (display only), `cif-preamble`, the polygon-set canonicalizer and the `Completed N%` mask. Verify the `tools/golden/tests/` comparator tests.
- [ ] 13.3 (The use-order probe moved to task 1.5.) Add the `golden-gds-rt` case label (a `gds_rt = true` manifest key, as `slow = true` gives `golden-slow`) and make `replay.py` skip such cases unless `--include-gds-rt` is passed, and fail instead of skipping when a case listed with `--require-inputs` has no input. `ci.yml`'s golden replay passes `--require-inputs` for the three standard-cell `MASK-RT-*` cases. Verify with `tools/golden/tests/` tests for the label, the exclusion and the missing-input failure.
- [ ] 13.4 Add `authoring/{mask_cases,m5_cases}.py` and author the 233 MASK cases of `tests/golden/MASK/CASES.md`, following the session-state pinning of design D18 (fresh session, one write, `drc off`, `TZ=UTC`, `--trust-pdk` and `random seed 1` for PDK cases). The history cases are named. MASK-GW-040 becomes a unit test, and MASK-GW-032/034 use `.mag` abstract cells. Verify that `capture.py --check` accepts every case directory and `m5_cases.py --not-authored` lists only rows deferred with a reason and a target milestone. Expected: MASK-OS-016 (the contact cut size is observable only through `def write`, M6, or extraction, M7; deferred to M6), and MASK-GR-009 only if its LEF abstract cannot be a `.mag` abstract cell as for MASK-GW-032 (then M6). Task 15.6 lists the final set.
- [ ] 13.5 Remove the cifinput `Input style …` line from the `magic-tech-sections` mask and update UI-G-005's reason. Verify that the 490 cases carrying the mask still pass.
- [ ] 13.6 Author MISC-MH-03..06, MISC-FL-05 and CELL-TS-07, and remove `deferred = "M5"` from DRC-CIF-4..7. Verify `capture.py --check` and that `m3_cases.py`/`misc_cases.py`/`cell_cases.py --not-authored` list nothing for M5.
- [ ] 13.7 Add the `MASK-RT-<lib>` rows and cases of design D19: the wrapper with its timestamp pinned by `cellname timestamp <wrapper> 0`, a digest of the masked, decompressed output plus a decoded summary (with the BGNLIB creation date), and Hefesto's non-gating, order-insensitive equivalence report of a second read and write (no fixed-point gate). The three standard-cell cases carry the `golden` label and replay on every CI leg; the others carry `golden-gds-rt` (task 13.3). Provide the shard list that task 1.6's job reads. Verify that one case per PDK captures and replays locally.
  - Note (round 1): the shard list lives in `scripts/gds_rt_shards.py` (INTEGRATION), keyed by library: name each case `MASK-RT-<lib>` or `MASK-RT-<lib>-<suffix>`; a library in no shard fails the `plan` job. A case covers every file of its library (design D19 "Task 1.1 facts" lists the multi-file libraries) unless it is split by suffix. Each case's `[[external]]` inputs name the GDS set (`set = "sky130-gds"`, or `"sky130-gds-sc"` for the three standard-cell cases).
- [ ] 13.8 Capture the new and changed cases, the `MASK-RT-*` cases through task 1.6's `capture-gds-rt` job (which this task depends on). Record deviations only with their approved decisions (Q-MASK-201, 208, 209, 219, 222, 227, 314, 318, and 10 where a listing differs). Verify that `ctest -L golden` passes on every CI leg and golden-capture verify matches Magic.

## 14. PERF: GDS throughput and the round-trip job

- [ ] 14.1 Add `bench/gds/` with the workloads of design D20: read `sky130_fd_sc_hd.gds`, and write the all-cells block with and without `merge`. Add a `gds-bench` job in `perf.yml` that measures callgrind instructions and wall time for Hefesto and Magic on the same runner. Verify that a run records both tools' numbers in `docs/design/m5-ui-conformance.md` "Performance".
- [ ] 14.2 Gate `gds-bench` as Open Question 2 decides (recommended: Hefesto's instructions at or below Magic's per workload, then the baseline + 2 %). Run it on PRs touching `gds/`, `mask/`, `cif/` or `db/use_index*` and on main, schedule and dispatch. Verify that the gate passes, or record the gap for the user with a profile.
- [ ] 14.3 Add the `gds-roundtrip` job running every `MASK-RT-*` case (design D19), with the equivalence report in the job summary. Add `src/core/{include/hefesto,src}/{gds,mask,cif}/**`, `tests/golden/MASK/**`, `bench/gds/**` and `scripts/fetch_pdk.py` to `perf.yml`'s `push` and `pull_request` path filters (as an open item for INTEGRATION, task 15.1). Verify that the job passes, that a PR touching only `src/core/src/gds/` triggers it, and that its time and data use are recorded.
- [ ] 14.4 Re-run `drc-profile` after DRCCIF and INDEX land. Verify that it stays at or below 12.38 billion instructions per block pass, with the timed ratio recorded as information.
- [ ] 14.5 If Open Question 2 is answered (a), draft the amendment of `hefesto-performance` "Batch throughput relative to Magic" (callgrind instructions per workload as the gate for DRC and GDS read/write, wall time as information) and the decision record in `docs/decisions/`, as an open item that INTEGRATION applies. Verify that `openspec validate --all --strict` passes after it.

## 15. INTEGRATION

- [ ] 15.1 Apply the shared-file open items of workstreams 1–14 (CMake, `EditorState` members, CLI options, presets, the `perf.yml` and `golden-capture.yml` jobs, the `perf.yml` path filters of task 14.3, notices). Verify that every macOS preset configures, builds and tests with `-DHEFESTO_WARNINGS_AS_ERRORS=ON`.
  - Round 1 (2026-11-13): merged PDK (1.2–1.4), GOLDEN's probe (1.5) and the scaffold (2.2–2.6). Applied: `external.py` GDS sets (GOLDEN item), `scripts/d19/`, the `golden-capture.yml` jobs of tasks 1.1 and 1.6, design D2 (`tests/data/db/use_index/` to GOLDEN), D7 (the probe reads order from use-id properties), D19 (task 1.1 facts), the task 1.5 wording and the 5.2 note. Seen by PDK, not changed: `hefesto-cli pdk install -unpack-factor 32` fails with PDK-UNPACK-002 on very small archives (tar overhead exceeds 32x; `gf180mcu_ocd_alpha_{large,small}`, `sky130_ml_xx_hd`, none in a set), and unauthenticated GitHub API use hits the 60 requests/hour limit after about 20 installs, so CI keeps passing `GITHUB_TOKEN`.
- [ ] 15.2 Add the fuzz targets `fuzz_gds_reader` (plain and gzip, with the content-hash scan and an in-memory write), `fuzz_cif_reader`, `fuzz_cifoutput_section` and `fuzz_cifinput_section` (design D21), with seeds and resource caps. Wire them into the fuzz job. Verify that the seed replays pass on every leg and the 60 s fuzz job runs each target without a crash.
- [ ] 15.3 Review the new readers for malformed input (design R9): recursion bounds, structure and symbol limits, 64-bit coordinates and gzip bombs (an inflate size cap reported as a diagnostic). Write the findings in `docs/contributing/security.md`. Verify that each finding is fixed with a regression test or recorded as an accepted limit.
- [ ] 15.4 Add `--require-area MASK` to the determinism job once MASK expected outputs exist. Verify that the job is identical on the seven legs over MASK and every earlier area.
- [ ] 15.5 Verify that no `CORE-NOTIMPL-005` use or stub remains, retire the code in the table, and verify that the diagnostics tests pass.
- [ ] 15.6 Verify the M5 exit criteria end to end:
  - unit, property (tasks 4.8, 9.9, 10.10) and fuzz tests are green in CI on every leg;
  - every authored MASK golden case (the 233 rows less the deferred ones, plus the `MASK-RT-*` cases) passes or carries an approved deviation in `deviations.toml`; the deferred rows are listed by ID with their target milestone (expected: MASK-OS-016, M6; see task 13.4);
  - DRC-CIF-2..7 pass;
  - GDS output equals Magic's apart from the masked time fields on every GW case and every round trip, except the cases with an approved fix deviation, each listed with its decision: MASK-GW-003 (Q-MASK-201), MASK-GW-006 (Q-MASK-209), MASK-GW-039 (Q-MASK-219), and MASK-GW-005's label-text-after-write output (Q-MASK-208; its GDS bytes are still compared); task 13.8 records any case added to this list, with its decision;
  - every sky130, gf180mcu and IHP library GDS round-trips (`gds-roundtrip` green);
  - the determinism job covers MASK;
  - `check_requirement_coverage.py --milestone M5` covers the 194 exact and behavioral requirements;
  - `check_milestone_readiness.py --milestone M6` reports the next milestone's state;
  - `openspec validate --all --strict` passes;
  - `perf.yml` is green (`gds-bench`, `gds-roundtrip`, `drc-profile`, `golden-slow`), and the round-trip equivalence report has been reviewed.
- [ ] 15.7 Update README.md, CHANGELOG.md (mask-layer DRC errors, GDS/CIF commands, the Q-MASK-400 OFF change), docs/roadmap.md (M5 status), docs/building.md (GDS data, the time-zone setting), docs/contributing/ (the `mask`, `gds` and `cif` areas, codes, fuzz targets, golden modules, the GDS bench) and the diagnostic code documents. Verify that the documented commands run as written.
