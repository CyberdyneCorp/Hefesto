# Tasks

How this plan runs:
- **M6 starts once the M5 branch (PR #65) has merged.** M6 edits M5 code only as design D17 lists.
- Each task ends with how it is verified.
- Section 1 lands first: the reference data, the reference tools and the `.ext` probe (the user's answers were recorded on 2026-11-18, task 1.1). Workstreams 3–16 run after the scaffold, in the dependency order of design D2, and edit only what D2 assigns to them. Shared-file needs are reported as open items.
- Golden case inputs are authored by GOLDEN from the owning workstream's requirements (design D19). `expected/` is captured in CI.
- Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`. Requirements with a magic-compat dependence get one test per mode.
- The last task of each capability workstream also runs `scripts/check_requirement_coverage.py --capability <cap>` and verifies that every requirement of its capability is covered, or listed as deferred with a reason.
- Every approved decision cited below is in `docs/decisions/m6-decision-table.md`, including Q-LEFDEF-114 (compat, approved 2026-11-18).
- The design's Open Questions 1–6 were **decided by the user on 2026-11-18**: option (a) for 1, 2, 4, 5 and 6, and (b) for 3 (no source of commercial OASIS files). The tasks below follow those answers.

## 1. Answers, reference data and reference tools (first)

- [x] 1.1 (INTEGRATION) Record the user's answers to design Open Questions 1–6 and to Q-LEFDEF-114 (decision table "Pending"):
  - in the design's Open Questions;
  - in the decision table;
  - for Q-LEFDEF-114, as `Status:`/`Resolution:` lines in `docs/decisions/open-questions/LEFDEF.md`.

  If Open Question 1, 2 or 4 is answered with an option that amends a spec or the roadmap's M6 exit criteria, make that edit and adjust D2, D10 and sections 8, 9 and 16. Verify that `openspec validate --all --strict` and `python3 scripts/check_milestone_readiness.py --milestone M6` pass and that `docs/specs-index.md` is regenerated without diff.

  Done 2026-11-18: all six answered as recommended (Open Question 3 with its fallback (b)) and Q-LEFDEF-114 approved as compat. Spec and roadmap edits: `def-write` and `lef-write` name the pulled-forward `.ext` and extraction-style subsets; `oasis-format` records the OASIS-012/030/061 deviation; the roadmap's M6 deliverables and exit criteria and its M7 entry name the pull-forward and the deviation. Q-OASIS-2's judgement item is reopened with option (b) (buy SEMI P39). Design D1, D2, D4, D9, D10, D16, D19, D22 and the risks follow the answers. Readiness for M6 and M7 and `openspec validate --all --strict` pass, and `docs/specs-index.md` regenerates without diff.
- [x] 1.2 (PDK) Verify the release data:
  - list `libs.ref/<lib>/{lef,techlef}` of every library in the pinned ciel releases (sky130A and gf180mcuD at `5740c4a8…`, ihp-sg13g2 at `5e6d592e…`);
  - install the tech LEF and the standard-cell LEF into a scratch root, recording download size, kept size, peak disk and file names;
  - record whether any library ships an OASIS file and which tool wrote it (from the START/property records). Open Question 3 was decided (b), so a commercial-written file found here is a new input for OASIS-012/030/061 (task 15.6), not a precondition.

  Record the facts in design D2 (PDK row) and D19 ("Commercial OASIS files"). Verify that the documented commands reproduce them.

  As landed (2026-11-18): every asset of the three releases was streamed and listed; the facts are in design D2 "PDK row: task 1.2 facts" and in D19. The tech LEF is `techlef/<lib>__{nom,min,max}.tlef` for sky130 and gf180mcu, and `lef/sg13g2_tech.lef` for IHP. The LEF sets keep 2.0, 0.77 and 0.22 MB; the largest peak disk is 1.63 GB (gf180mcu). The only OASIS file is IHP's KLayout-written LVS test layout `SP01.oas`, and no file is from a commercial writer. `fetch_pdk.py … --root <scratch>` reproduced the digests, and a second run was a verified no-op.
- [x] 1.3 (GOLDEN) Pin KLayout and gdstk in the capture image (design D18):
  - decide the install source from facts: a Debian package at the image's APT snapshot, otherwise KLayout's upstream `.deb` for Debian 13 with a SHA-256; and a gdstk wheel pinned by version and hash;
  - add `[tools.klayout]` and `[tools.gdstk]` to `tools/golden/pins.toml` and the ARGs to `tools/golden/container/Containerfile`;
  - extend `tools/golden/tests/test_pins.py`;
  - rebase onto the M8 `gawk` change if it landed first.

  Record the versions, the source and the image size increase in design D18. Verify a CI image build whose log prints `klayout -v` and the gdstk version, and that `test_pins.py` passes.
  - As landed (round 1): KLayout is Debian `klayout 0.30.0-1+b1` at the APT snapshot (`.deb` SHA-256 checked); gdstk is the `gdstk-1.0.2-cp313` manylinux wheel (SHA-256 checked) in `/opt/ref/py` over the snapshot's `python3-numpy`. The M8 `gawk` change had not landed, so there was nothing to rebase onto. The CI image builds of golden-capture runs 44289108952 (ext-inputs) and 44289117039 (oasis-validate) print `KLayout 0.30.0` and `gdstk 1.0.2`; the image is 1,635,348,350 bytes (D18; the increase stays the Packages-index estimate, since no earlier run recorded the size). `test_pins.py` passes.
- [x] 1.4 (PDK) Add `sky130-lef`, `gf180mcu-lef` and `ihp-lef` to `scripts/fetch_pdk.py` (Open Question 6 (a)): pins, `-only` subsets, tree digests, cache keys and `--print-digest`. Add their paths to `tests/unit/common/reference_data.hpp` (nullopt when missing, so tests skip), with tests in `scripts/tests/test_fetch_pdk.py`. Restore and fetch them only in the jobs that use them. Verify that a fresh install matches the digests, a second run is a verified no-op, and the `ci.yml` legs fetch only the sets their golden cases replay.
  - As landed: the three sets are under their own PDK root `.cache/pdks/lef`, with keys `pdk-<set>-<pin12>-<digest16>`, and a bare run does not fetch them. `reference_data.hpp` gains `PinnedLef`, `pinned_lef_library`, `pinned_tech_lef` and `pinned_cell_lef`, and the constants `kSky130FdScHdLef`, `kGf180mcu7t5v0Lef` and `kSg13g2StdcellLef` (nullopt when a set is missing). There are 7 new tests in `test_fetch_pdk.py`. CI is wired but off: `FETCH_LEF_SETS: "false"` in the `env` of `ci.yml` (`reference-data` looks up, fetches and saves the sets; the build legs restore and verify them) and of `golden-capture.yml` (`capture` restores, fetches and saves them). INTEGRATION sets it to `"true"` in `ci.yml` with the first PDK LEF unit test (3.4 or 5.7) or the `LEFDEF-RT-<lib>` cases (15.5), and in `golden-capture.yml` with those cases. Until then no CI job fetches a LEF set.
- [x] 1.5 (PDK) Update `docs/building.md` "Reference data for tests" with the LEF sets, and "Golden capture" with the reference tools. Verify that the documented commands run as written.
  - Round 1: "Reference data for tests" has the LEF sets (new subsection "LEF data sets (M6)"), and its commands ran as written. Open: `docs/building.md` has no "Golden capture" section, and the KLayout and gdstk pins are task 1.3 (GOLDEN). That part follows task 1.3.
  - Round 1 integration: INTEGRATION added the "Golden capture" section to `docs/building.md` (the image's tools with the KLayout 0.30.0 and gdstk 1.0.2 pins of task 1.3, the `ext-inputs` and `oasis-validate` dispatch modes, pointing to `tools/golden/README.md`). The section's commands are the dispatches of tasks 1.6 and 1.7; both ran as written (runs 44289108952 and 44289117039), and `--sync-ext` on the ext-inputs artifact ran as written.
- [x] 1.6 (GOLDEN) Add the `.ext` probe (Open Question 1, decided (a)). A capture mode `mode=ext-inputs` of `golden-capture.yml` runs Magic's `extract all` on the layout of each DEF-write net case (LEFDEF-317..328, 330..334, and the PDK DEF cases). `tools/golden/authoring/lefdef_cases.py --sync-ext` copies the `.ext` files into each case's `input/` and into `tests/data/extflat/<case>/`, with the probe script committed. Verify that a dispatch run produces the `.ext` files, that they are committed with their script, and that every file ends with Magic's last record and has no `error` line.
  - As landed (round 1): golden-capture run 44289108952 (`mode=ext-inputs`) passed. Magic extracted the smoke layout (`tests/data/extflat/smoke/input/`, the `extsmoke` technology's first run in Magic) and `goldenlib/extfile.py` accepted `leaf.ext` and `top.ext` (complete records, no `error` line, clean probe log). `--sync-ext` committed them with `run.tcl` and `probe.log` in `tests/data/extflat/smoke/`. The 17 DEF-write net cases (LEFDEF-317..328, 330..334) are listed as skipped, "not authored": each becomes a target automatically once task 15.1 authors its `input/` (one `*.tech`, a `commands.tcl` that loads the root cell). The PDK DEF cases of task 15.5 need their own target rule (no local `*.tech`); the probe is re-dispatched then.
- [x] 1.7 (INTEGRATION, with GOLDEN) Add the dispatch mode `oasis-validate` to `golden-capture.yml` (design D18). It builds Hefesto, writes the OASIS-05x outputs, the strict-mode files and the 419-byte file, and runs KLayout (`strm2gds`, `strmxor`, `strmcmp`, the reader with `expect_strict_mode`) and gdstk (`oas_validate`, read). The verdicts are a job summary and an artifact. Verify with a dispatch run on the scaffold that the tools run and report, with Hefesto's OASIS steps skipped until they exist.
  - As landed (round 1): golden-capture run 44289117039 (`mode=oasis-validate`) passed on the scaffold. KLayout 0.30.0 (`klayout -b`) and gdstk 1.0.2 checked the gdstk and KLayout smoke files: strict and non-strict reads, `strm2gds`, per-layer XOR and LayoutDiff against the GDS, gdstk `read_oas` and `oas_validate` for CRC32 and CHECKSUM32, and both negative controls failed as required (a strict file read as non-strict warns; the corrupted copy fails `oas_validate`). OASIS-050..059, 061 are skipped ("not authored yet") and `reference_419.oas` ("not committed yet"). The Debian package has no `strm2gds`/`strmxor`/`strmcmp` programs, so their operations run as a KLayout batch script through KLayout's API (D18). `produce` stops skipping a case once its input exists and `oasis write` works; the workflow needs no change. Per-output expectations (OASIS-057 strict mode, OASIS-058 signatures, OASIS-051 XOR against Magic's GDS) go in `oasis_validate.CASE_CHECKS` with tasks 15.x; OASIS-060, if added, needs the PDK restore steps in the job.

## 2. Scaffold

- [x] 2.1 Create the change `m6-lef-def-oasis` (README, proposal, design, tasks, `skip_specs: true`). Verify that `openspec validate m6-lef-def-oasis --strict` and `openspec validate --all --strict` pass.
- [x] 2.2 Add the areas `lef` (`lex`, `tech`, `via`, `read`, `write`, `command`), `def` (`read`, `write`) and `oasis` (`codec`, `read`, `write`, `command`), plus `extflat` and `extract/tech` (Open Questions 1 and 2, decided (a); `extract/tech` may already exist from M10's subset). Each subdirectory gets its `sources.cmake` and `codes.inc`, plus the INTEGRATION-owned assemblers. Add the test directories and `docs/diagnostics/codes/{lef,def,oasis}.md`, one section per owning workstream (design D1). Verify that the build passes and the test "each area's code table matches its document and prefix" covers the new areas.
  Done 2026-11-18 (branch `m6-r1-areas`): `lef`, `def`, `oasis` and `extract` have INTEGRATION-owned `sources.cmake`/`codes.cpp` assemblers and one `sources.cmake` and `codes.inc` per subdirectory; `extflat` is one EXTSUB directory (`sources.cmake`, `codes.inc`) whose `codes.cpp` is INTEGRATION's. The code areas are `LEF`, `DEF`, `OASIS`, `EXTFLAT` and `EXTRACT`, documented in `docs/diagnostics/codes/{lef,def,oasis,extflat,extract}.md` with one section per owning workstream. `tests/unit/{lef,def,oasis,extract}/<sub>/` and `tests/unit/extflat/` exist and build an executable once they hold a test. The macos-debug build passes with `-Werror`, and "each area's code table matches its document and prefix" covers the five areas.
- [x] 2.3 Write the documented public headers of design "Area contracts" with stubs, and add `CORE-NOTIMPL-006`. Register `lef` and `def` through one dispatcher with Magic's option lookup (design D3, D14), and `oasis` with its full option table. Unimplemented options fail with `CORE-NOTIMPL-006`. Add the `def` and `oasis` help lines to `ui/magic_help.cpp`. Verify that the headers compile standalone with `-Werror` in `macos-debug`, and that a `[m6-scaffold]` test lists every `lef`, `def` and `oasis` option and Magic's invalid-option message.
  Done 2026-11-18 (branch `m6-r1-areas`): the "Area contracts" headers outside task 2.5's D4 interfaces, with stubs (`lef/read/find_file`, `lef/via/{cut,regions}`, `lef/write/{macro,number}`, `oasis/codec/{validate,cblock}`, `oasis/read/{stamps,find_file}`, `oasis/write/{emit,transcode,repetitions}`, `oasis/command/{command,settings,format}`, private `gds/write/vendor_oasis.hpp`); they forward-declare the 2.5 types `lef::LayerRecord` (struct), `lef::LayerTable` and `oasis::Encoder` (classes). `lef`/`def` share one dispatcher with Magic's two tables, `def writeall`'s refusal and the "only for LEF" answers; `oasis` has 34 options, and an unknown or ambiguous one is a `CMD-USAGE-001` error. `magic_help.cpp` already had the `lef` and `def` lines and gets no `oasis` line: it holds Magic's help strings, printed with magic-compat ON (UI-G-005); `oasis` is listed from its own help text with it OFF. The new headers compile standalone with `-Werror` (Clang, `-O0`/`-O3`); `tests/unit/core/m6_scaffold_test.cpp` covers the options and the invalid-option message.
- [x] 2.4 Split `gds::GdsSettings` (design D15):
  - `stream::StreamReadSettings` gets the 11 shared read fields;
  - `MaskSession` gains `stream_read` and `oasis`;
  - every `gds` option handler and every reader of a moved field is updated.

  The split is behaviour-preserving. Verify that every `tests/unit/gds/` test, the `[m5-scaffold]` test and every MASK golden case that runs locally pass unchanged, and that `gds readonly`, `gds flatten` and `gds ordering` queries return the same values.
  - As landed (round 1): `stream::StreamReadSettings` and `stream::PolygonSubcells` in `mask/stream_read.hpp` (INTEGRATION; `gds::PolygonSubcells` is an alias), holding the 11 options (12 fields with `flatten_limit`); `GdsSettings` keeps `magscale`, `rewound` and the write options. `MaskSession::stream_read` added; `GdsReadOptions` gained `stream_read`, which `gds read` copies from the session and the GDS reader reads (`read_detail::Reader::stream_read()`). `MaskSession::oasis` was not added in this step (`oasis::OasisSettings` belongs to the `oasis/command/` header of task 2.3); the round 1 integration added it (task 2.5). Tests and the GDS fuzz target follow the field move with renames only. `[m6-scaffold]` (`tests/unit/core/m6_shared_interfaces_test.cpp`) checks that the options write `stream_read`, leave `GdsSettings` at its defaults and query as before. Full ctest and the local golden replay (224 MASK cases pass, the rest skip for missing reference data) pass.
- [x] 2.5 Add the shared interfaces of design D4 as stubs:
  - `lef::LefSession` in `EditorState`;
  - `Lexer`, `MessageCounter`, `LayerTable`, `ViaGenerator`, `paint_polygon`;
  - `oasis::Decoder`, `Encoder`, `ModalState`, `Repetition`, `Element`;
  - `extflat::read_flat` and `extract::StyleView` (Open Questions 1 and 2 (a));
  - the read and write entry points.

  Verify that the CLI, shell and core tests pass unchanged.
  - Partly landed (round 1, Scaffold B): `extflat::read_flat`, `Netlist`, `FlatNode`, `FileStamp` (`extflat/read.hpp`) and `extract::StyleView` with `from_section`, `gate_types`, `diff_types`, `active_types`, `substrate_types` (`extract/tech/style_view.hpp`), as header-only stubs (no area sources needed): `read_flat` fails with `CORE-NOTIMPL-006` and `from_section` gives a view with no style. `gds::prepare_write`, `emit_write` and `finish_write` already exist from M5. Still open, because they live in the `lef/` and `oasis/` areas of tasks 2.2 and 2.3: `EditorState::lefdef` (`lef::LefSession`), `MaskSession::oasis`, and the LEF/DEF and OASIS types and entry points.
  - Done in the round 1 integration (`m6-round1`): `lef::LefSession` (new INTEGRATION header `lef/session.hpp`) as `EditorState::lefdef`, `MaskSession::oasis` (`oasis::OasisSettings`), and the D4 types and entry points as stubs: `lef::Lexer`, `MessageCounter` (`lef/lex/lexer.hpp`), `LayerTable`, `LayerRecord`, `ViaTable`, `RuleTable` (`lef/tech/table.hpp`), `ViaGenerator` (`lef/via/generate.hpp`), `paint_polygon` (`lef/via/polygon.hpp`), `oasis::Decoder`, `Encoder`, `ModalState`, `Repetition`, `Element` (`oasis/codec/{decoder,encoder,modal,elements}.hpp`), `lef::read`, `lef::write`, `lef::write_all`, `def::read`, `def::write`, `oasis::read`, `oasis::write` and `oasis::to_gds_write_options` (implemented: it maps the shared write options). The `extflat::read_flat` stub moved into `src/core/src/extflat/read.cpp` and uses `cmd::not_implemented_diagnostic`. The class keys match the forward declarations of task 2.3 (`struct LayerRecord`, `class LayerTable`, `class Encoder`). Five more `[m6-scaffold]` cases in `tests/unit/core/m6_shared_interfaces_test.cpp`; the CLI, shell and core tests pass unchanged. Differences from D4: design "As-landed scaffold contracts".
- [x] 2.6 Record the as-landed file and owner table and every difference from "Area contracts" in design.md ("As-landed scaffold contracts"), and seed `docs/design/m6-ui-conformance.md`. Verify that `openspec validate --all --strict` passes.
  - Done in the round 1 integration: design "As-landed scaffold contracts" (files and owners; differences from "Area contracts" and D4, including the `StreamReadSettings` count, the `lef/session.hpp` and `oasis/codec/modal.hpp` headers, the `StyleView` namespace against M10, the `lef`/`def` window check, and no `oasis` line in `ui/magic_help.cpp`). `docs/design/m6-ui-conformance.md` is seeded with the D16 rows (all todo or blocked). `openspec validate --all --strict` passes.

## 3. LEFTECH: lef-tech-section

- [x] 3.1 Port lefTech.c into `LayerTable::from_section`:
  - the keyword grammar, magic type resolution and class mapping;
  - aliases and canonical names;
  - the tech-load messages ("Unknown LEF section keyword: %s.  Line ignored." and the others).

  Use `util::MagicHashTable` with the signed-byte hash (Q-LEFDEF-82, 92, 116). Verify the `[cap:lef-tech-section]` tests for "Lef section line grammar", "Magic type resolution in lef lines", "Keyword to LEF class mapping" and "LEF name aliasing and canonical names", and a hash-order test against the M1 hash model.

  As landed:
  - The parser is `src/lef/tech/section.cpp`, with the codes LEF-TECH-001..008.
  - `from_section` gained an overload that takes the lambda-grid DRC rules and the compat setting. The 3-argument scaffold form reads with no rules and magic-compat ON.
  - `LayerRecord` gained `obstruction_type` (Magic's obsType).
  - `magic_to_lef(type)` keeps its signature: the table stores the technology's contact types.
  - `add`, `find_mutable` and `release` were added for LEFR, LEFVIA and DEFW.
  - More than two types: magic-compat ON copies Magic, whose loop stops at the second type. OFF reports `Too many types`.
  - Two undefined-behaviour paths are fixed in both modes: an `ignore` line naming a mapped name, and the type name read at index -1. The spec is updated.
- [x] 3.2 Implement the defaults and their fill with explicit unset flags: route width and spacing from `drc::default_width`/`default_spacing`, contact areas, and the fill after grid scaling (Q-LEFDEF-1, 2 per mode; Q-LEFDEF-5 fix). Verify the `[req:Default routing parameters]`, `[req:Default contact area]` and `[req:Filling defaults after grid scaling]` tests, with one test per mode for 1:1, 3:2, 2:1 and shrinking grids.

  As landed:
  - With magic-compat ON a record holds Magic's numeric sentinels; with it OFF an unset value is `std::nullopt` (documented in the header; the spec is updated).
  - An obstruction-only record fills with 0 in both modes, where Magic reads out of bounds.
  - Creating the Q-LEFDEF-5 LEF LAYER records is LEFR's work, through `add`.
- [x] 3.3 Implement `scale(n, d)` (truncating route values, flooring contact areas, no refill) and the table lifecycle: rebuild at tech load, kept across reads, and the hooks in `tech/loader.cpp` and `tech/scale_layout.cpp`. Verify the `[req:Scaling the LEF table]`, `[req:LEF table lifecycle]` and `[req:Use of entry classes by readers and writers]` tests, and that `scalegrid 1 2` gives the LEFDEF-004 values.

  As landed:
  - The hooks are in `lef/tech/lifecycle.{hpp,cpp}`: `read_layer_section`, `install_layer_table` and `refill_layer_table`.
  - They are called from `tech/commands.cpp`, where the session installs a technology (`load_session_technology`, `install_technology`), rather than from `tech/loader.cpp`, and from `tech/scale_layout.cpp`.
  - The magic-compat OFF refill after `scalegrid` runs after the DRC rescale.
  - The LEFDEF-004 values come from Magic's arithmetic, computed by hand. Comparing them with Magic's capture is GOLDEN's work.
- [x] 3.4 Load the sky130A, gf180mcuD and ihp-sg13g2 `lef` sections at the M4 pins without new diagnostics. Verify a unit test per PDK that compares the record count and the canonical names with the PDK tech file.

  As landed: `tests/unit/lef/tech/pdk_test.cpp` checks 50, 58 and 14 LEF names in 14, 11 and 14 records. Each test skips when its data set is missing.
- [x] 3.5 Keep every function's cognitive complexity at most 15 (flag the irreducible ones). Verify with the cognitive-complexity tooling on `lef/tech/`, and that `check_requirement_coverage.py --capability lef-tech-section` reports 10 of 10.

  As landed: the highest score is 9 (`new_record`), and coverage is 10 of 10.

## 4. LEFVIA: via geometry, shared by LEF and DEF

- [ ] 4.1 Implement the LEF VIA and VIARULE storage and redefinition: shared entries, aliases and class VIA (Q-LEFDEF-8 fix). Verify the `[req:LEF VIA and VIARULE definitions]` and `[req:Via redefinition]` tests (lef-read and def-read).
  - Partly landed (`m6-r2-lefvia`): `lef/via/store.hpp` (`begin_via`, `shared_names`, `add_via_rect`) and `ViaDefinition` extended in `lef/tech/table.hpp` (class, normal and obstruction type, half-unit area, extra list head first, canonical name, ref count); tested with hand-built rectangles (`tests/unit/lef/via/store_test.cpp`). Open: the shared-alias case needs LEFTECH's `LayerTable` records and a LayerTable call to move the canonical name; the parser-driven tests need LEFR and DEFR.
  - Round-2 integration (`m6-round2`): `tests/unit/lef/via/layer_table_test.cpp` runs `begin_via` and `shared_names` over a table read by LEFTECH's `read_layer_section` (three aliases of a cut, a two-name route, a single-name route, a VIARULE on a layer name). Still open: the Q-LEFDEF-8 canonical-name move needs a `LayerTable` operation (e.g. `redefine_alias(name)`: drop `name` from the names and, when the shared record's canonical name is `name`, take another alias in hash order) and a writable table in `begin_via`; the parser-driven scenarios wait for LEFR's VIA/VIARULE dispatch (`Reader::via_section`, `Reader::via_definition` are still skip-to-END stubs).
  - Open (DEFW, decision needed): Magic keeps layers and vias in one LefInfo hash table; Hefesto splits `LayerTable` and `ViaTable`, so the def-write "Table enumeration order" (VIAS order with magic-compat ON, growth after 96 entries) can differ from Magic. Either a combined-order enumeration or a decision.
  - Open (LEFVIA): the DEF 5.8 PATTERN reading (Q-LEFDEF-62 OFF; `cut_pattern`) is unchecked against the LEF/DEF reference or a third-party reader.
- [x] 4.2 Implement cut growing to `mask::OutputStyle::contact_size` (the output style, Q-LEFDEF-17) with Magic's warnings. Verify the `[req:Cut growing]` tests and LEFDEF-011's warning text in a unit test.
  - Done (`m6-r2-lefvia`): `grow_cut(cut, type, named_type, ViaContext)` over `OutputStyle::contact_cut`, `LEF-VIA-003/004`. With magic-compat ON the first warning names the entry's normal type, as Magic prints it (lef-read "Cut growing" updated). `tests/unit/lef/via/cut_test.cpp`.
- [ ] 4.3 Implement `ViaGenerator::generated`: CUTSIZE, LAYERS (with the layer-order warning), CUTSPACING, ENCLOSURE and ROWCOL. Omitted parameters take the VIARULE values or 0 (Q-LEFDEF-61 fix). ORIGIN, OFFSET and PATTERN apply with magic-compat OFF only (Q-LEFDEF-62), as do via-rule vias (Q-LEFDEF-7 OFF). Verify the `[req:Generated via geometry]` and `[req:Generated DEF vias]` tests, one per mode for Q-LEFDEF-7 and 62.
  - Partly landed (`m6-r2-lefvia`): `ViaGenerator::generated(params, ViaContext)`, `order_layers`, `complete_params` (Q-LEFDEF-61, `LEF-VIA-007`), `apply_generated` and `cut_pattern` in `lef/via/generate.hpp`; `GeneratedViaParams` holds resolved layer types and the input scale. ORIGIN, OFFSET and PATTERN are tested per mode (`tests/unit/lef/via/generate_test.cpp`). Open: the Q-LEFDEF-7 per-mode test needs LEFR's reader.
- [x] 4.4 Implement `paint_polygon` over `db::decompose_polygon`, with port labels from the single decomposition (Q-LEFDEF-23). Verify that LEF POLYGON, DEF pin POLYGON (Q-LEFDEF-66 OFF) and DEF route POLYGON (Q-LEFDEF-70 OFF) give identical tiles for the same vertices.
  - Done (`m6-r2-lefvia`): `paint_polygon_rects`, `paint_polygon` (optional `PortLabel`) and `polygon_rects` in `lef/via/polygon.hpp`, taking the technology. `tests/unit/lef/via/polygon_test.cpp`.
- [x] 4.5 Implement the DEF-write side:
  - contact-region identification, with Q-LEFDEF-111 per mode and Q-LEFDEF-106 fixed;
  - generated-via naming, with the `_hx`/`_vx` suffixes and Q-LEFDEF-105 per mode;
  - the cut array, with Q-LEFDEF-107 per mode;
  - a residue with no LEF mapping is skipped with a warning (Q-LEFDEF-115).

  Verify the `[req:Contact region identification]` and `[req:Generated via definitions]` tests.
  - Done (`m6-r2-lefvia`): `contact_regions`, `region_via`, `via_name`, `register_vias` (VIAS count, position index) and `via_rects` (VIAS RECTs and cut array, `LEF-VIA-009/010/011`) in `lef/via/regions.hpp`, through `DefViaContext`. `tests/unit/lef/via/regions_test.cpp`. def-write "Contact region identification" now follows Magic's RT/TR walk (its L-shaped scenario was wrong for magic-compat ON); "VIAS section content" states the Q-LEFDEF-115 fix.
- [x] 4.6 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `lef/via/`.
  - Done (`m6-r2-lefvia`): clang-tidy `readability-function-cognitive-complexity` with the build's compile database: worst 11 (`region_via`).

## 5. LEFR: lef-read, the tokenizer and the lef read command

- [x] 5.1 Port LefNextToken, LefEstimate, LefError and LefParseEndStatement into `Lexer` and `MessageCounter`: 2048-character lines, comments, quoted strings, `;` glued to numbers, the 100-message cap and the totals. Verify the `[req:LEF/DEF tokenizer]`, `[req:END statement matching]` and `[req:LEF/DEF message reporting]` tests, and LEFDEF-005/006's inputs in unit tests.
  - As landed (round 2, `m6-r2-lefr`): `Lexer` (`next`/`take` with `EolMode`, line and offset per token, `estimate` from a size hint) and `MessageCounter` (`report`, `summarize`, `Dialect`) in `lef/lex/lexer.hpp`; LookupFull, the `%f` parse, LefEndStatement, LefParseEndStatement, LefSkipSection and `MessageTarget` in `lef/lex/statements.hpp`, for DEFR too. Codes `LEF-LEX-001..003`. Fixed in both modes: the end of input is final, and a nameless skip meeting ENDEXT does not crash. `tests/unit/lef/lex/lexer_test.cpp` covers the three requirements and LEFDEF-005's input; LEFDEF-006 is in `tests/unit/lef/read/structure_test.cpp`.
- [x] 5.2 Implement unit conversion (half away from zero in binary32), layer name resolution (Q-LEFDEF-3 per mode; Q-LEFDEF-4 fix) and the top-level structure. Verify the `[req:LEF unit conversion]`, `[req:LEF layer name resolution]` and `[req:Supported LEF file structure]` tests, including the LEFDEF-007 values at .0025 boundaries.
  - As landed (round 2): `lef_scale`, `to_internal`, `to_half_internal` (`lef/read/units.hpp`; a value outside int or NaN is no value), `resolve_layer` and `magic_type` (`lef/read/layers.hpp`), and `read_stream` (`lef/read/stream.hpp`), LefRead's loop with codes `LEF-READ-001..003`. The reader works through `LayerAccess`, its view of the layer table; the adapter over `lef::LayerTable` needs LEFTECH's mutators (open item), so the tests use a hand-built table and `lef::read` stays a stub until task 5.6. VIA/VIARULE (LEFVIA 4.1) and MACRO/SITE (5.4) are stubs that skip their section. A section keyword at the end of the input stops the read (Magic dereferences NULL). LEFDEF-007's .0025 values are pinned in `units_layers_test.cpp`.
- [x] 5.3 Implement the LAYER section (Q-LEFDEF-6 per mode) and NONDEFAULTRULE (Q-LEFDEF-10 per mode; Q-LEFDEF-9 fix). Verify the `[req:LEF LAYER section]` and `[req:LEF NONDEFAULTRULE section]` tests.
  - As landed (round 2): `layer_section.cpp` and `nondefault_rule.cpp`; `NonDefaultRule` gains `name` and `layers` (`NonDefaultRuleLayer`, `RuleTarget`) in `lef/tech/table.hpp`. Fixed in both modes: a non-numeric value leaves the field unchanged (Magic uses an uninitialised float), and a RECT in a LAYER section is read for its messages and dropped (LEFVIA takes the VIA-class case with 4.1). Q-LEFDEF-6, 10 tested per mode, Q-LEFDEF-9 with a third duplicate (`R_1`).
- [ ] 5.4 Implement MACRO and SITE cell creation:
  - the coordinate offset (Q-LEFDEF-12 per mode);
  - the statements and cell properties (Q-LEFDEF-19 fix);
  - completion (FIXED_BBOX, LEFview);
  - PIN attributes (Q-LEFDEF-13, 14 per mode);
  - PORT and OBS geometry (Q-LEFDEF-15, 20 per mode; Q-LEFDEF-18 fix);
  - port labels in creation mode.
  - Round-2 integration note: LEFR's `LayerAccess` adapter over `lef::LayerTable` (`find`, `add_route`, `update_route`, `reserve`) is not written; LEFTECH's `find_mutable`/`add`/`release` landed, a `reserve(name)` (MagicHashTable insert without a record) and the DRC defaults for `add_route` are still needed. LEFR's tests use a hand-built `FakeLayers`. The RECT of a LAYER section on a VIA-class entry is read and dropped until LEFVIA's via geometry is wired in. `lef::read` (file open, totals, table init) stays the scaffold stub until 5.6.

  Verify each `[req:…]` test of these requirements, one per mode for every compat decision.
- [ ] 5.5 Implement annotation of existing cells (Q-LEFDEF-11, 21 per mode).
- [ ] 5.6 Implement `lef read` side effects and arguments (`-import` has no effect, D-5), file lookup (Q-LEFDEF-24), `lef datestamp` with the fixed stamp, and one undo transaction per read (Q-LEFDEF-22).

  Verify the `[req:Annotation of existing cells]`, `[req:lef read side effects]`, `[req:lef read arguments]` and `[req:lef datestamp]` tests, and that `undo` after a `lef read` removes every created cell.
- [ ] 5.7 Read the tech and standard-cell LEF of each PDK set (task 1.4) without errors. Verify a unit test per PDK that skips cleanly without the data, and compares the macro count and one macro's properties and labels with the LEF text.
- [ ] 5.8 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `lef/{lex,read}/`. Verify that `check_requirement_coverage.py --capability lef-read` reports 22 of 22.

## 6. LEFW: lef-write and the lef write commands

- [ ] 6.1 Implement the command surface:
  - `lef write` and its argument grammar;
  - distance option values;
  - `writeall` scope and order (Q-LEFDEF-41 through `UseIndex::enumerate_all`);
  - output file naming (Q-LEFDEF-43);
  - `lef nocheck`;
  - help.

  Verify the `[req:Command dispatch and target cell]`, `[req:lef write argument grammar]`, `[req:Distance option values]`, `[req:lef writeall arguments and scope]`, `[req:writeall macro order]`, `[req:Output file naming]`, `[req:lef nocheck net list]` and `[req:Help text]` tests.
- [ ] 6.2 Implement the output scale and database units (Q-LEFDEF-34 per mode) and the float32 `lefPrint` formatting (Q-LEFDEF-45). Verify the `[req:Output scale and LEF database units]` and `[req:Distance number formatting]` tests, with pinned values for `-0.000`, half-grid rounding and LEFDEF-102/105's units.
- [ ] 6.3 Implement:
  - the file grammar and single-cell layout;
  - the library header (PROPERTYDEFINITIONS in hash order, Q-LEFDEF-30);
  - SITE blocks (Q-LEFDEF-33 per mode);
  - `-tech` layers (Q-LEFDEF-31, 32 per mode);
  - the magic-to-LEF name.

  Verify each `[req:…]` test, one per mode.
- [ ] 6.4 Implement:
  - macro source geometry and the layer masks (type sets R, H, W, P, G, D; Q-LEFDEF-40);
  - the boundary (Q-LEFDEF-53);
  - the header lines;
  - pin enumeration (Q-LEFDEF-50) and pin headers (Q-LEFDEF-36 per mode);
  - power and ground pins (Q-LEFDEF-44 project setting);
  - antenna areas and the well set (`extract::StyleView`, EXTSUB task 9.3, Open Question 2 (a)).

  Verify the `[req:…]` tests, including the LEFDEF-118/119 antenna values.
- [ ] 6.5 Implement:
  - default, pin-only, hidden-detail (zero and positive setback, Q-LEFDEF-37, 51 per mode), degenerate-setback and top-layer pin geometry;
  - geometry record emission in tile order (Q-LEFDEF-39 per mode);
  - contact cut records (Q-LEFDEF-52);
  - PORT blocks (Q-LEFDEF-46);
  - obstructions without hiding, with zero setback and with positive setback (Q-LEFDEF-38 per mode, Q-LEFDEF-47).

  Verify each `[req:…]` test, one per mode for every compat decision.
- [ ] 6.6 Implement the macro PROPERTY lines, names written verbatim with the warning (Q-LEFDEF-35), the write side effects and the progress messages. Verify their `[req:…]` tests.
- [ ] 6.7 Keep every function's cognitive complexity at most 15, or at most 25 with a flag for the hide passes, and verify it with the tooling on `lef/write/`. Verify that `check_requirement_coverage.py --capability lef-write` reports 37 of 37.

## 7. DEFR: def-read

- [ ] 7.1 Implement the `def read` command, its context (headless, Q-LEFDEF-86; edits blocked, Q-LEFDEF-88), file lookup, tokenization with DEF keyword tables, and the top-level grammar (Q-LEFDEF-84 per mode). Verify the `[cap:def-read]` tests for "def read command", "Command context", "DEF file lookup", "Tokenization and keyword matching" and "Top-level DEF grammar and section dispatch".
- [ ] 7.2 Implement unit scaling (Q-LEFDEF-79 per mode), design cell selection (Q-LEFDEF-78 per mode) and the die area (Q-LEFDEF-77 per mode; Q-LEFDEF-68 fix). Verify their tests with LEFDEF-200..203's values.
- [ ] 7.3 Implement the VIAS section through `ViaGenerator` (Q-LEFDEF-60, 62 per mode), and COMPONENTS:
  - the placement transform;
  - unplaced components (Q-LEFDEF-85);
  - duplicate names renamed with the `.mag` rule (Q-LEFDEF-83);
  - masters loaded from `.mag`.

  Verify their `[req:…]` tests.
- [ ] 7.4 Implement PINS:
  - grammar, geometry placement and port labels;
  - Q-LEFDEF-64, 65, 66 per mode;
  - Q-LEFDEF-67, 89 fixed.

  Verify their tests, including LEFDEF-219's label sizes.
- [ ] 7.5 Implement NETS and SPECIALNETS:
  - grammar (Q-LEFDEF-69, 74 per mode);
  - layer resolution and width selection (the Q-LEFDEF-1 width fix in both modes);
  - wire segments;
  - via placement (Q-LEFDEF-63, 72 per mode);
  - other route elements (Q-LEFDEF-70, 71, 73 per mode);
  - painting and net labels;
  - annotation mode (Q-LEFDEF-81 per mode).

  Verify each `[req:…]` test, one per mode.
- [ ] 7.6 Implement NONDEFAULTRULES (Q-LEFDEF-75 fix), BLOCKAGES (Q-LEFDEF-76 fix), section count checks, diagnostics with error limits, read completion, and one undo transaction (Q-LEFDEF-22). Verify their tests.
- [ ] 7.7 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `def/read/`. Verify that `check_requirement_coverage.py --capability def-read` reports 27 of 27.

## 8. DEFW: def-write

- [ ] 8.1 Implement the command, argument validation (Q-LEFDEF-104 fix), `def writeall` refusal, output file naming (Q-LEFDEF-90 platform: in-memory net buffers, rename on success), preconditions (Q-LEFDEF-110 per mode), and output scale and number formatting. Verify their `[cap:def-write]` tests.
- [ ] 8.2 Implement the file grammar and section order, DIEAREA, the magic-to-LEF layer map, VIAS (Q-LEFDEF-91 per mode, through LEFVIA), component orientation and COMPONENTS (Q-LEFDEF-93, 94 per mode). Verify their tests.
- [ ] 8.3 Implement PINS (Q-LEFDEF-103 per mode). The unmapped-layer message is compat (Q-LEFDEF-114): Magic's swapped text with magic-compat ON, the layer name then the pin name with it OFF. Verify the `[req:PINS section]` tests, one per mode for Q-LEFDEF-103 and 114, and that LEFDEF-316 compares with no deviation.
- [ ] 8.4 Implement, from `extflat::read_flat` (EXTSUB, Open Question 1 (a)):
  - net classification (Q-LEFDEF-102 project setting) and DEF net naming;
  - record headers and connections;
  - geometry discovery (`connect::search_flat`, Q-LEFDEF-108 tile order, Q-LEFDEF-112 per mode);
  - wire segment derivation (Q-LEFDEF-95 per mode);
  - route assembly (Q-LEFDEF-96 per mode, Q-LEFDEF-97 line breaks).

  Verify their tests against the `.ext` files of task 1.6.
- [ ] 8.5 Implement NONDEFAULTRULES (Q-LEFDEF-98, 99 per mode; Q-LEFDEF-100 fix), BLOCKAGES (Q-LEFDEF-101 fix; active types from `extract::StyleView`), table enumeration order, and session state after writing. Verify their tests, including LEFDEF-334's hash rebuild.
- [ ] 8.6 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `def/write/`. Verify that `check_requirement_coverage.py --capability def-write` reports 24 of 24 (nothing is deferred to M7).

## 9. EXTSUB: netlist read and extraction-style subsets (Open Questions 1 and 2, decided (a))

Pulled forward from M7 on 2026-11-18 and built to the M7 specs in M7's area layout (design D10). The requirements stay M7's in the coverage report; each task notes which it covers. EXTSUB starts after the scaffold, with LEFTECH. LEFW's antenna areas wait for 9.3, and DEFW's net work (8.4) and BLOCKAGES (8.5) for 9.2 and 9.3.

- [x] 9.1 Port the `.ext` reader subset: EFReadFile over the `extract-file-format` grammar (read side), and the node, connection, merge and kill records of EFbuild.c (no device parameters). Verify `[cap:ext-flattening-read]` tests over the task 1.6 files and malformed inputs. List the requirements covered in this task's note, since they stay M7's in the coverage report.
  As landed (`src/core/src/extflat/`, `tests/unit/extflat/`): covers ext-flattening-read "Locating and recursively reading .ext files", "Line tokenization", "Keyword recognition", "Scale record", "Node and substrate records", "Device records" (node side only), "Parameters records" (messages only), "Equivalence, merge and kill records", "Use, port, cap, resist, attr and connect records", "Technology, style, version and resistance classes" (10 of 11; the `.res.ext` overlay is M7). Codes EXTFLAT-READ-001..012; caps in `extflat::ReadLimits` (EXTFLAT-READ-009/010/011, native). `FlatNode` gained `port`, `top_port`, `names`, `locations` for DEFW, `ReadOptions` the search path, cell directory, tech name, `globalNets`, units per lambda and known styles.
- [ ] 9.2 Port EFFlatBuild with `EF_FLATNODES | EF_NOFLATSUBCKT` (unscaled, resistance unused), EFVisitNodes order, and the name preference of `ext-flattening-names`. Verify `[cap:ext-flattening-hierarchy]` and `[cap:ext-flattening-names]` tests, and that the visit order matches Magic's on the task 1.6 cases (a probe line in each case).
  As landed: `flatten.cpp` (efFlatNodesStdCell, efFlatKills, efFlatGlob, EFVisitNodes) with the smoke files of 1.6 checked by hand; covers ext-flattening-hierarchy "Container iteration order", "Flat node construction", "Node merge semantics", "Kills", "Global name merging", "Visit orders for output" and ext-flattening-names "Hierarchical name structure", "Component hash", "Instance and array element names", "Name preference order", "Global and ground detection". Open: the visit-order comparison with Magic's probe lines needs the per-case `.ext` files of 15.1.
- [x] 9.3 Write the style selection, `device` (class and types), `antenna` tie-down, `substrate` and active-type parts of the `extract-tech-section` reader in `extract/tech/`, sharing its files with M10's subset if that landed first (design D10), and `extract::StyleView` over them. Every other keyword is skipped silently. Verify `[cap:extract-tech-section]` tests on the three PDK tech files with no new diagnostics, and the no-style case (Q-LEFDEF-40: empty sets).
  As landed (`src/core/src/extract/tech/style_reader.cpp`, `style_view.cpp`): `StyleView::from_section` (first style, or a named one) and `from_technology`; G from `mosfet`/`msubcircuit`/`fet` lines, D from `tiedown`, W from `substrate`, active types after `resist ... None` and "Non-electrical types" (mentions of every keyword's type arguments). M10's subset had not landed, so this is the style framework. No loader hand-off yet: Magic's load-time messages stay unprinted (the golden mask drops the non-electrical line). Codes EXTRACT-STYLE-001/002.
- [x] 9.4 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `extflat/` and `extract/tech/`. As landed: clang-tidy cognitive complexity, worst 13 (116 functions).

## 10. OASFMT: oasis-format

- [x] 10.1 Implement:
  - the primitives: unsigned and signed integers (Q-OASIS-1; the 65-bit overflow), reals of types 0–7, strings with their character sets, deltas, point lists, repetitions (lazy), intervals;
  - the record identifiers and the file structure.

  Verify the `[cap:oasis-format]` tests for "File structure and magic bytes", "Record identifiers", "Unsigned and signed integers", "Real numbers", "Strings and character sets", "Delta encodings", "Point lists", "Repetitions" and "Intervals", with OASIS-001..004's byte files as unit inputs.

  As landed: `oasis/codec/primitives.hpp` (span entry points; the decoder uses the same code through an internal byte input), `lists.cpp` (point lists, repetitions with `instance_count`/`offset_at`, never expanded eagerly) and `geometry.cpp` (`outline`). Where the spec is silent: a point-list type above 5 stops decoding (the record length is unknown), interval bounds are 32-bit fields, and a repetition step outside 32 bits or a farthest offset outside 64 bits drops the element (OASIS-CODEC-008); raised as spec open items. Tests: `tests/unit/oasis/codec/primitives_test.cpp`.
- [x] 10.2 Implement START and table offsets (Q-OASIS-8), END and padding, validation (Q-OASIS-2: gdstk coverage and byte order), name records and refnums, CELL, and modal variables (Q-OASIS-5). Verify their tests with OASIS-005..008's inputs.

  As landed: `decoder.cpp` (one handler per record ID; state in `decoder_impl.hpp`) and `input.cpp` (buffered file bytes with running CRC32/CHECKSUM32 and one inflated CBLOCK in front). References stay refnums in the records. Undefined refnums, cells defined twice, unknown standard properties and the strict-mode checks are reported at END through `diagnostics()`. `ModalState{}` is the reset state (x and y 0). Tests: `records_test.cpp`.
- [x] 10.3 Implement the element records: PLACEMENT, TEXT, RECTANGLE (Q-OASIS-3), POLYGON, PATH (Q-OASIS-11), TRAPEZOID, CTRAPEZOID (Q-OASIS-4, all 26 types), CIRCLE. Then PROPERTY (Q-OASIS-6), standard properties, extension records, PAD, CBLOCK (Q-OASIS-9, inflate cap) and strict mode (Q-OASIS-7). Verify each `[req:…]` test with OASIS-009..015's inputs.

  As landed: `decoder_elements.cpp`. The modal state after a record is a function of the resolved element, which the encoder mirrors. PROPERTY records carry a `PropertyTarget`. A CBLOCK whose declared size is over `DecoderOptions::inflate_limit` (1 GiB) is refused from its header. Codes OASIS-CODEC-001..028 are in `docs/diagnostics/codes/oasis.md`.
- [x] 10.4 Implement the `Encoder`: minimal forms, modal omission, strict and non-strict tables, CBLOCK at a level, and END with validation. Verify that decode(encode(x)) = x over seeded random records, and that encode(decode(f)) = f for every hand-assembled file written in minimal form.

  As landed: `encoder.cpp`, `encoder_elements.cpp`, with `standard_properties`, `file_property`, `layer_name` and `property` added to the scaffold's four methods. TRAPEZOID is always record 23 and the RECTANGLE S form is never written. A rejected element leaves the modal state unchanged. Round trips: 20 seeds of 180 random elements, strict and non-strict, compress 0 and 6; encode(decode(f)) = f for the reference file and a minimal non-strict file of every element kind (`encoder_test.cpp`).
- [x] 10.5 Write `tests/data/oasis/codec/make_reference.py` (with tests) to assemble the 419-byte file from the "Complete annotated example" listing, and commit the file and its SHA-256. Verify the `[req:Complete annotated example]` test: 419 bytes, the decoded cell as listed, and identical re-encoding.

  As landed: the script parses the spec listing (each row checked against its offset, the listed CRC32 recomputed) and writes `reference_419.oas` and `reference_419.oas.sha256` (`8606cf40…12a89`); `--check` verifies them. `test_make_reference.py` (6 tests) runs with `python3 -m unittest discover -s tests/data/oasis/codec`; wiring it into `ci.yml` is an INTEGRATION open item. The encoder writes the file byte for byte.
- [x] 10.6 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `oasis/codec/`. Verify that `check_requirement_coverage.py --capability oasis-format --classes native` reports 30 of 30 (all native).

  As landed: clang-tidy `readability-function-cognitive-complexity` over the compile database reports a maximum of 14 (`Decoder::Impl::property`). Coverage reports 30 of 30.

## 11. OASR: oasis-read

- [ ] 11.1 Implement the read command and file lookup (`.oas`, `.oas.gz`, `.oasis`, none), two-pass decoding and name resolution (Q-OASIS-34), units and input scale, and coordinate conversion through `mask::ReadEngine`. Verify their `[cap:oasis-read]` tests.
- [ ] 11.2 Implement:
  - cell processing and timestamps (Q-OASIS-30);
  - layer and datatype mapping (Q-OASIS-38, 39);
  - shape repetitions (Q-OASIS-10);
  - RECTANGLE, POLYGON, TRAPEZOID, CTRAPEZOID, CIRCLE (Q-OASIS-31: the pinned N = 64 vertices) and PATH (Q-OASIS-33 per mode);
  - TEXT.

  Verify each `[req:…]` test.
- [ ] 11.3 Implement placement transforms (Q-OASIS-12), placement repetitions (the duplicate-use rule), instance properties (61/99/98), post-order reading and flattening options (`stream_read` settings), unsupported and extension content (Q-OASIS-40), top cell selection, the read option set, and diagnostics. The gds-read helpers are reached through INTEGRATION's shared header (design D17). Verify their tests.
- [ ] 11.4 Implement read-only vendor views: `OASIS_FILE`/`OASIS_CELL`, the units check, the `db/property.cpp` read-only rule and LEFview annotation. Implement the content-hash timestamps over decompressed cell bytes, with gds-read's shared `stamp(s)`. Verify the `[req:Read-only OASIS vendor views]` and `[req:Content-hash timestamps for read-only OASIS cells]` tests, including equal stamps for a plain and a CBLOCK-recompressed copy.
- [ ] 11.5 Implement the host mechanisms used by the reader, and run the GDS-equivalence cross-check as a unit property test: for seeded random GDS layouts L, write L as GDS and as OASIS with Hefesto, read each into a fresh session, and compare the `.mag` output. Verify the `[req:GDS-equivalence cross-check]` and `[req:Host mechanisms used by the reader]` tests.
- [ ] 11.6 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `oasis/read/`. Verify that `check_requirement_coverage.py --capability oasis-read` reports 15 of 15, and every native requirement covered.

## 12. OASW: oasis-write and the GDS transcoder

- [ ] 12.1 Implement the write command and file naming, the option settings, and the three-phase write over `gds::prepare_write`, `emit` and `gds::finish_write` (design D4). Verify the `[req:Write command and output file naming]` and `[req:Write option settings]` tests, and that a cancelled write leaves no file.
- [ ] 12.2 Implement:
  - GDS-consistent content, file layout and units;
  - refnums and name tables (strict and non-strict);
  - cell names (Q-OASIS-61) and string mapping (Q-OASIS-60);
  - boundary translation (Q-OASIS-62);
  - modal field omission;
  - text translation (Q-OASIS-32 warning);
  - placements, array placements and reference properties (Q-OASIS-69).

  Verify each `[req:…]` test.
- [ ] 12.3 Implement shape repetition detection (Q-OASIS-67), CBLOCK compression (Q-OASIS-64 pinned parameters), strict and non-strict modes, standard properties (Q-OASIS-66), layer names, END and validation (Q-OASIS-65), and determinism. Verify their tests and byte equality across every CI leg.
- [ ] 12.4 Implement the read-only GDS cell transcoder (Q-OASIS-63: odd width is an error, round ends become CIRCLEs) and the read-only OASIS cell re-encoder (Q-OASIS-40), with write result and error propagation and the full-dump prefix (Q-OASIS-68). Verify their tests.
- [ ] 12.5 Implement `gds/write/vendor_oasis.cpp` (Q-OASIS-35): transcode read-only OASIS cells into GDS elements. Layer or datatype above 32767, or a polygon above 8191 points, is an error. Verify the `[cap:gds-write][req:Read-only OASIS vendor cells]` tests, with layer 32767 written and 32768 failing, and that every M5 `gds-write` test passes unchanged.
- [ ] 12.6 Verify the `[req:Complete annotated example]` test: `oasis write` of the reference cell gives the 419 bytes of task 10.5.
- [ ] 12.7 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `oasis/write/`. Verify that `check_requirement_coverage.py --capability oasis-write` reports 9 of 9, and every native requirement covered.

## 13. OASCMD: oasis-command

- [ ] 13.1 Implement the command name and forms, option syntax, the options shared with `gds` (through `stream_read`) and the OASIS-only options. Verify the `[cap:oasis-command]` tests, including "`gds readonly true` then `oasis read` marks the read cells read-only" (Q-OASIS-90).
- [ ] 13.2 Implement format recognition on open (`sniff_format` after gzip, in `app/`) and scripting access with query results. Verify the `[req:Format recognition on open]` and `[req:Scripting access]` tests, including `chip.gds` holding OASIS.
- [ ] 13.3 Verify that `check_requirement_coverage.py --capability oasis-command` reports 2 of 2, and every native requirement covered.

## 14. SHELL: desktop (Open Question 5, decided (a))

- [ ] 14.1 Route the desktop File > Open picker and drag and drop through the generic open, so OASIS is recognised by content. Verify a shell test that opens an OASIS file named `.gds`.
- [ ] 14.2 Run LEF, DEF and OASIS reads and writes through the M5 progress and cancel hosts. Verify shell tests for progress and for a cancelled OASIS write leaving no file. Record the items in `docs/design/m6-ui-conformance.md`.
- [ ] 14.3 Verify that the desktop adds no LEF, DEF or OASIS menu item or board (Open Question 5 (a)), and that `docs/design/m6-ui-conformance.md` says so.

## 15. GOLDEN: LEFDEF and OASIS cases

- [ ] 15.1 Add `authoring/{lefdef_cases,oasis_cases,m6_cases}.py` (with `--list`, `--not-authored`, `--exit-check`) and author the 143 LEFDEF rows, following the session-state pinning of design D19. Use synthetic per-family tech files, and the PDK LEF sets for the PDK cases. Verify that `capture.py` captures every authored case in `golden-capture.yml`, and that `m6_cases.py --not-authored` lists only rows deferred with a reason.
- [ ] 15.2 Add `authoring/oasgen.py` (a clean-room OASIS writer from `oasis-format`, with tests) and `goldenlib/oasis.py` (a clean-room decoder, with tests against the 419-byte file). Author the 51 OASIS rows. OASIS-020/021 take `strm2oas` of the sky130 standard-cell GDS as captured input. Rewrite OASIS-091 to Q-OASIS-90 (a) in `tests/golden/OASIS/CASES.md` and in its case. Verify capture and `--not-authored` as in 15.1.
  - Round 1 note (PDK, optional): IHP's `SP01.oas` (26,667 bytes, KLayout-written, with CBLOCKs and `S_GDS_PROPERTY`; in the 434 MB ihp `common` asset, design D2) could be an extra KLayout-written reader input from a real PDK for OASIS-02x/03x, if its licence allows its use as a test input.
  - Round 1 note (GOLDEN): the Debian KLayout package has no `strm2gds`/`strmxor`/`strmcmp` programs, so D18 runs their operations as a KLayout batch script through KLayout's documented API. `tests/golden/OASIS/CASES.md` still names the programs; align the wording when authoring the rows.
  - As landed (round 2, tools only; the 51 rows and OASIS-091 are open): `authoring/oasgen.py` (primitives, every record, `Builder` with table offsets and the 256-byte END and signature, `reference_419()`) and `goldenlib/oasis.py` (`records`, `decode`, `dump`). `tests/test_oasgen.py` checks the writer against the spec's byte listings; `reference_419()` equals the "Complete annotated example" listing byte for byte, CRC32 `0xEE7E3BD3`. `tests/test_oasis.py` covers the spec's scenarios. Round-2 integration: OASFMT's committed file is now `tests/data/oasis/codec/reference_419.oas`, and `CommittedReferenceTest` runs: it equals `oasgen.reference_419()` byte for byte and `goldenlib/oasis.py` decodes it (Hefesto's C++ decoder and encoder are checked on the same file in `tests/unit/oasis/codec/`).
- [x] 15.3 Add the comparators `oasis-records`, `stream-geometry`, `lef-text` and `def-text` (design D19), with tests in `tools/golden/tests/`. Verify that `stream-geometry` finds a one-unit shift on one layer and accepts a reordered, re-fractured equivalent.
  - As landed (round 2): `compare/stream_geometry.py`, `compare/lefdef_text.py`, and `oasis-records` from `goldenlib/oasis.py` `dump` (also a display view, added to `goldenlib/case.py` `DISPLAY_VIEWS`). `stream-geometry` takes GDS or OASIS on either side and flattens top cells. Manhattan geometry is merged through `polygon_set.canonical_rects`; other polygons are a canonical list. `lef-text`/`def-text` drop the D19 progress lines and Magic's startup preamble. `tests/test_m6_compare.py` covers the one-unit shift on layer 2, a reordered and re-fractured GDS equivalent (an L polygon in place of two rectangles, a split bar, an inner overlap), a GDS/OASIS equivalent, CBLOCK and repetitions, SREF/AREF/PLACEMENT flattening, and path outlines. The README's "Comparators and masks" lists them.
- [ ] 15.4 Author MASK-OS-016 with `lef write` of a contact cell. Drop `def, lef` from UI-G-005's deviation. Add a startup case that loads a `.lef` from the command line. Verify the replay.
  - Round 1 integration: `def, lef` is dropped from UI-G-005's deviation text in `tests/golden/deviations.toml` (both commands are registered by task 2.3, so Magic's help lines print). UI-G-005 stays a deviation for the M7, M10 and M12 commands. MASK-OS-016 and the startup case are open.
- [ ] 15.5 (Open Question 6, decided (a)) Add one `LEFDEF-RT-<lib>` case per standard-cell library (tech LEF and cell LEF read, then `lef writeall`), and LEFDEF-240 on a small OpenROAD-style sky130 DEF. Verify their capture, and that `ci.yml`'s golden legs replay them with `--require-inputs`.
  - Round 1 integration notes: `tools/golden/goldenlib/external.py` `PDK_SETS` now has `sky130-lef`, `gf180mcu-lef` and `ihp-lef`, so a case can use `[[external]] set = "<pdk>-lef"` (test in `tools/golden/tests/test_external.py`). With these cases, INTEGRATION sets `FETCH_LEF_SETS: "true"` in `golden-capture.yml` (the capture job restores, fetches and saves the sets) and in `ci.yml` (also with the first PDK LEF unit test of 3.4 or 5.7), and adds the case IDs to `replay.py --require-inputs` in `ci.yml`'s "Replay golden cases into golden-out/" step. The task 1.6 probe needs its own target rule for the PDK DEF cases, which have no local `*.tech` file.
- [ ] 15.6 (Open Question 3, decided (b)) Author OASIS-012, 030 and 061 with their gdstk- and KLayout-written inputs and readers only. Record each case's approved deviation (Q-OASIS-2, amended 2026-11-18: the commercial writer's or reader's input is missing) in its case notes and its `tests/golden/OASIS/CASES.md` row, not in `deviations.toml`, since the cases pass (design D19). Verify their capture, and that `m6_cases.py --exit-check` lists the three deviations with Q-OASIS-2.
- [ ] 15.7 Record deviations only with an approved decision: Q-LEFDEF-1 (DEF-read width), 73 and 83. Q-LEFDEF-114 is compat and needs none. Verify that `m6_cases.py --exit-check` lists every deferral with its decision or target milestone.

## 16. PERF: benches (Open Question 4, decided (a))

- [ ] 16.1 Add `bench/lefdef/` (`lef read` of `sky130_fd_sc_hd.lef`, `def read` of a placed-and-routed sky130 block, `lef writeall` of the library) and `bench/oasis/` (`oasis read`/`oasis write` of the `gds-bench` workloads), measuring callgrind instructions for Hefesto, and for Magic on the LEF/DEF workloads. Verify that a dispatch run reports the counts.
- [ ] 16.2 Add the `lefdef-bench` and `oasis-bench` jobs to `perf.yml`, with the path filters `src/core/{include/hefesto,src}/{lef,def,oasis}/**` and `bench/{lefdef,oasis}/**`. Record the baselines and gate each workload at its own baseline + 2 %. The Magic (LEF/DEF) and GDS-path (OASIS) comparisons are reported for information only. Verify that `gds-bench` and `drc-profile` still pass.
  - Round 1 note (PDK): no LEF set is wired into `perf.yml` yet. `lefdef-bench` restores `sky130-lef` by the same keys as `ci.yml` (`scripts/fetch_pdk.py --cache-info` prints `sky130_lef_key` and `sky130_lef_path`).

## 17. INTEGRATION

- [ ] 17.1 Apply the shared-file open items of workstreams 1–16: CMake, the `EditorState` and `MaskSession` members, the shared gds-read helpers and `stamp(s)` (design D17), CLI options, presets, the `golden-capture.yml` and `perf.yml` jobs, and the layout-undo spec line for Q-LEFDEF-22. Verify that every macOS preset configures, builds and tests with `-DHEFESTO_WARNINGS_AS_ERRORS=ON`.
  - Round 2 (`m6-round2`): merged LEFTECH, LEFVIA, LEFR, OASFMT, EXTSUB and GOLDEN; regenerated the `lef.md` code table; accepted LEFTECH's hook location (`tech/commands.cpp`, D2/D17 rows amended); added the magic-compat OFF refill after finer-grid reads in `cell/io_session.cpp` and `mask/input/scale.cpp` (regression test in `tests/unit/lef/tech/lifecycle_test.cpp`); renamed OASFMT's reference file to `reference_419.oas` as D20 names it (GOLDEN's `CommittedReferenceTest` and `oasis_validate.py` now find it); added the `make_reference.py` unit tests to `ci.yml`; recorded the round-2 contract changes in design.md; applied LEFR's lef-read and OASFMT's oasis-format spec text items. Owner replacements of the `[m6-scaffold]` tests by OASFMT, EXTSUB and LEFR in `tests/unit/core/` are accepted.
  - Round 2 CI fixes: GCC `-Werror` in `oasis/codec/` (`__int128` through the `__extension__` typedef, info-byte bits without `uint8_t |=` promotion, a useless `uLong` cast) and `lef/via/generate.cpp` (the ENCLOSURE `optional<pair>` GCC 14 flags as maybe-uninitialized); UBSan in `extflat`: a `substrate` record shorter than 7 tokens read its layer past the token list and lost the `Too few area/perim values` warning (fixed in both modes, regression test in `tests/unit/extflat/read_test.cpp`, ext-flattening-read "Node and substrate records" updated).
- [ ] 17.2 Add the fuzz targets `fuzz_lef_reader`, `fuzz_def_reader`, `fuzz_lef_section`, `fuzz_oasis_reader` (plain and gzip, with the content hash and in-memory OASIS and GDS writes) and `fuzz_ext_reader` (EXTSUB), with seeds and resource caps (design D21). Wire them into the fuzz job. Verify that the seed replays pass on every leg and the 60 s job runs each target without a crash.
- [ ] 17.3 Review the new readers for malformed input (design D23, M6-S1..S9) and write the findings in `docs/contributing/security.md`. Draft each cap that changes behaviour as a proposed decision for the user, and never approve it here. Verify that each finding is fixed with a regression test or recorded as an accepted limit.
  - Round-2 inputs: OASFMT proposes caps on decoded point-list and explicit-repetition sizes (both bounded only by the input today, up to ~16x memory amplification of a 1 GiB inflated CBLOCK; `DecoderOptions::inflate_limit` defaults to 1 GiB); EXTSUB's `extflat::ReadLimits` (file 1 GiB, logical line 16 MiB, 1 Mi cells, depth 1000, 4 Mi instances, 16 Mi flat names, 16 Mi merge elements, lines over 128 tokens ignored, use cycles fail) and `use /abs/path` opening `/abs/path.ext` as PaOpen does (M6-S9). Both need proposed decisions.
- [ ] 17.4 Add `--require-area LEFDEF --require-area OASIS` to the determinism job once the expected outputs exist. Verify that the job is identical on every leg.
- [ ] 17.5 Verify that no `CORE-NOTIMPL-006` use or stub remains, retire the code in the table ("Retired in M6, never reused"), and verify that the diagnostics tests pass.
- [ ] 17.6 Confirm the 419-byte reference file with third-party readers (design D20). Dispatch `oasis-validate` at the head being declared done, and check that:
  - KLayout reads the file with no warning;
  - `strmcmp` against the KLayout-made equivalent reports no difference;
  - gdstk reads it and accepts its signature.

  Record the run ID and tool versions here and in OASIS-013's notes. Verify the recorded run.
- [ ] 17.7 Verify the M6 exit criteria end to end:
  - unit, property and fuzz tests are green in CI on every leg;
  - every authored LEFDEF and OASIS golden case (plus MASK-OS-016, the `LEFDEF-RT-*` cases and the DEF-write net cases on Magic-written `.ext` inputs) passes or carries an approved deviation (OASIS-012/030/061: Q-OASIS-2, missing commercial input); `python3 tools/golden/authoring/m6_cases.py --exit-check` exits 0 (no entry still deferred to M6);
  - `oasis-validate` at the same head reports no KLayout `strmxor` difference and no strict-mode warning for the OASIS-051/057/060/061/062 outputs;
  - the 419-byte file is confirmed (task 17.6);
  - the determinism job covers LEFDEF and OASIS;
  - `check_requirement_coverage.py --milestone M6` covers the 146 exact and behavioural requirements, and every native requirement of the OASIS capabilities has a tagged test;
  - `check_milestone_readiness.py --milestone M7` reports the next milestone's state, and the EXTSUB requirements covered in M6 are listed in section 9's notes for M7;
  - `openspec validate --all --strict` passes;
  - `perf.yml` is green (`gds-bench`, `drc-profile`, `golden-slow`, `gds-roundtrip`, and `lefdef-bench` and `oasis-bench` within baseline + 2 %);
  - every result above is from a run at the head being declared done.
- [ ] 17.8 Update:
  - README.md;
  - CHANGELOG.md (the `lef`, `def` and `oasis` commands, the shared read settings, GDS transcoding of OASIS cells);
  - docs/roadmap.md (M6 status);
  - docs/building.md (LEF data, the reference tools);
  - docs/contributing/ (the `lef`, `def`, `oasis` and EXTSUB areas, codes, fuzz targets, golden modules, `oasis-validate`, benches);
  - the diagnostic code documents.

  Verify that the documented commands run as written.
