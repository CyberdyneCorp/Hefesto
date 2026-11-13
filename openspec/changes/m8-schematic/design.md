# Design: M8 Schematic capture

> Drafted 2026-11-13 against the decisions the user approved that day (`docs/decisions/m8-decision-table.md`: the 24 open SCHEM entries, D-21 and judgement items J-1 to J-5, all as recommended). Open Questions 1–3 at the end are not yet decided. The table "Decisions this design depends on" lists, for each decision, the D-numbers it shapes.

## Context

See proposal.md for motivation and what exists today. The conventions of M0–M5 hold:
- SPDX headers;
- Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`;
- per-area diagnostic code tables `AREA-TOPIC-NNN` in `docs/diagnostics/codes/<area>.md`;
- cognitive complexity at most 15 per function in the core and 8–12 in the shell view models, 25–35 for the parsers and evaluators (`schem/file`, `schem/net/expr`), with irreducible functions flagged;
- no UI, GPU, Raven or process headers in the core (`scripts/check_core_includes.py`);
- golden expected outputs captured only in CI;
- golden cases run with the compatibility settings ON, and OFF behaviours are unit-tested, one test per mode.

What is new in M8 is the reference tool. xschem is GPL-2.0-or-later, and the specs were written by reading its source. Every decision below is shaped by the two-team clean-room process (J-1, `hefesto-licensing` "Two-team clean-room process for specs written from GPL source").

## Goals / Non-Goals

**Goals:**
- Every requirement of the six `schematic-*` capabilities (101), except the parts of `schematic-simulation-integration` that need M9 (D13).
- Byte-identical `.sch`/`.sym` round trips of every PDK file, and netlists identical to xschem's for the PDK example schematics after path normalisation (the roadmap's M8 exit criteria), with `xschem-compat` ON.
- One branch point per compat entry (Q-SCHEM-3, -12, -14, -19, -21, -22), each tested in both modes.
- A process that keeps xschem's source out of the implementation, with checks that make violations visible.

**Non-Goals:** as in proposal.md "Non-goals".

## Decisions

### D1. Gates and order

| Gate | Passes when | Unlocks |
|---|---|---|
| G0 decisions | the M8 decision table approved, specs aligned, `check_milestone_readiness.py --milestone M8` ready | done 2026-11-13 |
| G1 clean room | CLEANROOM tasks 1.1–1.4 done: roles recorded, implementation-session rules in place, paraphrase review of the six specs and `SCHEM.md` done, `check_cleanroom.py` in CI | every implementation task |
| G2 golden | the 41 SCHEM cases authored and captured from xschem 3.4.8RC with gawk (roadmap principle 2); the spec team has recorded the capture-fixed texts (Q-SCHEM-14, -21, -22, -27, -28) | FORMAT, SYMLIB, NETLIST, CMD, SIMINT |
| G3 interface | the spec's system-library interface table and Tcl procedure semantics written from the inventory (SPEC) | DEVLIB, the Tcl part of SYMLIB |
| Exit | D21 | M8 done |

Order: CLEANROOM and PDK first (they need nothing), then GOLDEN and SPEC in parallel, then the implementation workstreams in dependency order FORMAT → SYMLIB → NETLIST, with COMPAT and SCAFFOLD before them, EDIT and CMD after FORMAT, SIMINT after NETLIST, SHELL after EDIT. SECURITY reviews once SYMLIB's script paths exist. Nothing in M8 needs M5–M7 code; whether M8 implementation starts before M7 is finished is Open Question 3.

### D2. The two-team process in practice (CLEANROOM; J-1)

- **Roles.** The spec team writes and changes `schematic-*` specs, `SCHEM.md` and the system-library interface table; it may read xschem's source and run xschem. The implementation team writes code, tests and golden inputs; it never reads xschem's source. Each workstream of D3 belongs to one team; the D3 table says which. A person or agent session is assigned to one team for M8 and does not switch. `docs/contributing/cleanroom.md` records the roster (roles, not names, for agents) and the rules.
- **Implementation sessions.** Agent sessions for implementation start from a worktree of this repository, whose only xschem material is the specs. Their instructions forbid fetching or opening xschem's repository, the `xschem` directory of the capture image (`/opt/src/xschem`) and the `Evidence:` and `(xschem ref: ...)` paths. Questions the spec does not answer are raised as spec issues for the spec team, never filled by looking.
- **Allowed observation.** Running the xschem binary in the capture runner is black-box observation and is allowed for both teams. The xschem manual pages (`xschem_man/*.html`) are public documentation and may be read by both teams.
- **Paraphrase review.** Before G1 the spec team reviews the six specs and `SCHEM.md` for code paraphrase (source identifiers that are not file-format or user-visible names, the structure of xschem's awk or Tcl programs, pseudo-code mirroring C) and rewrites such text as behaviour. The two netlisting post-processing requirements ("Element line post-processing", "Line breaking"), whose current text follows xschem's awk scripts step by step, are the first to review; the rewrite keeps the observable rules and drops any step order that is not observable.
- **Checks.** `scripts/check_cleanroom.py` (run in CI's docs job) fails when a committed file carries xschem's GPL header, when a golden case under `tests/golden/SCHEM/` declares no `provenance` or declares xschem as its source, or when a file under `src/` contains an `xschem ref:` trace. The pull-request template gains a checkbox for schematic code: "I have not read xschem's source; I worked from the specs only." The review checklist of `hefesto-licensing` rejects a pull request whose author is on the spec team.
- **Counsel.** One counsel review of the process and of the interface table before Desktop 1.0, tracked in the M13 exit (not blocking M8).

### D3. Layout of the code, workstreams and ownership

The core area is `schem`, diagnostic prefix `SCHEM-`, code document `docs/diagnostics/codes/schem.md`:

| Module | Path | Owner | Team |
|---|---|---|---|
| File grammar, reader, writer | `schem/file/` | FORMAT | impl |
| Symbol model, pins, tokens | `schem/sym/` | SYMLIB | impl |
| References, search path, `xschemrc`, cache, generators | `schem/lib/` | SYMLIB | impl |
| `tcleval`, xschem Tcl procedures, permission model | `schem/tcl/` | SYMLIB | impl |
| Connectivity, naming, substitution, `expr`, post-processing, writer | `schem/net/` | NETLIST | impl |
| Document model, editing operations, highlight | `schem/edit/` | EDIT | impl |
| `schematic` and `xschem` commands | `schem/cmd/` | CMD | impl |
| Schematic circuit source and deck building | `schem/sim/` | SIMINT | impl |
| System symbol library | `assets/schematic/devices/` | DEVLIB | impl |
| Desktop screen | `src/shell/desktop/src/schematic_*.cpp` | SHELL | impl |
| Specs, `SCHEM.md`, interface table, capture-fixed texts | `openspec/specs/schematic-*`, `docs/decisions/` | SPEC | spec |
| Inventory script, PDK data sets | `scripts/schem_inventory.py`, `scripts/fetch_pdk.py` | PDK | either (reads only Apache-2.0 PDK trees) |
| Golden inputs and capture | `tests/golden/SCHEM/`, `tools/golden/` | GOLDEN | impl (inputs) |
| Process, checks | `docs/contributing/cleanroom.md`, `scripts/check_cleanroom.py`, PR template | CLEANROOM | spec |
| Compatibility settings | `compat/`, `settings/` | COMPAT | impl |
| Security review | review record | SECURITY | reviewer outside both teams |

Each core subdirectory has its `sources.cmake` and `codes.inc`. Workstreams edit only what this table assigns them; shared-file needs are raised as open items to INTEGRATION.

### D4. Document and session state

A schematic document (`schem::Document`) holds the records in file order (instances, wires, graphics, texts, global properties, embedded symbols), the header text, the file version read and the modified flag. It is independent of the layout database: schematics and layouts are separate documents. `EditorState` gains a `schem::Session` with the open schematic documents, the symbol cache (keyed by resolved absolute path), the search path and the active PDK's `xschemrc` result. Netlisting reads a document and the cache and writes only the computed `lab` values (Q-SCHEM-29), without touching the undo log or the modified flag.

### D5. The compatibility settings (COMPAT; D-21, Q-SCHEM-13)

- `compat::CompatSettings` gains `bool xschem_compat = true`, and `settings::ProjectSettings` gains the names `xschem-compat` and `portable-netlist-paths` (default off), stored in `hefesto.project` in that fixed order after `netgen-compat`. The test in `tests/unit/core/settings_test.cpp` that uses `xschem-compat` as an unknown name switches to another unknown name.
- `compat::xschem_compat_netlist_line(settings)` returns `** xschem-compat=off` or nothing; the netlist writer inserts it after the first `** sch_path:` line. `magic_compat_header_tag` is not used by schematic netlists.
- Branch points, one function each, all in the impl team's modules: Q-SCHEM-3 `schem/file` (first-record version), Q-SCHEM-12 `schem/net/expr` (number lexing), Q-SCHEM-14 `schem/net` (subcircuit naming), Q-SCHEM-19 and Q-SCHEM-21 `schem/net` (header template filter), Q-SCHEM-22 `schem/net` (post-processing step 2). Each takes the setting explicitly; none reads global state. Warnings that the entries give in both modes are emitted outside the branch.

### D6. File format (FORMAT; `schematic-file-format`)

A hand-written lexer and recursive reader over the shared record grammar, producing `schem::Document`. Brace strings keep their escapes until a property is read; numeric fields keep their source text so that unedited records write back byte-identically, and edited values are printed by the spec's rule with `std::to_chars` (no locale, no `printf`). Writing follows "Canonical write order" from the model, not from the source text, so the round-trip guarantee is tested as a property: read → write → read → write is a fixpoint, and for every PDK file read → write equals the input after Q-SCHEM-1 normalisation. Embedded blocks are written by the xschem rules in both modes (Q-SCHEM-2). The first-record version rule is the Q-SCHEM-3 branch point. Safe saving reuses the platform file service (write to a temporary file, then rename). The reader is fuzzed (D19).

### D7. Symbols, libraries and Tcl (SYMLIB; `schematic-symbols-and-libraries`)

- **References and search path** exactly as specified; `http` references are never fetched and resolve to the missing-symbol placeholder with a warning (Q-SCHEM-9).
- **`xschemrc` as data** runs in a fresh sandboxed interpreter with synthetic `env()` mapped to the PDK store and the document folder (Q-SCHEM-23); the result is the search path and the variables the inventory lists. `xschem_execute_scripts` is read, ignored and logged (Q-SCHEM-25).
- **`tcleval`** runs in the X-3 sandbox (`script/tcl/sandbox.hpp`) with the procedures the inventory lists (`ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::*`, the `xschem` subset), implemented from the semantics the spec team writes into "tcleval values" from the public manual and computed PDK values (Q-SCHEM-8, G3). Their numeric output is checked against values computed for every PDK symbol that uses them (SCHEM-NL-07/08 and unit tables).
- **Pin order** uses `std::stable_sort` and warns on duplicates (Q-SCHEM-5).
- **Symbol drc procedures** run in the sandbox (Q-SCHEM-6); generators are Tcl only (Q-SCHEM-11).

### D8. The system symbol library (DEVLIB; Q-SCHEM-10, J-3)

1. PDK runs the inventory (D18) and commits the list of referenced `devices/` symbols.
2. The spec team extracts each listed symbol's interface into `schematic-symbols-and-libraries` "System library": file name, pin names, order and positions, pin directions, and the netlisting properties (`type`, `format`, `lvs_format`, `template`, `global`, `spice_ignore` and the like), as one table. Graphics are not described beyond a bounding box.
3. DEVLIB writes each `.sym` from that table with independently drawn graphics, under `assets/schematic/devices/`, with Apache-2.0 headers in the `v` record. A test parses every library file and checks its interface against a machine-readable copy of the table (`tests/data/schem/devices_interface.json`, generated from the spec by a script), and that the library holds exactly the inventoried symbols.
4. The library is reachable as `devices/<name>` and `<name>`. Its shipping in a release waits for the counsel review (D2), which is an M13 item.

### D9. Netlisting (NETLIST; `schematic-netlisting`)

A pipeline of pure stages over one hierarchy run: connectivity (exact binary64 geometry, spatial index per schematic) → net naming → element text (format substitution, hierarchical parameters, `expr`, `tcleval`) → file structure and subcircuit definitions → post-processing → line breaking. Post-processing and line breaking are written natively from the spec's rules (no awk, no external program; gawk's case-insensitive semantics on every platform, Q-SCHEM-18). Netlisting runs in memory; writing a file is a separate step. All shorts are reported (Q-SCHEM-16). Tokens end as in xschem, with the prefix warning (Q-SCHEM-26). `expr` uses `std::from_chars` for the `%lf` scan and a `%.15g`-equivalent formatter built on `std::to_chars`, so results are identical on every platform (D20). Flat mode substitutes subcircuit bodies on the parsed hierarchical result and is compared behaviourally (D10).

### D10. Flat netlists and the canonical SPICE comparator (Q-SCHEM-15)

`tools/golden/compare/spice_canonical.py` parses a SPICE netlist into devices (name, ordered nodes, model, parameters), with subcircuit expansion where definitions exist, names compared case-insensitively, node names canonicalised by connectivity (two netlists match when a node bijection maps one device set onto the other with equal models and parameter values), and reports the first difference. SCHEM-NL-21 uses it; byte comparison is never used for flat output. The comparator is Hefesto code written from SPICE semantics, not from xschem.

### D11. Editing core (EDIT; `schematic-editing`)

Editing operations are commands on `schem::Document` through the `schematic` command (so they are logged and replayable), each one undo step, recorded by a schematic undo recorder on the existing undo log. Snap, stretch and wire routing work in document coordinates with exact arithmetic. Net highlighting reuses NETLIST's connectivity for one schematic and the hierarchy path for descend/ascend. Per-document undo history survives descend and ascend. Read-only libraries are enforced at the command level.

### D12. Commands (CMD; `schematic-command`)

`schematic <option>` follows Magic-style matching (unique prefixes, the error table) and every option of "Subcommand table"; it works headless and in hefesto-cli. The `xschem` Tcl command maps the inventoried subcommands onto `schematic` and returns xschem's result formats; any other subcommand raises `xschem command '<cmd>' is not supported`. The inventory test fails when a PDK bump adds a subcommand (Q-SCHEM-10).

### D13. Simulation integration: M8 part and M9 part (SIMINT)

| Requirement | M8 | M9 |
|---|---|---|
| Schematic circuit source | the deck text and its validation hand-off | running it |
| Testbench code blocks | yes | |
| Control block translation | yes | |
| Simulator vector names | yes (name mapping) | used by plots |
| Simulation launchers | confirmation and sandboxed run; `sim` calls return CORE-NOTIMPL until M9 | full |
| Operating-point annotation, Embedded graphs, Waveform cross-probing | data model and drawing hooks only | yes |
| Schematic to layout cross-probing, Schematic-driven LVS command | | yes (needs `lvs-*`) |
| Results interchange with xschem | | yes (raw files) |

The M9-dependent requirements are listed as deferred with this reason in the requirement coverage report (`check_requirement_coverage.py`), and M9's plan takes them over.

### D14. Script permission model and its review (SECURITY; Q-SCHEM-25)

`tcleval`, `@tcleval` and symbol `drc` procedures run in the sandbox without prompting; launcher `tclcommand` scripts are confirmed once per document per session through the existing consent UI (`script/pdk/consent.hpp`) and run in the same sandbox; denials give the sandbox diagnostic and the value `?`. A reviewer outside both teams checks the model against `hefesto-security` ("Script sandbox enforcement", "Schematic script permission model") with the PDK testbenches and hostile test schematics (exec, socket, file access outside granted folders, `load`, `interp` escapes), and records the sign-off in the M8 exit table. M8 cannot exit without it.

### D15. Desktop (DESIGN, SHELL)

The Schematic tab opens the schematic editor per `Schematic.dc.html` (canvas, symbol library browser, properties panel, hierarchy breadcrumb, netlist action, messages) with xschem-style default bindings, rebindable through the existing key-binding sheet. The "Waveforms" tab is relabelled "arrive in M9". The floating tool palette (Q-SCHEM-24) is a view model in the shell library, tested headless and shown on desktop as an optional toolbar; its iPad presentation, the gesture set and the finger-vs-Pencil usability test are M14 items recorded in the roadmap's de-risking schedule. Progress and cancellation reuse the M4/M5 patterns. `docs/design/m8-ui-conformance.md` records the conformance checks.

### D16. Golden cases and capture (GOLDEN; J-1)

- **Image.** The capture image installs `gawk` and makes it `/usr/bin/awk` (update-alternatives), with a build-time check that `awk --version` reports GNU Awk; mawk stays installed but is not `awk`. xschem is already built at the pinned commit. Captures run with `xschem_execute_scripts` set to `yes`, `--network=none`, and the xschem PDK data sets (D17) mounted read-only.
- **Inputs.** PDK example files (Apache-2.0) and Hefesto-authored files only; every `case.toml` gains `provenance = "pdk:<set>:<path>"` or `"hefesto"`. SCHEM-RT-05, NL-01 and NL-19 have new Hefesto-authored inputs. No file from xschem's distribution is committed (D2 check).
- **Comparison.** Exact after normalising `** sch_path:` and `** sym_path:` lines (Q-SCHEM-13) and the version of inputs written by another xschem (Q-SCHEM-1); flat netlists by D10; Hefesto runs with `xschem-compat` ON and portable paths off.
- **Capture-fixed texts.** After capture the spec team records, in the specs and the entries, the exact texts of Q-SCHEM-14 (SCHEM-NL-16), Q-SCHEM-21 (a case added for a multi-line template, unless the inventory shows a PDK case), Q-SCHEM-22 (SCHEM-NL-01 and a user-code case), Q-SCHEM-27 (SCHEM-NL-14) and Q-SCHEM-28 (SCHEM-NL-15), and confirms that SCHEM-NL-21 passes the D10 comparator against itself. None of them changes a decision; if a capture contradicts a spec rule, the spec team corrects the rule and records it.
- **Round trips.** SCHEM-RT-02 covers every `.sch` and `.sym` of the three PDK xschem trees; files whose second xschem save differs from the first are listed, not failed.

### D17. PDK xschem data sets (PDK)

`fetch_pdk.py` gains `sky130-xschem` (`sky130A/libs.tech/xschem`, from `xschem_sky130` @ `2bfd7d65` as installed by open_pdks @ `c6df28c1`), `gf180mcu-xschem` (`gf180mcu_fd_pr/cells/xschem` @ `aacc04cc`) and `ihp-xschem` (`ihp-sg13g2/libs.tech/xschem` @ `5e6d592e`, plus `libs.ref/sg13g2_stdcell/sym/xschem`), with tree digests and cache keys like the M5 sets, and `tests/unit/common/reference_data.hpp` paths with clean skips. Whether the ciel releases used in M5 install exactly these trees is Open Question 1.

### D18. The inventory script (PDK; Q-SCHEM-8, -10, -21)

`scripts/schem_inventory.py` reads only the three PDK data sets and writes `tests/data/schem/inventory.json` with sorted lists: referenced `devices/` symbols (both `devices/<name>` and bare forms that resolve there), every `tcleval(` body's Tcl commands and `$::` variables, every `xschem` subcommand in launchers, attributes and `xschemrc` files, and every `subcircuit` symbol whose `template` contains an LF. `scripts/tests/test_schem_inventory.py` runs it on fixture trees; a CI step re-runs it against the cached data sets and fails on any difference from the committed file. SYMLIB, DEVLIB and CMD tests read the committed file.

### D19. Fuzz targets

`fuzz_sch_reader` (whole `.sch`/`.sym` files), `fuzz_schem_props` (property strings and brace strings), `fuzz_schem_expr` (the `expr` evaluator, both modes) and `fuzz_schem_bus` (bus expansion), with corpora seeded from the Hefesto-authored golden inputs only (not from PDK files, to keep the corpus small and license-clean).

### D20. Determinism

Same output on Linux x86_64, Linux aarch64 and macOS arm64: number parsing and printing through `from_chars`/`to_chars`; map iteration only over ordered containers or explicit sorts; the device-model hash order exactly as specified; Tcl evaluation in the C locale with a fixed `tcl_precision`. A cross-platform digest test netlists every PDK example and compares digests across CI legs.

### D21. Exit criteria

- The SCHEM golden cases pass (D16), including every PDK round trip and the PDK example netlists.
- `check_requirement_coverage.py` covers every requirement of the six capabilities, or lists it as deferred to M9 with D13's reason.
- Unit, property and fuzz tests green; output byte-identical across the three platforms (D20).
- Each compat branch point tested in both modes; `xschem-compat` OFF changes no layout, extraction or LVS output (`hefesto-magic-compat-mode` scenarios).
- The `hefesto-security` sign-off of D14 recorded.
- `check_cleanroom.py` passes and every schematic pull request carries the clean-room checkbox.
- `openspec validate --all --strict` passes.

## Area contracts

### FORMAT: `hefesto/schem/file/`
`read_document(std::string_view bytes, const ReadOptions&) -> diag::Outcome<Document>` (ReadOptions carries the file kind and `xschem_compat`); `write_document(const Document&) -> std::string`; `parse_properties(std::string_view) -> PropertyList`; `get_token`/`set_token` with xschem's update rules.

### SYMLIB: `hefesto/schem/sym/`, `hefesto/schem/lib/`, `hefesto/schem/tcl/`
`SymbolCache::resolve(Reference, const Document&) -> const Symbol&` (placeholder on failure); `SearchPath::from_xschemrc(...)`; `TclBridge::evaluate(std::string_view, const TclContext&) -> std::string` (returns `?` LF on failure); `pin_order(const Symbol&) -> std::vector<PinIndex>`.

### NETLIST: `hefesto/schem/net/`
`netlist(const Session&, const Document& top, const NetlistOptions&, const compat::CompatSettings&) -> NetlistResult` (text, diagnostics, failure flag, computed labels); `evaluate_expr(std::string_view, bool xschem_compat) -> std::string`; `expand_bus(std::string_view) -> std::vector<std::string>`.

### EDIT and CMD: `hefesto/schem/edit/`, `hefesto/schem/cmd/`
Editing operations as `cmd::Command` implementations behind `schematic`; `register_xschem_command(script::TclInterpreter&, Session&)`.

### SIMINT: `hefesto/schem/sim/`
`build_deck(const Session&, const Document& testbench) -> diag::Outcome<Deck>` for `simulation-netlist-preparation`'s `schematic` source; `vector_name(const HierPath&, std::string_view net) -> std::string`.

## Risks / Trade-offs

- **R1. Spec gaps under the clean room.** Implementers cannot look up what a spec leaves open. → The golden cases come first (G2), and spec issues go to the spec team with a turnaround target of one working day; a gap that blocks a workstream is an INTEGRATION open item.
- **R2. Paraphrase in the netlisting specs.** The post-processing requirements follow xschem's awk scripts closely. → They are reviewed first and rewritten as behaviour before NETLIST starts (D2); the goldens (SCHEM-NL-01..23) pin the behaviour independently of the wording.
- **R3. Capture surprises.** The specs were written without running xschem. → G2 comes before implementation; contradictions are spec corrections made by the spec team, not code workarounds.
- **R4. Counsel rejects the interface table.** → The library is built in M8 but ships only after the review; the fallback is the decision table's option (b) for Q-SCHEM-10 (a user-installed xschem library on desktop), which needs no code change beyond the search path.
- **R5. Tcl numeric formatting.** `ev7` and `to_eng` outputs depend on Tcl's formatting. → Fixed `tcl_precision`, C locale and computed-value tables for every PDK use.
- **R6. PDK trees differ from the pinned commits.** → Open Question 1 and digest checks.
- **R7. Security review late.** → SECURITY starts as soon as SYMLIB's sandbox paths land, not at the end.

## Migration Plan

No user data changes. The project file gains `xschem-compat` and `portable-netlist-paths`; files without them read the defaults. The Schematic tab becomes enabled.

## Open Questions

1. **Source of the xschem PDK data.** (a) *Recommended:* install `libs.tech/xschem` from the same ciel releases as M5 and check their tree digests against the pinned PDK commits of `SCHEM.md`; if they differ, re-pin the specs and golden plan to the ciel content (one consistent PDK set); (b) fetch the pinned PDK repositories directly (exact commits, a second source to cache); (c) both, with the pinned repositories only for capture.
2. **Who draws the system-library graphics.** (a) *Recommended:* DEVLIB draws them from the interface table's pin positions and bounding boxes, with the design system's symbol style; (b) the design workstream draws them in the board first; (c) generate minimal graphics (box and pin stubs) automatically.
3. **When M8 implementation starts relative to M6 and M7.** (a) *Recommended:* follow the roadmap order: CLEANROOM, PDK, GOLDEN and SPEC (no product code) may run now; implementation starts after M7 exits; (b) start implementation as soon as G1 and G2 pass, in parallel with M6/M7, since no code depends on them; (c) start after M9 planning, so the simulation part is planned once.

## Decisions this design depends on

| Decision (approved 2026-11-13) | Option | Design sections it shapes |
|---|---|---|
| J-1 clean-room provenance | (a) two-team process, Hefesto-authored golden inputs, counsel before Desktop 1.0 | D1, D2, D3, D8, D16, D19 |
| D-21 `xschem-compat` | (a) setting, default ON, netlist line only when OFF | D5, D6, D9, D16 |
| Q-SCHEM-10 / J-3 system library | (a) redraw only PDK-used symbols, interface table with counsel sign-off | D8, D18 |
| Q-SCHEM-4 / J-4 netlisters | (a) SPICE only, records preserved | D6, D9 |
| Q-SCHEM-25 / J-5 permissions | (a) sandbox always, launchers once per document, security sign-off | D7, D14, D21 |
| Q-SCHEM-3, -12, -14, -19, -21, -22 | compat | D5, D6, D9 |
| Q-SCHEM-1, -2, -18, -20, -26, -27, -28, -29 | replicate | D6, D9, D16 |
| Q-SCHEM-15 | replicate (behavioural) | D9, D10 |
| Q-SCHEM-5, -16 | fix | D7, D9 |
| Q-SCHEM-8 | other (inventory, public-doc semantics) | D7, D18 |
| Q-SCHEM-9, -13, -24 | platform | D5, D7, D15 |
