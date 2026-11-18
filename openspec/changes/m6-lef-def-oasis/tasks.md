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
- [ ] 1.2 (PDK) Verify the release data:
  - list `libs.ref/<lib>/{lef,techlef}` of every library in the pinned ciel releases (sky130A and gf180mcuD at `5740c4a8…`, ihp-sg13g2 at `5e6d592e…`);
  - install the tech LEF and the standard-cell LEF into a scratch root, recording download size, kept size, peak disk and file names;
  - record whether any library ships an OASIS file and which tool wrote it (from the START/property records). Open Question 3 was decided (b), so a commercial-written file found here is a new input for OASIS-012/030/061 (task 15.6), not a precondition.

  Record the facts in design D2 (PDK row) and D19 ("Commercial OASIS files"). Verify that the documented commands reproduce them.
- [ ] 1.3 (GOLDEN) Pin KLayout and gdstk in the capture image (design D18):
  - decide the install source from facts: a Debian package at the image's APT snapshot, otherwise KLayout's upstream `.deb` for Debian 13 with a SHA-256; and a gdstk wheel pinned by version and hash;
  - add `[tools.klayout]` and `[tools.gdstk]` to `tools/golden/pins.toml` and the ARGs to `tools/golden/container/Containerfile`;
  - extend `tools/golden/tests/test_pins.py`;
  - rebase onto the M8 `gawk` change if it landed first.

  Record the versions, the source and the image size increase in design D18. Verify a CI image build whose log prints `klayout -v` and the gdstk version, and that `test_pins.py` passes.
- [ ] 1.4 (PDK) Add `sky130-lef`, `gf180mcu-lef` and `ihp-lef` to `scripts/fetch_pdk.py` (Open Question 6 (a)): pins, `-only` subsets, tree digests, cache keys and `--print-digest`. Add their paths to `tests/unit/common/reference_data.hpp` (nullopt when missing, so tests skip), with tests in `scripts/tests/test_fetch_pdk.py`. Restore and fetch them only in the jobs that use them. Verify that a fresh install matches the digests, a second run is a verified no-op, and the `ci.yml` legs fetch only the sets their golden cases replay.
- [ ] 1.5 (PDK) Update `docs/building.md` "Reference data for tests" with the LEF sets, and "Golden capture" with the reference tools. Verify that the documented commands run as written.
- [ ] 1.6 (GOLDEN) Add the `.ext` probe (Open Question 1, decided (a)). A capture mode `mode=ext-inputs` of `golden-capture.yml` runs Magic's `extract all` on the layout of each DEF-write net case (LEFDEF-317..328, 330..334, and the PDK DEF cases). `tools/golden/authoring/lefdef_cases.py --sync-ext` copies the `.ext` files into each case's `input/` and into `tests/data/extflat/<case>/`, with the probe script committed. Verify that a dispatch run produces the `.ext` files, that they are committed with their script, and that every file ends with Magic's last record and has no `error` line.
- [ ] 1.7 (INTEGRATION, with GOLDEN) Add the dispatch mode `oasis-validate` to `golden-capture.yml` (design D18). It builds Hefesto, writes the OASIS-05x outputs, the strict-mode files and the 419-byte file, and runs KLayout (`strm2gds`, `strmxor`, `strmcmp`, the reader with `expect_strict_mode`) and gdstk (`oas_validate`, read). The verdicts are a job summary and an artifact. Verify with a dispatch run on the scaffold that the tools run and report, with Hefesto's OASIS steps skipped until they exist.

## 2. Scaffold

