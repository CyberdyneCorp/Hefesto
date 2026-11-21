# Tasks

How this plan runs:
- **Section 1 comes first.** The user decides design Open Questions 1–9 and Q-EXTRACT-27. The tasks below follow the recommended answers. A different answer changes the design sections named in "Decisions this design depends on" and the tasks that cite them.
- The rounds are those of design D1: R0 (section 1), R1 (sections 2 and 3, and GOLDEN's NETLIST capture), R2 to R4 (the workstreams, in D3's dependency order), R5 (GOLDEN's PDK cases, PERF) and R6 (INTEGRATION's exit tasks).
- Each task ends with how it is verified.
- Workstreams edit only what design D3 assigns them. Shared-file needs are reported as open items.
- Golden case inputs are authored by GOLDEN from the owning workstream's requirements (design D21). `expected/` is captured in CI.
- Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`. Requirements with a magic-compat dependence get one test per mode.
- **Coverage targets.** The last task of each capability workstream runs `scripts/check_requirement_coverage.py --capability <cap>` and verifies that 100 % of its exact and behavioural requirements are tagged, or listed as deferred with a reason. The milestone target is 204 of 204 (28 today, all from M6's EXTSUB). Each workstream also keeps every function at cognitive complexity 15 or below, measured with the cognitive-complexity tooling (clang-tidy) on its directories, and flags irreducible functions with a reason.
- Each task names the requirements it covers. Together they cover all 207 requirements of the 21 capabilities (design D3 totals).
- Every approved decision cited below is in `docs/decisions/m7-decision-table.md`.

## 1. Decisions (R0)

- [ ] 1.1 (INTEGRATION) Record the user's answers to design Open Questions 1–9:
  - in the design's "Open Questions";
  - in the decision table's "Planning questions raised by the M7 plan".

  Record Q-EXTRACT-27 as `Status:`/`Resolution:` lines in `docs/decisions/open-questions/EXTRACT.md`. If Open Question 5 is answered (a), edit `hefesto-performance` "Batch throughput relative to Magic" to the text of design D23. If Q-EXTRACT-27 is answered fix, edit `extract-commands` "Incremental extraction" and "Output path and file location" (each file written completely or not at all, with a scenario). Adjust D1–D25 and these tasks for any answer other than the recommended one. Verify that `openspec validate --all --strict` and `python3 scripts/check_milestone_readiness.py --milestone M7` pass, and that `python3 scripts/gen_specs_index.py` leaves no diff after the commit.

## 2. Reference data and capture (R1)

- [ ] 2.1 (PDK) Confirm the standard-cell inputs of Open Question 4: the `.mag` counts of `sky130_fd_sc_hd` (446) and `gf180mcu_fd_sc_mcu7t5v0` (229) in the pinned sets, and that `sg13g2_stdcell` GDS is in the `ihp-gds` set. Add `pinned_stdcell_mag(lib)` and `pinned_stdcell_gds(lib)` to `tests/unit/common/reference_data.hpp` (nullopt when the set is missing). Record the default extract style of each PDK and its tech format. Verify that a fresh fetch reproduces the digests and that the helpers return the paths on a leg that has the sets.
- [ ] 2.2 (GOLDEN) Build Magic's standalone `extcheck` in the capture image: `make -C extcheck` after the main build, at the same pin, installed next to `magic`. Extend `tools/golden/tests/test_pins.py` if the build adds an ARG. If `extcheck` no longer builds at the pin, record why and hand-derive NL-CHK-01 (design R7). Verify a CI image build whose log runs `extcheck` with no arguments and shows exit status 1 and the usage text.
- [ ] 2.3 (GOLDEN) Extend `golden-capture.yml` and `tools/golden/capture.py` to collect the M7 outputs by name pattern (`*.ext`, `*.spice`, `*.sim`, `*.al`, `*.nodes`, `*.res.ext`, `*.res.lump`, `*.fh`, and stdout), each case in a fresh session (Q-NETLIST-16, Q-NETLIST-46). Verify with a dispatch run on a smoke case (M6's `tests/data/extflat/smoke/` layout through `extract all` and `ext2spice`) that every pattern is collected.
- [ ] 2.4 (GOLDEN) Add the comparators of design D21: `compare/spice_cap.py`, `sim_cap.py` and `res_ext.py`, each with its canonicalization documented in the module docstring and unit tests in `tools/golden/tests/`. The tests cover a swapped coupling pair, a renumbered coupling device, a reordered resistor record within one net, and a negative control in which a changed value fails. Add masks for the ExtTimes timing lines to `magic-console`. Verify that the tests pass in CI.
- [ ] 2.5 (GOLDEN) Author and capture the 44 NETLIST rows whose inputs are hand-written `.ext` files (NL-FLAT-01..18, NL-SPICE-01..14, NL-HIER-01..06, NL-SIM-01..05, NL-CHK-01), with `tools/golden/authoring/netlist_cases.py` and `m7_cases.py`. Verify that `m7_cases.py --not-authored` no longer lists them and that their `expected/` files are committed from a capture run.

## 3. Scaffold (R1)

- [ ] 3.1 Create the change `m7-extraction` (README, proposal, design, tasks, `skip_specs: true`). Verify that `openspec validate m7-extraction --strict` and `openspec validate --all --strict` pass.
- [ ] 3.2 Add the subdirectories of design D2 to `extract/` (`node`, `file`, `cap`, `device`, `hier`, `command`, `length`), the new areas `netlist` (`spice`, `sim`, `extcheck`), `extresist` (`network`, `command`, `output`) and `antenna`, and `src/cli/extcheck/` (Open Question 6 (a)). Each gets its `sources.cmake` and `codes.inc`, plus the INTEGRATION assemblers, the test directories and `docs/diagnostics/codes/{netlist,extresist,antenna}.md`, with one section per owning workstream. Verify that the build passes and that the test "each area's code table matches its document and prefix" covers the new areas.
- [ ] 3.3 Write the documented headers of design "Area contracts" with stubs, including `ExtStyle` and `testing::StyleBuilder`, and add the next free `CORE-NOTIMPL-NNN` (design D4). Register `extract`/`ext`, `ext2spice`/`exttospice`, `ext2sim`/`exttosim`, `extresist` and `antennacheck`, each with its full option table and Magic's option matching. Unimplemented options fail with the new code. Verify that the headers compile standalone with `-Werror`, and that a `[m7-scaffold]` test lists every option of the five commands and Magic's invalid-option messages.
- [ ] 3.4 (INTEGRATION, MODEL review) Add the tile client slot and the restricted stitch header (design D6, Open Question 1 (a)): `client`, `set_client`, `reset_clients`, `kClientDefault`, `db/plane_stitch.hpp`, and the `check_core_includes.py` rule that only `extract/` and `extresist/` include it. Verify:
  - unit tests show the slot is ignored by `clone`, `operator==` and undo;
  - `drc-profile` and `gds-bench` stay within their gates;
  - the iPad memory check of `hefesto-performance` "Memory bound on iPad" passes with the extra field.
- [ ] 3.5 Add `EditorState::extract`, `netlist` and `extresist` with the members of design D5, and the entry points `extract::run`, `ensure_extracted`, `netlist::spice::run`, `render`, `netlist::sim::run`, `netlist::extcheck::run`, `extresist::run` and `antenna::check` as stubs. Verify that the CLI, shell and core tests pass unchanged.
- [ ] 3.6 Record the as-landed file and owner table and every difference from "Area contracts" in design.md ("As-landed scaffold contracts"), and seed `docs/design/m7-ui-conformance.md`. Verify that `openspec validate --all --strict` passes.

## 4. EXTTECH: extract-tech-section (R2; Open Question 2)

Builds on M6's `style_reader.cpp` and M10's TECH subset. Handlers go in new files first (design D7) and are wired into the shared dispatch table after M10's TECH merges.

- [ ] 4.1 Turn the keyword table into the dispatch table of design D7, and build the style table with lazy per-style parsing (ExtLoadStyle). Covers "Extract section line grammar and keyword matching", "Extraction styles and variants", "Style defaults" (completing M6's partial tests). Verify `[cap:extract-tech-section]` tests for the three requirements, including variant expansion and an ambiguous keyword prefix.
- [ ] 4.2 Parse the scale, step and connectivity keywords. Covers "Units, lambda, cscale and rscale" (the cscale and rscale parts beyond M10's), "Step size and side halo parsing" (Q-EXTRACT-19 per mode), "Plane ordering" (verify M10's implementation against the spec's scenarios; add only what is missing), "Connectivity overrides". Verify the tests, with one per mode for Q-EXTRACT-19.
- [ ] 4.3 Parse resistance classes. Covers "Resistance classes and contact resistance", "Contact residues and coupling table completion". Verify the tests on a contact with residues on two planes.
- [ ] 4.4 Parse the capacitance rules into `CapRules`. Covers "Area and perimeter capacitance rules", "Overlap capacitance rules", "Sidewall coupling rules and halving", "Sideoverlap rules" (Q-EXTRACT-1 per mode), "Default capacitance shorthands", "Fringe model default chosen from tech format" (format 34 threshold; a style change leaves it alone). Verify the tests, with one per mode for Q-EXTRACT-1.
- [ ] 4.5 Complete the device, substrate and antenna keywords. Covers "Device declaration parsing" (every class, parameters, ranges, identifier layers; Q-EXTRACT-2 replicate, Q-EXTRACT-21 per mode), "Substrate declaration", "Non-electrical types", "Antenna keywords parse-level reference" (`antenna`, `model`, `tiedown`; Q-MISC-3 in both modes). Verify the tests, including sky130A's photodiode line (Q-EXTRACT-2).
- [ ] 4.6 Implement micron conversion and grid rescale for every rule. Covers "Layer height and thickness defaults" (check M10's implementation against the spec's scenarios), "Micron conversion" (Q-EXTRACT-3 per mode), "Scaling to internal units" (Q-EXTRACT-25: overrides scaled, lost on reload). Hook the style into tech load and `scalegrid` through the hook M10's TECH added (design D20). Verify the tests, and that `scalegrid 1 2` gives EX-059's values.
- [ ] 4.7 Re-source `StyleView` and M10's `ExtractStyleSubset` from `ExtStyle` without changing their interfaces. Verify that M6's `style_view_test.cpp`, the LEFDEF cases and M10's layer-stack tests pass unchanged, and that sky130A, gf180mcuD and ihp-sg13g2 load with no new diagnostics (one load test per PDK, comparing the messages with the TECH golden cases).
- [ ] 4.8 Run `check_requirement_coverage.py --capability extract-tech-section` (22 of 22) and the complexity check on `extract/tech/`.

## 5. EXTNODE: extract-nodes-and-labels, extract-file-format (R2)

- [ ] 5.1 Port ExtFindRegions and the region callbacks over the plane client slot (design D6, D8). Covers "Node connectivity" (cross-plane halo, split tiles). Verify the tests, including EX-025's well–psd connection, and that every slot is back at `kClientDefault` after a pass (debug check).
- [ ] 5.2 Implement the node reference point and labels. Covers "Node reference point and generated name", "Label attachment", "Label types by trailing character". Verify the tests, including negative coordinates and the right side of a split tile (EX-019).
- [ ] 5.3 Implement name selection and the per-node records. Covers "Node name selection" (Q-EXTRACT-4 per mode), "Port records", "Node and attribute records", "Alias output". Verify the tests, with one per mode for an attribute label attached before two name labels.
- [ ] 5.4 Port ExtUnique.c. Covers "Duplicate label warning", "Unique naming of duplicate labels" (Q-EXTRACT-23 replicate; Q-EXTRACT-6 fix: temporary labels reverted in every renamed cell, the top cell included). Verify the tests, including the layout labels after `extract do unique` on a top cell.
- [ ] 5.5 Implement `ExtWriter`. Covers "Lexical conventions", "Record grammar", "Header records" (Q-EXTRACT-10), "Use records", "Record order" (Q-EXTRACT-5 tile order, Q-EXTRACT-7 canonical `cap` order and hash-order `merge`), "Reader constraints honored by the writer", "Annotated example" (informative: a round-trip fixture). Verify the tests, and that every emitted file in the unit tests is read back by EXTFLAT's reader with no message.
- [ ] 5.6 Run `check_requirement_coverage.py --capability extract-nodes-and-labels` (10 of 10) and `--capability extract-file-format` (6 of 6 exact or behavioural, plus the informative example) and the complexity check on `extract/{node,file}/`.

## 6. EXTCAP: extract-capacitance, extract-resistance-lumped (R2; Open Question 3)

- [ ] 6.1 Implement the substrate caps. Covers "Capacitance option gating", "Area capacitance to substrate", "Perimeter capacitance to substrate" (diagonal edges). Verify the tests with EX-002's and EX-003's values.
- [ ] 6.2 Implement overlap coupling. Covers "Overlap coupling with shielding". Verify the tests with EX-004's and EX-005's values.
- [ ] 6.3 Implement sidewall coupling. Covers "Sidewall edge enumeration", "Nearest-neighbor shadowing along an edge", "Sidewall coupling value" (Q-EXTRACT-20, float32). Verify the tests with EX-006..008's values, and a pinned-value test of the float32 denominator.
- [ ] 6.4 Implement sideoverlap with the bit-identical `atan` of Open Question 3. Covers "Legacy sideoverlap (fringe directly below the edge)" (Q-EXTRACT-18 per mode), "Fringe-halo sideoverlap" (Q-EXTRACT-17 per mode). Verify:
  - the tests, with one per mode for each decision;
  - pinned values of `fringe_fraction` on every leg;
  - a comparison with glibc's `atan` on the capture image over the arguments the EX and PDK cases produce (design D22).
- [ ] 6.5 Implement corner fringe. Covers "Corner fringe capacitance to substrate", "Corner fringe coupling" (Q-EXTRACT-24 per mode). Verify the tests with EX-013's layouts.
- [ ] 6.6 Implement the coupling output. Covers "Coupling to the substrate node", "Coupling record output" (Q-EXTRACT-7). Verify the tests, including EX-016.
- [ ] 6.7 Implement lumped resistance. Covers "Per-class area and perimeter of a node", "Lumped resistance formula", "Resistance output rounding", "Area and perimeter values in records", "Resistance class header". Apply Q-EXTRACT-22 per mode to the shorted-ports warning. Verify the tests with EX-017's and EX-018's values.
- [ ] 6.8 Run `check_requirement_coverage.py --capability extract-capacitance` (13 of 13) and `--capability extract-resistance-lumped` (5 of 5), and the complexity check on `extract/cap/`.

## 7. EXTDEV: extract-devices, extract-substrate (R2)

- [ ] 7.1 Implement device regions and terminals. Covers "Device regions", "Subcell device priority", "Terminal discovery on the device plane", "Terminals off the device plane", "Terminal area, perimeter and sharing". Verify the tests, including EX-040's shared diffusion and EX-043's subcell priority.
- [ ] 7.2 Implement record and model selection. Covers "Device record selection", "Model selection by parameter ranges" (Q-EXTRACT-21 per mode), "Missing terminals and rejected devices". Verify the tests with EX-037..039.
- [ ] 7.3 Implement MOSFET dimensions. Covers "MOSFET-family length and width", "Boundary lists for dimension computation", "Effective length and width of non-rectangular gates". Verify the tests with EX-028..030, and a pinned-value test of the non-rectangular estimate.
- [ ] 7.4 Implement the passive devices. Covers "Resistor dimensions and value", "Capacitor dimensions and value" (Q-EXTRACT-8 fix), "Diode records" (Q-EXTRACT-13 fix). Verify the tests with EX-031..035.
- [ ] 7.5 Implement the device output. Covers "Device record header and substrate field", "Device parameter values" (Q-EXTRACT-15 fix), "Terminal output and ordering", "Parameters records", "Device cell property" (Q-EXTRACT-16). Verify the tests with EX-026, 027, 036, 041, 042 and 044.
- [ ] 7.6 Implement the substrate. Covers "Backward-compatible mode without a substrate declaration", "Isolated substrate regions", "Global substrate node discovery", "Substrate node naming" (Q-EXTRACT-9), "Substrate record", "Device substrate terminals", "Subcell substrate connections", "Substrate coupling relocation". Verify the tests with EX-045..048, and the `scripting-api` "Script variables read by the engine" extraction scenarios (SCRIPT-API-16's input).
- [ ] 7.7 Run `check_requirement_coverage.py --capability extract-devices` (19 of 19) and `--capability extract-substrate` (8 of 8), and the complexity check on `extract/device/`.

## 8. EXTHIER: extract-hierarchy (R3)

- [ ] 8.1 Implement `extract_cell` and the bottom-up order. Covers "Bottom-up extraction order", "Hierarchical section order in the file". Verify the tests, including EX-056's empty child.
- [ ] 8.2 Implement interactions. Covers "Interaction halo and step squares", "Interaction area". Verify the tests, including EX-055's coupling across step-square borders.
- [ ] 8.3 Implement pass 1. Covers "Hierarchical label names", "Interaction processing and connections (pass 1)", "Name unification order". Verify the tests with EX-049 and EX-051.
- [ ] 8.4 Implement pass 2 and hierarchical coupling. Covers "Merge records and pass-2 adjustments", "Hierarchical coupling records". Verify the tests with EX-049 and EX-050, and that `merge` lines come out in Magic's hash order.
- [ ] 8.5 Implement the remaining hierarchy features. Covers "Hard-way node names", "Array interactions", "Sticky labels across the hierarchy" (Q-EXTRACT-26 replicate). Verify the tests with EX-052..054.
- [ ] 8.6 Run `check_requirement_coverage.py --capability extract-hierarchy` (12 of 12) and the complexity check on `extract/hier/`.

## 9. EXTCMD: extract-commands (R2 options, R3 driver; Q-EXTRACT-27)

- [ ] 9.1 Implement the command's options. Covers "Command syntax and option matching", "Extraction options", "Option defaults", "Warnings". Verify the tests, and EX-067's listing.
- [ ] 9.2 Implement the run driver with the three locking phases of design D12. Covers "Extract all, cell, parents and showparents", "Cell properties affecting extraction", "Incremental extraction" (with Q-EXTRACT-27 as decided: temporary file and rename). Verify:
  - the tests, including EX-064's rewrite set;
  - a cancel test showing the previous `.ext` intact and no temporary file left;
  - that rendering reads the cell during the extract phase.
- [ ] 9.3 Implement the style and path options. Covers "Style, halo and step size" (Q-EXTRACT-25), "Output path and file location" (Q-EXTRACT-11 per platform). Verify the tests, including a sandboxed-host test with the project folder as the current directory, and SCRIPT-API-20's `extract style` result.
- [ ] 9.4 Implement the unique command and the messages. Covers "Unique command and temporary unique option" (with EXTNODE's renamer), "Progress and summary messages" (informative). Verify the tests with EX-024.
- [ ] 9.5 Port ExtLength.c. Covers "Path length extraction" (Q-EXTRACT-12, tile-exact). Verify the tests with EX-068.
- [ ] 9.6 Run `check_requirement_coverage.py --capability extract-commands` (11 of 11 exact or behavioural, plus the informative "Progress and summary messages" documented) and the complexity check on `extract/{command,length}/`.

## 10. EXTFLAT: ext-flattening-read, -hierarchy, -names (R2)

Extends M6's code (design D13). The 18 requirements M6 covered keep their tests, and every task below re-runs them.

- [ ] 10.1 Complete the read side. Covers "Device records" (every class, parameters, attributes), "Parameters records" (Q-NETLIST-8 per mode, Q-NETLIST-9 reversed order), "Resistance-extraction overlay". Also add the `resist` offset of Q-NETLIST-12 per mode. Verify the tests with NL-FLAT-04, 05 and 09, and a `.res.ext` overlay test.
- [ ] 10.2 Complete flattening. Covers "Common netlister arguments" (EFArgs; Q-NETLIST-42 fix), "Coupling capacitance flattening", "Lumped node resistance", "Device length and width", "Visit orders for output" (devices, caps and resistors, completing M6's node visits), "One-level flattening for hierarchical output". Keep `read_flat` as M6's wrapper. Verify the tests with NL-FLAT-10..16, and that DEFW's tests and the LEFDEF cases pass unchanged.
- [ ] 10.3 Implement name conversion. Covers "SPICE output name conversion" (Q-NETLIST-30 per mode, the leading-comma guard in both), "Sim output name conversion" (Q-NETLIST-16 per mode). Verify the tests with NL-FLAT-17 and 18.
- [ ] 10.4 Close M6 task 9.2's visit-order comparison with Magic's probe lines, if M6 has not. Extend `fuzz_ext_reader` into `fuzz_ext_netlist` once M6's target has landed (design D22). Verify the comparison on the 17 DEF-write `.ext` inputs.
- [ ] 10.5 Run `check_requirement_coverage.py --capability ext-flattening-read` (11 of 11), `--capability ext-flattening-hierarchy` (11 of 11) and `--capability ext-flattening-names` (7 of 7), and the complexity check on `extflat/`.

## 11. SPICE: netlist-spice-options, -devices, -hierarchy (R2–R3)

Develops against the NETLIST cases captured in task 2.5.

- [ ] 11.1 Implement the command and the file. Covers "Command options" (Q-NETLIST-13, Q-NETLIST-41 per mode), "Run arguments and output file" (Q-NETLIST-42), "File header and scale card". Verify the tests, and NL-SPICE-01 and 02.
- [ ] 11.2 Implement values, names and parasitics. Covers "SI value formatting" (pinned values per leg), "Node names per dialect", "Parasitic and node lines" (Q-NETLIST-19, Q-NETLIST-20 per mode). Verify the tests, and NL-SPICE-03, 04 and 07.
- [ ] 11.3 Implement the netlist structure. Covers "Flat netlist structure" (Q-NETLIST-37 per mode), "Subcircuit header line", "Subcircuit instance lines", "Global card in hierarchical mode". Verify the tests, and NL-SPICE-05, 06 and 08.
- [ ] 11.4 Implement MOSFETs and parameters. Covers "Device instance names" (Q-NETLIST-34 per mode), "Device eligibility and annotations", "MOSFET lines", "Terminal area and perimeter" (Q-NETLIST-7 per mode), "Device parameter output" (Q-NETLIST-4, Q-NETLIST-32 per mode). Verify the tests, and NL-SPICE-09 and 13.
- [ ] 11.5 Implement the other devices and merging. Covers "Resistor lines" (Q-NETLIST-5, Q-NETLIST-31 per mode), "Capacitor, diode, BJT and voltage source lines", "Subcircuit-class device lines", "Substrate terminal text", "Parallel device merging" (Q-NETLIST-6 fix). Verify the tests, and NL-SPICE-10, 11, 12 and 14.
- [ ] 11.6 Implement hierarchical output. Covers "Definition traversal order", "Port promotion", "Omitted, stubbed and absorbed definitions", "Section layout", "Subcircuit calls in hierarchical mode", "Hierarchical device and parasitic differences" (Q-NETLIST-35 per mode). Verify the tests, and NL-HIER-01..06.
- [ ] 11.7 Implement `render` for M9 (design D18). Verify a test that renders with the `lvs` preset and with simulation overrides, and shows `netlist.spice` unchanged afterwards.
- [ ] 11.8 Run `check_requirement_coverage.py --capability netlist-spice-options` (10 of 10), `--capability netlist-spice-devices` (10 of 10) and `--capability netlist-spice-hierarchy` (6 of 6), and the complexity check on `netlist/spice/`.

## 12. SIM: netlist-sim, netlist-extcheck (R2–R3; Open Question 6)

- [ ] 12.1 Implement the command and device lines. Covers "Command options and defaults" (Q-NETLIST-18 per mode, Q-NETLIST-14 fix), "File structure and order", "Device lines" (Q-NETLIST-17 per mode). Verify the tests, and NL-SIM-01 and 02.
- [ ] 12.2 Implement attributes, parasitics and name state. Covers "Transistor attributes", "Parasitic and node records" (Q-NETLIST-15 per mode), "Name conversion state" (Q-NETLIST-16 per mode). Verify the tests, and NL-SIM-03 and 04.
- [ ] 12.3 Document "Deprecated sim2simp filter" (informative) as not provided (Q-NETLIST-48) in the command help and `docs/reference/`. Verify that the help text names `ext2sim merge`.
- [ ] 12.4 Implement extcheck. Covers "Invocation" (Q-NETLIST-36 defaults), "Counts and report format". Add `hefesto-extcheck` (Open Question 6 (a)). Verify the tests, NL-CHK-01, and the exit status 1 on a usage error.
- [ ] 12.5 Run `check_requirement_coverage.py --capability netlist-sim` (6 of 6, the informative one documented) and `--capability netlist-extcheck` (2 of 2), and the complexity check on `netlist/{sim,extcheck}/`.

## 13. RESNET: extresist-network (R4)

- [ ] 13.1 Implement net collection and fracturing through `db::StitchEditor` (design D6, D16). Covers "Net geometry collection" (the `skip` masks of Q-NETLIST-23), "Tile fracturing". Verify the tests with NL-RESNET-06 and 07's tile dump, and a debug check that no cell plane reaches the stitch editor.
- [ ] 13.2 Implement device annotation, the start tile and breakpoints. Covers "Device annotation", "Start tile and origin", "Breakpoints", "Tile processing order". Verify the tests with NL-RESNET-08.
- [ ] 13.3 Implement the resistors. Covers "Tile resistors", "Near-device cutting" (Q-NETLIST-43 per mode), "Contact resistors". Verify the tests with NL-RESNET-01, 03, 09 and 10.
- [ ] 13.4 Implement node merging and reductions. Covers "Node merging", "Construction-time reductions" (Q-NETLIST-44 per mode, Q-NETLIST-45 fix). Verify the tests with NL-RESNET-11 and 15.
- [ ] 13.5 Implement path resistance and pruning. Covers "Path resistance and capacitance distribution" (Q-NETLIST-24 replicate; Q-NETLIST-25, Q-NETLIST-46 per mode), "Elmore delay and pruning". Verify the tests with NL-RESNET-05, 12 and 13, and pinned Elmore values per leg.
- [ ] 13.6 Implement simplification. Covers "Network simplification", "Final scrunch pass" (Q-NETLIST-26 stable sort, Q-NETLIST-47 guards). Verify the tests with NL-RESNET-04 and 14.
- [ ] 13.7 Run `check_requirement_coverage.py --capability extresist-network` (15 of 15) and the complexity check on `extresist/network/`.

## 14. RESCMD: extresist-command, extresist-output (R4)

- [ ] 14.1 Implement the command. Covers "Command options" (Q-NETLIST-23 fix, Q-NETLIST-28 ps, Q-NETLIST-21 default), "Run flow". Verify the tests, and NL-RES-02.
- [ ] 14.2 Implement the input model and cutoffs. Covers "Input model" (ResReadExt over EXTFLAT's tokenizer; Q-NETLIST-29 per mode with its guards in both), "Net selection and first cutoff", "Second cutoff after extraction", "Lumped resistance file". Verify the tests, and NL-RES-01, 03 and 04.
- [ ] 14.3 Implement the overlay output. Covers "Overlay file grammar and order" (Q-NETLIST-39 per mode), "Naming of network nodes", "Consumption by netlisters" (with EXTFLAT's overlay reader). Verify the tests, and NL-RES-05, NL-RES-06 and NL-SIM-05.
- [ ] 14.4 Implement the FastHenry and geometry output. Covers "FastHenry geometry file" (Q-NETLIST-40 per mode; Q-NETLIST-27 and Q-NETLIST-21 fixes), "Geometry centerlines". Verify the tests, and NL-RES-07 and 08.
- [ ] 14.5 Run `check_requirement_coverage.py --capability extresist-command` (6 of 6) and `--capability extresist-output` (5 of 5), and the complexity check on `extresist/{command,output}/`.

## 15. ANTENNA: antenna-check (R3)

- [ ] 15.1 Expose the antenna rules as `antenna::RuleView` over `ExtStyle`. Covers "Antenna tech-file keywords grammar", "Antenna coefficient unit conversion" (with EXTTECH's parser). Verify the tests.
- [ ] 15.2 Implement the command. Covers "Antenna check command" (no plane order: MISC-ANT-07's error), "Gate nets visited once", "Violation reporting". Verify the tests, and MISC-ANT-07 and 08.
- [ ] 15.3 Implement the measurement. Covers "Per-plane progressive evaluation" (Q-MISC-1 per mode), "Antenna area measurement" (Q-MISC-4, Q-MISC-7 per mode; Q-MISC-3), "Antenna ratio test". Verify the tests, and MISC-ANT-01..06.
- [ ] 15.4 Switch `lef write` to the shared rules (design D17, D20). Covers "Shared antenna definitions for LEF output". Verify the tests, MISC-ANT-09, and that every LEFDEF antenna case still passes.
- [ ] 15.5 Run `check_requirement_coverage.py --capability antenna-check` (9 of 9) and the complexity check on `antenna/`.

## 16. SHELL: desktop (R4; Open Question 7)

- [ ] 16.1 Enable the top bar's "Extract" button: run `extract all` on the active window's root with the progress panel and cancel, enabled only when the technology has an extract style, with a tooltip otherwise (design D19). Record the states in `docs/design/m7-ui-conformance.md`. Verify the shell view-model tests (complexity 8–12 per function) and a screenshot of each state.

## 17. GOLDEN: EXTRACT, NETLIST, MISC-ANT, PDK and deferred cases (R1–R5; Open Questions 4 and 9)

- [ ] 17.1 Author EX-001..070 with `authoring/extract_cases.py` and the committed `exttest.tech`. Capture them as they are authored, the flat ones first. Verify that `m7_cases.py --not-authored` lists none of them and that each replays once its workstream's task is done.
- [ ] 17.2 Audit the catalogues against the 204 exact and behavioural requirements. For each requirement whose behaviour Magic shows and that no case or pinned unit test covers, add a row (EX-071 on, NL-<family>-<n+1>). Verify that the audit table, kept in each CASES.md, lists every requirement with its case or test.
- [ ] 17.3 Author and capture NL-RES-01..08 and NL-RESNET-01..15 (23 cases) and MISC-ANT-01..09. Verify that they replay after sections 13–15.
- [ ] 17.4 Author `EXTRACT-PDK-<lib>` and `NETLIST-PDK-<lib>` for `sky130_fd_sc_hd`, `gf180mcu_fd_sc_mcu7t5v0` and `sg13g2_stdcell` (Open Question 4 (a)), with `authoring/ext_pdk_cases.py`, `--trust-pdk` and the PDK's default style. Verify that they are captured, that every cell's `.ext` and netlist pass `ext-cap` and `spice-cap`, and that a failing cell is named in the report.
- [ ] 17.5 Replay SCRIPT-API-16 and SCRIPT-API-20 and remove their `deferred = "M7"` entries. Drop the M7 commands from UI-G-005's deviation reason. Verify that both cases pass and that UI-G-005 still lists only its M10 and M12 commands.
- [ ] 17.6 Add LEFDEF-336 (Open Question 9 (a)): `extract all` then `def write` on LEFDEF-317's layout. Verify that it replays against Magic.
- [ ] 17.7 Record the deviations of design D21 for the cases that exercise a both-modes fix, each with its decision. Verify that `m7_cases.py --exit-check` lists each with its decision and nothing else.

## 18. PERF: benches (R5; Open Questions 5 and 8)

- [ ] 18.1 Add `bench/extract/` (`extract all`, flat `ext2spice lvs`, `ext2spice hierarchy on`, `ext2sim` on the `gds-bench` block) and `bench/extresist/` (`extresist all` on a routed sky130 block), measuring callgrind instructions for Hefesto and Magic. Verify that a dispatch run reports both counts.
- [ ] 18.2 Add the `extract-bench` and `extresist-bench` jobs to `perf.yml`, with path filters on `src/core/{include/hefesto,src}/{extract,extflat,netlist,extresist,antenna}/**` and `bench/{extract,extresist}/**`. Gate as Open Question 5 decides: at most Magic's count, then the recorded baseline + 2 %, for `extract-bench`; the baseline + 2 % for `extresist-bench`. Verify that `drc-profile` and `gds-bench` still pass.

## 19. INTEGRATION (every round; exit in R6)

- [ ] 19.1 Apply the shared-file open items of sections 2–18 each round: CMake, the `EditorState` members, the tech hook shared with M10, the command registrations, the `golden-capture.yml` and `perf.yml` jobs, and `check_core_includes.py`. Verify that every macOS preset configures, builds and tests with `-DHEFESTO_WARNINGS_AS_ERRORS=ON`.
- [ ] 19.2 Add `fuzz_extract_section`, `fuzz_ext_netlist`, `fuzz_res_ext_reader` and `fuzz_extract_layout` with seeds and resource caps (design D22), and wire them into the fuzz job. Verify that the seed replays pass on every leg and that the 60 s job runs each target without a crash.
- [ ] 19.3 Review the new readers and extraction on hostile layouts (design D24, M7-S1..S8), and write the findings in `docs/contributing/security.md`. Draft each cap that changes behaviour as a proposed decision for the user, and never approve it here. Verify that each finding is fixed with a regression test or recorded as an accepted limit.
- [ ] 19.4 Add `--require-area EXTRACT --require-area NETLIST` to the determinism job once the expected outputs exist. Verify that the job is identical on every leg.
- [ ] 19.5 Verify that no `CORE-NOTIMPL` use or stub of M7 remains, retire the code in its table ("Retired in M7, never reused"), and verify that the diagnostics tests pass.
- [ ] 19.6 Run the exit list of design D25: coverage of 204 of 204 across the 21 capabilities, `m7_cases.py --exit-check`, the determinism job, the fuzz job, the security findings, the benches, and the complexity check over every M7 directory. Update `docs/roadmap.md` (M7 status), `CHANGELOG.md`, `docs/contributing/` (areas, fuzz targets, golden modules) and `docs/building.md`. Verify that `openspec validate --all --strict` passes and that `python3 scripts/gen_specs_index.py` leaves no diff.