- [x] 2.1 Create the change `m6-lef-def-oasis` (README, proposal, design, tasks, `skip_specs: true`). Verify that `openspec validate m6-lef-def-oasis --strict` and `openspec validate --all --strict` pass.
- [x] 2.2 Add the areas `lef` (`lex`, `tech`, `via`, `read`, `write`, `command`), `def` (`read`, `write`) and `oasis` (`codec`, `read`, `write`, `command`), plus `extflat` and `extract/tech` (Open Questions 1 and 2, decided (a); `extract/tech` may already exist from M10's subset). Each subdirectory gets its `sources.cmake` and `codes.inc`, plus the INTEGRATION-owned assemblers. Add the test directories and `docs/diagnostics/codes/{lef,def,oasis}.md`, one section per owning workstream (design D1). Verify that the build passes and the test "each area's code table matches its document and prefix" covers the new areas.
  Done 2026-11-18 (branch `m6-r1-areas`): `lef`, `def`, `oasis` and `extract` have INTEGRATION-owned `sources.cmake`/`codes.cpp` assemblers and one `sources.cmake` and `codes.inc` per subdirectory; `extflat` is one EXTSUB directory (`sources.cmake`, `codes.inc`) whose `codes.cpp` is INTEGRATION's. The code areas are `LEF`, `DEF`, `OASIS`, `EXTFLAT` and `EXTRACT`, documented in `docs/diagnostics/codes/{lef,def,oasis,extflat,extract}.md` with one section per owning workstream. `tests/unit/{lef,def,oasis,extract}/<sub>/` and `tests/unit/extflat/` exist and build an executable once they hold a test. The macos-debug build passes with `-Werror`, and "each area's code table matches its document and prefix" covers the five areas.
- [x] 2.3 Write the documented public headers of design "Area contracts" with stubs, and add `CORE-NOTIMPL-006`. Register `lef` and `def` through one dispatcher with Magic's option lookup (design D3, D14), and `oasis` with its full option table. Unimplemented options fail with `CORE-NOTIMPL-006`. Add the `def` and `oasis` help lines to `ui/magic_help.cpp`. Verify that the headers compile standalone with `-Werror` in `macos-debug`, and that a `[m6-scaffold]` test lists every `lef`, `def` and `oasis` option and Magic's invalid-option message.
  Done 2026-11-18 (branch `m6-r1-areas`): the "Area contracts" headers outside task 2.5's D4 interfaces, with stubs (`lef/read/find_file`, `lef/via/{cut,regions}`, `lef/write/{macro,number}`, `oasis/codec/{validate,cblock}`, `oasis/read/{stamps,find_file}`, `oasis/write/{emit,transcode,repetitions}`, `oasis/command/{command,settings,format}`, private `gds/write/vendor_oasis.hpp`); they forward-declare the 2.5 types `lef::LayerRecord` (struct), `lef::LayerTable` and `oasis::Encoder` (classes). `lef`/`def` share one dispatcher with Magic's two tables, `def writeall`'s refusal and the "only for LEF" answers; `oasis` has 34 options, and an unknown or ambiguous one is a `CMD-USAGE-001` error. `magic_help.cpp` already had the `lef` and `def` lines and gets no `oasis` line: it holds Magic's help strings, printed with magic-compat ON (UI-G-005); `oasis` is listed from its own help text with it OFF. The new headers compile standalone with `-Werror` (Clang, `-O0`/`-O3`); `tests/unit/core/m6_scaffold_test.cpp` covers the options and the invalid-option message.
- [ ] 2.4 Split `gds::GdsSettings` (design D15):
  - `stream::StreamReadSettings` gets the 11 shared read fields;
  - `MaskSession` gains `stream_read` and `oasis`;
  - every `gds` option handler and every reader of a moved field is updated.

  The split is behaviour-preserving. Verify that every `tests/unit/gds/` test, the `[m5-scaffold]` test and every MASK golden case that runs locally pass unchanged, and that `gds readonly`, `gds flatten` and `gds ordering` queries return the same values.
- [ ] 2.5 Add the shared interfaces of design D4 as stubs:
  - `lef::LefSession` in `EditorState`;
  - `Lexer`, `MessageCounter`, `LayerTable`, `ViaGenerator`, `paint_polygon`;
  - `oasis::Decoder`, `Encoder`, `ModalState`, `Repetition`, `Element`;
  - `extflat::read_flat` and `extract::StyleView` (Open Questions 1 and 2 (a));
  - the read and write entry points.

  Verify that the CLI, shell and core tests pass unchanged.
- [ ] 2.6 Record the as-landed file and owner table and every difference from "Area contracts" in design.md ("As-landed scaffold contracts"), and seed `docs/design/m6-ui-conformance.md`. Verify that `openspec validate --all --strict` passes.

## 3. LEFTECH: lef-tech-section

- [ ] 3.1 Port lefTech.c into `LayerTable::from_section`:
  - the keyword grammar, magic type resolution and class mapping;
  - aliases and canonical names;
  - the tech-load messages ("Unknown LEF section keyword: %s.  Line ignored." and the others).

  Use `util::MagicHashTable` with the signed-byte hash (Q-LEFDEF-82, 92, 116). Verify the `[cap:lef-tech-section]` tests for "Lef section line grammar", "Magic type resolution in lef lines", "Keyword to LEF class mapping" and "LEF name aliasing and canonical names", and a hash-order test against the M1 hash model.
- [ ] 3.2 Implement the defaults and their fill with explicit unset flags: route width and spacing from `drc::default_width`/`default_spacing`, contact areas, and the fill after grid scaling (Q-LEFDEF-1, 2 per mode; Q-LEFDEF-5 fix). Verify the `[req:Default routing parameters]`, `[req:Default contact area]` and `[req:Filling defaults after grid scaling]` tests, with one test per mode for 1:1, 3:2, 2:1 and shrinking grids.
- [ ] 3.3 Implement `scale(n, d)` (truncating route values, flooring contact areas, no refill) and the table lifecycle: rebuild at tech load, kept across reads, and the hooks in `tech/loader.cpp` and `tech/scale_layout.cpp`. Verify the `[req:Scaling the LEF table]`, `[req:LEF table lifecycle]` and `[req:Use of entry classes by readers and writers]` tests, and that `scalegrid 1 2` gives the LEFDEF-004 values.
- [ ] 3.4 Load the sky130A, gf180mcuD and ihp-sg13g2 `lef` sections at the M4 pins without new diagnostics. Verify a unit test per PDK that compares the record count and the canonical names with the PDK tech file.
- [ ] 3.5 Keep every function's cognitive complexity at most 15 (flag the irreducible ones). Verify with the cognitive-complexity tooling on `lef/tech/`, and that `check_requirement_coverage.py --capability lef-tech-section` reports 10 of 10.

## 4. LEFVIA: via geometry, shared by LEF and DEF

- [ ] 4.1 Implement the LEF VIA and VIARULE storage and redefinition: shared entries, aliases and class VIA (Q-LEFDEF-8 fix). Verify the `[req:LEF VIA and VIARULE definitions]` and `[req:Via redefinition]` tests (lef-read and def-read).
- [ ] 4.2 Implement cut growing to `mask::OutputStyle::contact_size` (the output style, Q-LEFDEF-17) with Magic's warnings. Verify the `[req:Cut growing]` tests and LEFDEF-011's warning text in a unit test.
- [ ] 4.3 Implement `ViaGenerator::generated`: CUTSIZE, LAYERS (with the layer-order warning), CUTSPACING, ENCLOSURE and ROWCOL. Omitted parameters take the VIARULE values or 0 (Q-LEFDEF-61 fix). ORIGIN, OFFSET and PATTERN apply with magic-compat OFF only (Q-LEFDEF-62), as do via-rule vias (Q-LEFDEF-7 OFF). Verify the `[req:Generated via geometry]` and `[req:Generated DEF vias]` tests, one per mode for Q-LEFDEF-7 and 62.
- [ ] 4.4 Implement `paint_polygon` over `db::decompose_polygon`, with port labels from the single decomposition (Q-LEFDEF-23). Verify that LEF POLYGON, DEF pin POLYGON (Q-LEFDEF-66 OFF) and DEF route POLYGON (Q-LEFDEF-70 OFF) give identical tiles for the same vertices.
- [ ] 4.5 Implement the DEF-write side:
  - contact-region identification, with Q-LEFDEF-111 per mode and Q-LEFDEF-106 fixed;
  - generated-via naming, with the `_hx`/`_vx` suffixes and Q-LEFDEF-105 per mode;
  - the cut array, with Q-LEFDEF-107 per mode;
  - a residue with no LEF mapping is skipped with a warning (Q-LEFDEF-115).

  Verify the `[req:Contact region identification]` and `[req:Generated via definitions]` tests.
- [ ] 4.6 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `lef/via/`.

## 5. LEFR: lef-read, the tokenizer and the lef read command

- [ ] 5.1 Port LefNextToken, LefEstimate, LefError and LefParseEndStatement into `Lexer` and `MessageCounter`: 2048-character lines, comments, quoted strings, `;` glued to numbers, the 100-message cap and the totals. Verify the `[req:LEF/DEF tokenizer]`, `[req:END statement matching]` and `[req:LEF/DEF message reporting]` tests, and LEFDEF-005/006's inputs in unit tests.
- [ ] 5.2 Implement unit conversion (half away from zero in binary32), layer name resolution (Q-LEFDEF-3 per mode; Q-LEFDEF-4 fix) and the top-level structure. Verify the `[req:LEF unit conversion]`, `[req:LEF layer name resolution]` and `[req:Supported LEF file structure]` tests, including the LEFDEF-007 values at .0025 boundaries.
- [ ] 5.3 Implement the LAYER section (Q-LEFDEF-6 per mode) and NONDEFAULTRULE (Q-LEFDEF-10 per mode; Q-LEFDEF-9 fix). Verify the `[req:LEF LAYER section]` and `[req:LEF NONDEFAULTRULE section]` tests.
- [ ] 5.4 Implement MACRO and SITE cell creation:
  - the coordinate offset (Q-LEFDEF-12 per mode);
  - the statements and cell properties (Q-LEFDEF-19 fix);
  - completion (FIXED_BBOX, LEFview);
  - PIN attributes (Q-LEFDEF-13, 14 per mode);
  - PORT and OBS geometry (Q-LEFDEF-15, 20 per mode; Q-LEFDEF-18 fix);
  - port labels in creation mode.

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

- [ ] 9.1 Port the `.ext` reader subset: EFReadFile over the `extract-file-format` grammar (read side), and the node, connection, merge and kill records of EFbuild.c (no device parameters). Verify `[cap:ext-flattening-read]` tests over the task 1.6 files and malformed inputs. List the requirements covered in this task's note, since they stay M7's in the coverage report.
- [ ] 9.2 Port EFFlatBuild with `EF_FLATNODES | EF_NOFLATSUBCKT` (unscaled, resistance unused), EFVisitNodes order, and the name preference of `ext-flattening-names`. Verify `[cap:ext-flattening-hierarchy]` and `[cap:ext-flattening-names]` tests, and that the visit order matches Magic's on the task 1.6 cases (a probe line in each case).
- [ ] 9.3 Write the style selection, `device` (class and types), `antenna` tie-down, `substrate` and active-type parts of the `extract-tech-section` reader in `extract/tech/`, sharing its files with M10's subset if that landed first (design D10), and `extract::StyleView` over them. Every other keyword is skipped silently. Verify `[cap:extract-tech-section]` tests on the three PDK tech files with no new diagnostics, and the no-style case (Q-LEFDEF-40: empty sets).
- [ ] 9.4 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `extflat/` and `extract/tech/`.

## 10. OASFMT: oasis-format

- [ ] 10.1 Implement:
  - the primitives: unsigned and signed integers (Q-OASIS-1; the 65-bit overflow), reals of types 0–7, strings with their character sets, deltas, point lists, repetitions (lazy), intervals;
  - the record identifiers and the file structure.

  Verify the `[cap:oasis-format]` tests for "File structure and magic bytes", "Record identifiers", "Unsigned and signed integers", "Real numbers", "Strings and character sets", "Delta encodings", "Point lists", "Repetitions" and "Intervals", with OASIS-001..004's byte files as unit inputs.
- [ ] 10.2 Implement START and table offsets (Q-OASIS-8), END and padding, validation (Q-OASIS-2: gdstk coverage and byte order), name records and refnums, CELL, and modal variables (Q-OASIS-5). Verify their tests with OASIS-005..008's inputs.
- [ ] 10.3 Implement the element records: PLACEMENT, TEXT, RECTANGLE (Q-OASIS-3), POLYGON, PATH (Q-OASIS-11), TRAPEZOID, CTRAPEZOID (Q-OASIS-4, all 26 types), CIRCLE. Then PROPERTY (Q-OASIS-6), standard properties, extension records, PAD, CBLOCK (Q-OASIS-9, inflate cap) and strict mode (Q-OASIS-7). Verify each `[req:…]` test with OASIS-009..015's inputs.
- [ ] 10.4 Implement the `Encoder`: minimal forms, modal omission, strict and non-strict tables, CBLOCK at a level, and END with validation. Verify that decode(encode(x)) = x over seeded random records, and that encode(decode(f)) = f for every hand-assembled file written in minimal form.
- [ ] 10.5 Write `tests/data/oasis/codec/make_reference.py` (with tests) to assemble the 419-byte file from the "Complete annotated example" listing, and commit the file and its SHA-256. Verify the `[req:Complete annotated example]` test: 419 bytes, the decoded cell as listed, and identical re-encoding.
- [ ] 10.6 Keep every function's cognitive complexity at most 15, and verify it with the tooling on `oasis/codec/`. Verify that `check_requirement_coverage.py --capability oasis-format --classes native` reports 30 of 30 (all native).

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
- [ ] 15.3 Add the comparators `oasis-records`, `stream-geometry`, `lef-text` and `def-text` (design D19), with tests in `tools/golden/tests/`. Verify that `stream-geometry` finds a one-unit shift on one layer and accepts a reordered, re-fractured equivalent.
- [ ] 15.4 Author MASK-OS-016 with `lef write` of a contact cell. Drop `def, lef` from UI-G-005's deviation. Add a startup case that loads a `.lef` from the command line. Verify the replay.
- [ ] 15.5 (Open Question 6, decided (a)) Add one `LEFDEF-RT-<lib>` case per standard-cell library (tech LEF and cell LEF read, then `lef writeall`), and LEFDEF-240 on a small OpenROAD-style sky130 DEF. Verify their capture, and that `ci.yml`'s golden legs replay them with `--require-inputs`.
- [ ] 15.6 (Open Question 3, decided (b)) Author OASIS-012, 030 and 061 with their gdstk- and KLayout-written inputs and readers only. Record each case's approved deviation (Q-OASIS-2, amended 2026-11-18: the commercial writer's or reader's input is missing) in its case notes and its `tests/golden/OASIS/CASES.md` row, not in `deviations.toml`, since the cases pass (design D19). Verify their capture, and that `m6_cases.py --exit-check` lists the three deviations with Q-OASIS-2.
- [ ] 15.7 Record deviations only with an approved decision: Q-LEFDEF-1 (DEF-read width), 73 and 83. Q-LEFDEF-114 is compat and needs none. Verify that `m6_cases.py --exit-check` lists every deferral with its decision or target milestone.

## 16. PERF: benches (Open Question 4, decided (a))

- [ ] 16.1 Add `bench/lefdef/` (`lef read` of `sky130_fd_sc_hd.lef`, `def read` of a placed-and-routed sky130 block, `lef writeall` of the library) and `bench/oasis/` (`oasis read`/`oasis write` of the `gds-bench` workloads), measuring callgrind instructions for Hefesto, and for Magic on the LEF/DEF workloads. Verify that a dispatch run reports the counts.
- [ ] 16.2 Add the `lefdef-bench` and `oasis-bench` jobs to `perf.yml`, with the path filters `src/core/{include/hefesto,src}/{lef,def,oasis}/**` and `bench/{lefdef,oasis}/**`. Record the baselines and gate each workload at its own baseline + 2 %. The Magic (LEF/DEF) and GDS-path (OASIS) comparisons are reported for information only. Verify that `gds-bench` and `drc-profile` still pass.

## 17. INTEGRATION

- [ ] 17.1 Apply the shared-file open items of workstreams 1–16: CMake, the `EditorState` and `MaskSession` members, the shared gds-read helpers and `stamp(s)` (design D17), CLI options, presets, the `golden-capture.yml` and `perf.yml` jobs, and the layout-undo spec line for Q-LEFDEF-22. Verify that every macOS preset configures, builds and tests with `-DHEFESTO_WARNINGS_AS_ERRORS=ON`.
- [ ] 17.2 Add the fuzz targets `fuzz_lef_reader`, `fuzz_def_reader`, `fuzz_lef_section`, `fuzz_oasis_reader` (plain and gzip, with the content hash and in-memory OASIS and GDS writes) and `fuzz_ext_reader` (EXTSUB), with seeds and resource caps (design D21). Wire them into the fuzz job. Verify that the seed replays pass on every leg and the 60 s job runs each target without a crash.
- [ ] 17.3 Review the new readers for malformed input (design D23, M6-S1..S9) and write the findings in `docs/contributing/security.md`. Draft each cap that changes behaviour as a proposed decision for the user, and never approve it here. Verify that each finding is fixed with a regression test or recorded as an accepted limit.
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
