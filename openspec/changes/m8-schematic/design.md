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
- **Checks.** `scripts/check_cleanroom.py` (run in CI's docs job) fails when a committed file carries xschem's GPL header, when a golden case under `tests/golden/SCHEM/` declares no `provenance` or declares xschem as its source, or when a file under `src/` or `tests/` contains an `xschem ref:` trace (captured `expected/` output and Markdown documents excepted). The pull-request template gains a checkbox for schematic code: "I have not read xschem's source; I worked from the specs only." The review checklist of `hefesto-licensing` rejects a pull request whose author is on the spec team.
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
| System symbol library | `assets/schematic/xschem_library/devices/` (under `${XSCHEM_SHAREDIR}` = `<CAD_ROOT>/assets/schematic`, where the PDK `xschemrc` files look) | DEVLIB | impl |
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

A hand-written lexer and reader over the shared record grammar, producing `schem::Document`; embedded blocks nested in blocks are kept on an explicit stack, at most 64 levels (Q-SCHEM-32, approved 2026-11-21), never by recursion. Brace strings keep their escapes until a property is read. Numeric fields are binary64 values with no source text kept: every value, edited or not, is printed by the spec's `%.16g` rule with `std::to_chars` (no locale, no `printf`), which is what xschem writes ("Numeric fields"; SCHEM-RT-02 confirms it on 1373 of 1375 PDK files). (Corrected 2026-11-21 by the spec team, round-1 spec issue S1: this paragraph used to say that numeric source text is kept.) Writing follows "Canonical write order" from the model, not from the source text, so the round-trip guarantee is tested as a property: read → write → read → write is a fixpoint, and for every PDK file read → write equals the input after Q-SCHEM-1 normalisation. Embedded blocks are written by the xschem rules in both modes (Q-SCHEM-2). The first-record version rule is the Q-SCHEM-3 branch point. Safe saving reuses the platform file service (write to a temporary file, then rename). The reader is fuzzed (D19).

### D7. Symbols, libraries and Tcl (SYMLIB; `schematic-symbols-and-libraries`)

- **References and search path** exactly as specified; `http` references are never fetched and resolve to the missing-symbol placeholder with a warning (Q-SCHEM-9).
- **`xschemrc` as data** runs in a fresh sandboxed interpreter with synthetic `env()` mapped to the PDK store and the document folder (Q-SCHEM-23); the result is the search path and the variables the inventory lists. `xschem_execute_scripts` is read, ignored and logged (Q-SCHEM-25).
- **`tcleval`** runs in the X-3 sandbox (`script/tcl/sandbox.hpp`) with the procedures the inventory lists (`ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::*`, the `xschem` subset), implemented from the semantics the spec team writes into "tcleval values" from the public manual and computed PDK values (Q-SCHEM-8, G3). Their numeric output is checked against values computed for every PDK symbol that uses them (SCHEM-NL-07/08 and unit tables).
- **Pin order** uses `std::stable_sort` and warns on duplicates (Q-SCHEM-5).
- **Symbol drc procedures** run in the sandbox (Q-SCHEM-6); generators are Tcl only (Q-SCHEM-11).

### D8. The system symbol library (DEVLIB; Q-SCHEM-10, J-3)

1. PDK runs the inventory (D18) and commits the list of referenced `devices/` symbols.
2. The spec team extracts each listed symbol's interface into `schematic-symbols-and-libraries` "System library": file name, pin names, order and positions, pin directions, and the netlisting properties (`type`, `format`, `lvs_format`, `template`, `global`, `spice_ignore` and the like), as one table. Graphics are not described beyond a bounding box.
3. DEVLIB writes each `.sym` from that table with independently drawn graphics, under `assets/schematic/xschem_library/devices/` (the PDK `xschemrc` files append `${XSCHEM_SHAREDIR}/xschem_library/devices`), with Apache-2.0 headers in the `v` record. A test parses every library file and checks its interface against a machine-readable copy of the table (`tests/data/schem/devices_interface.json`, generated from the spec by a script), and that the library holds exactly the inventoried symbols.
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

`fetch_pdk.py` gains `sky130-xschem` (`sky130A/libs.tech/xschem`, from `xschem_sky130` @ `2bfd7d65` as installed by open_pdks @ `c6df28c1`), `gf180mcu-xschem` (`gf180mcuD/libs.tech/xschem`, from `gf180mcu_fd_pr/cells/xschem` @ `5e4be872`, re-pinned from `aacc04cc`, see below) and `ihp-xschem` (`ihp-sg13g2/libs.tech/xschem` @ `5e6d592e`, plus `libs.ref/sg13g2_stdcell/sym/xschem`), with tree digests and cache keys like the M5 sets, and `tests/unit/common/reference_data.hpp` paths with clean skips. The sets install from the same ciel releases as M5 (Open Question 1, option (a)) under their own PDK root `.cache/pdks/xschem/`, like the GDS sets.

**Open Question 1 settled (task 2.1, 2026-11-17).** The ciel releases of M5 carry the xschem trees in their `common` archive, except IHP's standard-cell symbols, which are in the `sg13g2_stdcell` archive. No other `libs.ref/*/sym/xschem` directory exists in these releases (sky130's standard-cell symbols are `libs.tech/xschem/sky130_stdcells`; gf180's I/O libraries keep theirs in `libs.ref/<lib>/xschem`, which no SCHEM case uses; IHP's `libs.ref/sg13g2_stdcell/sch/xschem` is not on the `xschemrc` search path). Digests use the `fetch_pdk.py` record format; "tree" digests are taken with the tree directory as base (paths relative to it), so that an installed tree and an upstream checkout compare directly.

| Set | ciel archives (download) | Kept (files, bytes) | Set digest (`--print-digest`) | Tree digest of the ciel content | Pinned tree | Tree digest of the pinned tree |
|---|---|---|---|---|---|---|
| `sky130-xschem` | `common` 6.6 MB, `sky130_fd_pr` 13.9 MB (`-lib` only) | 955, 6 278 199 (953 in `libs.tech/xschem`; `libs.tech/netgen`, 2 files, added for SCHEM-LVS-01) | `8a3b28a791383eb5c6f546acb3a6fd42e91ebb954fa986257e7851d2447a78d8` | `b7846b6ee589419359480d6411eefe2eab8b67d9866b85ed7e4595b1ae5299c5` | `xschem_sky130` @ `2bfd7d65` (975 files) | `a18c67d3cdb60e7f631f6ff860a8eadd7ddcfd765e12e3ca7a131c8cbc508cf0` |
| `gf180mcu-xschem` | `common` 170.4 MB, `gf180mcu_fd_pr` 0.05 MB (`-lib` only) | 269, 452 450 | `e70989764fe362e8dc1d59824768c7dbc988eeab11c7ac714a3e6f118e69bad6` | `d4232cb82b56c929e29fed22e088d6c622a3b252e5202a5b561b8cf5dc368c10` | `gf180mcu_fd_pr` @ `aacc04cc` `cells/xschem` (177 files); @ `5e4be872` (195 files) | `64bb12b326848592e12aced63d164db56264002dc9d1d0ce9468b7be1e761148`; `2657432bdcc6b8d9f33366003a7bb17c44a90c1463d7e1b91eed1f07ac18020d` |
| `ihp-xschem` | `common` 434.4 MB, `sg13g2_stdcell` 6.4 MB | 362, 3 175 481 | `a38b0850a131fbe4f169b7332541da0012281765419523d7fad255af6a36498b` | `libs.tech/xschem` `e44975987e852c38b95a8f4e154f868afefd07c47fae4b114adb20cc2f0f76c1` (278 files); `sym/xschem` `eadce115fb4de57cac1ad0761fe662e5dd277e27accd7bde6e25f29deac15e6e` (84 files) | IHP-Open-PDK @ `5e6d592e`, the same two directories | equal to the ciel content |

What differs, file by file (`diff -rq`):
- **sky130: the pin, as installed.** The ciel tree is `xschem_sky130` @ `2bfd7d65` (open_pdks 5740c4a8's `sky130.json` reference block names this commit) with the installer's edits only: `decred_hash_macro/` is not installed; the sky130B-only ReRAM files (`sky130_fd_pr/reram.sym`, `sky130_tests/tb_reram.sch`, `tb_reram.sym`) and their two instances in `sky130_tests/top.sch` are removed for sky130A; an "open_pdks specific" block (start window under `${PDK_ROOT}`, `XSCHEM_USER_LIBRARY_PATH`) is appended to `xschemrc`. The specs already cite this tree "as installed by open_pdks"; no re-pin.
- **gf180mcu: a different commit.** The ciel tree comes from `gf180mcu_fd_pr` @ `5e4be8726e2ae8be099c4131f697a353d6457874` (the commit open_pdks' `gf180mcu.json` names), 145 commits after `aacc04cc`. Against `aacc04cc`: the symbol directory is `gf180mcu_fd_pr/` (installed twice, as `gf180mcu_fd_pr/` and as its copy `symbols/`), 16 symbols are new (`nfet3_*`, `pfet3_*`, `pfet_05v0`, `annotate_fet_params`, `annotate_op`), 18 differ (MOS and bipolar `format` strings gain `par=@m` after `m=@m`, the MOS operating-point texts use `to_eng` and `.m0`, the `npolyf_*` templates correct their `model`), every test schematic differs, and `xschem-menu` is new. Against `5e4be872` the only differences are the installer's: `symbols/` added, `user_xschemrc` not installed, and `xschemrc` gets open_pdks' `PDK_ROOT` search, `180MCU_MODELS` path and an appended block. Decision, per option (a): **re-pin the gf180 data to `5e4be872` as installed in `gf180mcuD/libs.tech/xschem`** (spec `schematic-symbols-and-libraries` "PDK reference data", `SCHEM.md` header, `tests/golden/SCHEM/CASES.md` inputs). The conventions the specs cite for gf180 (`+` continuation lines, `ad='int((nf+1)/2) * W/nf * 0.18u'`, `lvs_format` without `@spiceprefix`) and the files SCHEM-NL-09/NL-10 use hold in the re-pinned tree; no `Resolution:` changes.
- **IHP: identical.** Both directories equal IHP-Open-PDK @ `5e6d592e` byte for byte.

Reproduce: `scripts/fetch_pdk.py sky130-xschem gf180mcu-xschem ihp-xschem` (with a `hefesto-cli` that has `pdk install`) and `--print-digest <set>` give the set digests and sizes. The upstream trees come from `curl -L https://codeload.github.com/<owner>/<repo>/tar.gz/<commit> | tar -xzf - --include '<prefix>/*'` (`StefanSchippers/xschem_sky130`, `fossi-foundation/globalfoundries-pdk-libs-gf180mcu_fd_pr` `*/cells/xschem/*`, `IHP-GmbH/IHP-Open-PDK` `*/ihp-sg13g2/libs.tech/xschem/*` and `*/ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem/*`); a tree digest is `fetch_pdk.tree_digest(Path(<tree>), (".",))`, and `diff -rq <upstream tree> <installed tree>` lists the differences above. The reference commits are in `sky130/sky130.json` and `gf180mcu/gf180mcu.json` of open_pdks @ `5740c4a8` (block `reference`).

### D18. The inventory script (PDK; Q-SCHEM-8, -10, -21)

`scripts/schem_inventory.py` reads only the three PDK data sets and writes `tests/data/schem/inventory.json` with sorted lists: referenced `devices/` symbols (both `devices/<name>` and bare forms that resolve there), every `tcleval(` body's Tcl commands and `$::` variables, every `xschem` subcommand in launchers, attributes and `xschemrc` files, and every `subcircuit` symbol whose `template` contains an LF. `scripts/tests/test_schem_inventory.py` runs it on fixture trees; a CI step re-runs it against the cached data sets and fails on any difference from the committed file. SYMLIB, DEVLIB and CMD tests read the committed file.

### D19. Fuzz targets

`fuzz_sch_reader` (whole `.sch`/`.sym` files), `fuzz_schem_props` (property strings and brace strings), `fuzz_schem_expr` (the `expr` evaluator, both modes) and `fuzz_schem_bus` (bus expansion), with corpora seeded from the Hefesto-authored golden inputs only (not from PDK files, to keep the corpus small and license-clean).

### D20. Determinism

Same output on Linux x86_64, Linux aarch64 and macOS arm64: number parsing and printing through `from_chars`/`to_chars`; map iteration only over ordered containers or explicit sorts; the device-model hash order exactly as specified; Tcl evaluation in the C locale with a fixed `tcl_precision`. A cross-platform digest test netlists every PDK example and compares digests across CI legs.

### D21. Exit criteria

- The SCHEM golden cases pass (D16), including every PDK round trip and the PDK example netlists. Two kinds of listed case do not block the exit (decisions of 2026-11-18 and 2026-11-22): SCHEM-SIM-01 and SIM-02, whose simulation results move to M9, and SCHEM-LVS-01, whose LVS result needs the `lvs-*` capabilities of M9 (Q-SCHEM-39) (all `deferred = "M9"`, captures kept), and SCHEM-PR-05, which may differ only in `stdout.txt` and `stderr.txt` by the approved deviation Q-SCHEM-30.
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

All three were decided by the user on 2026-11-13, each as option (a), the recommended one.

1. **Source of the xschem PDK data.** (Settled by task 2.1 on 2026-11-17, see D17: gf180 re-pinned to `5e4be872`.) (a) *Recommended:* install `libs.tech/xschem` from the same ciel releases as M5 and check their tree digests against the pinned PDK commits of `SCHEM.md`; if they differ, re-pin the specs and golden plan to the ciel content (one consistent PDK set); (b) fetch the pinned PDK repositories directly (exact commits, a second source to cache); (c) both, with the pinned repositories only for capture.
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
| Q-SCHEM-38 (approved 2026-11-22) | platform, option (a): `env(HOME)`/`env(PWD)` the document's folder, relative library elements against it (`SCHEM-LIB-001`), unknown `xschemrc` commands are Tcl errors except the PDK GUI-only ones; both modes | D7, D14 |
| Q-SCHEM-42 / M8-D1 (approved 2026-11-22) | platform: flat netlist stops at 64 levels or past 256 MiB, `SCHEM-NET-017`, run not failed; both modes | D9, D10, D19 |
| Q-SCHEM-40 / M8-D2 (approved 2026-11-22) | platform: schematic used as a symbol capped at about 256 MiB besides 1,000,000 records, `SCHEM-LIB-009`; both modes | D7, D19 |
| Q-SCHEM-41 / M8-D3 (approved 2026-11-22) | platform: total bus expansion 4 GiB (64 × `kMaxBusBytes`) per netlist run or editing session, `SCHEM-NET-006`; both modes | D9, D19 |

## As-landed scaffold contracts

The scaffold (tasks.md 5.1–5.3, branch `m8-r1-scaffold`) landed the area, headers and stubs below. The headers are the contract; this section records where they are, who fills them in, and where they differ from "Area contracts", D4 and D5. Include paths are under `src/core/include/hefesto/`, sources under `src/core/src/`. Everything is in namespace `hefesto::schem` (one namespace for the area; the names do not collide).

### Files and owners

| Path | Contents | Owner (fills in) |
|---|---|---|
| `schem/{sources.cmake,codes.cpp}` | area sources; the code table assembled from `schem/<sub>/codes.inc` (topics `SCHEM-FILE`, `-SYM`, `-LIB`, `-TCL`, `-NET`, `-EDIT`, `-CMD`, `-SIM`) | SCAFFOLD |
| `schem/<sub>/{sources.cmake,codes.inc}` for `file sym lib tcl net edit cmd sim` | one source list and code fragment per subdirectory (no codes yet) | the D3 owner |
| `schem/not_implemented.hpp`, `src/schem/not_implemented.cpp` | `not_implemented(what) -> diag::Diagnostic` (`CORE-NOTIMPL-007`, constant `diag::codes::kSchemNotImplemented`) | SCAFFOLD (removed at M8 exit) |
| `schem/session.hpp` | `DocumentId`, `OpenDocument {id, document}`, `Session {documents, next_id, symbols, search_path, xschemrc}`; `EditorState::schem` | SCAFFOLD (members: EDIT `documents`/`next_id`, SYMLIB the rest) |
| `schem/file/document.hpp` | `FileKind`, `kLayerCount`, `Point`, `LineRecord` (= `BoxRecord`), `ArcRecord`, `PolygonRecord`, `TextRecord`, `WireRecord`, `InstanceRecord`, `GlobalProperties {k,g,v,s,f,e}`, `Document`, `EmbeddedSymbol {reference, symbol}` | FORMAT |
| `schem/file/read_write.hpp`, `src/schem/file/read_write.cpp` | `kWriterVersion` (`3.4.8RC`), `ReadOptions {kind, xschem_compat, warnings}`, `read_document`, `write_document` | FORMAT |
| `schem/file/properties.hpp`, `src/schem/file/properties.cpp` | `Property {key, raw_value, has_value}`, `PropertyList`, `ValueForm {decoded, raw}`, `parse_properties`, `get_token`, `set_token` | FORMAT |
| `schem/sym/symbol.hpp`, `src/schem/sym/symbol.cpp` | `PinIndex`, `Pin {name, direction, box}`, `Symbol {reference, file, document, pins, embedded, placeholder}`, `pin_order` | SYMLIB |
| `schem/lib/search_path.hpp`, `src/schem/lib/search_path.cpp` | `SearchPath` (`directories`, static `from_xschemrc`), `XschemrcInput {rc_file, env, document_folder}`, `XschemrcResult {search_path, variables, diagnostics}` | SYMLIB |
| `schem/lib/symbol_cache.hpp`, `src/schem/lib/symbol_cache.cpp` | `Reference {text}`, `ResolveContext {search_path, fs, warnings}`, `SymbolCache` (`resolve`, `placeholder`, `clear`, `size`) | SYMLIB |
| `schem/tcl/tcl_bridge.hpp`, `src/schem/tcl/tcl_bridge.cpp` | `kTclFailure` (`?` LF), `TclContext {path, sch_basename, instname, symname}`, `TclBridge(script::Interpreter*)::evaluate` | SYMLIB |
| `schem/net/{netlist,expr,bus}.hpp`, `src/schem/net/{netlist,expr,bus}.cpp` | `Hierarchy`, `NetlistOptions` (the eight options of "Netlist options" with their defaults, plus `portable_paths`), `ComputedLabel {schematic, instance, lab}`, `NetlistResult {text, diagnostics, failed, labels}`, `netlist`; `evaluate_expr`; `expand_bus` | NETLIST |
| `schem/edit/{hier_path,operations}.hpp`, `src/schem/edit/operations.cpp` | `HierPath {elements}`; `EditOperation {option, handler}`, `edit_operations` | EDIT |
| `schem/cmd/commands.hpp`, `src/schem/cmd/commands.cpp` | `register_schematic_command(cmd::CommandRegistry&)`, `register_xschem_command(script::Interpreter&, Session&)` | CMD |
| `schem/sim/deck.hpp`, `src/schem/sim/deck.cpp` | `Deck {text, diagnostics}`, `build_deck`, `vector_name` | SIMINT |
| `compat/compat_settings.hpp` | `CompatSettings::xschem_compat` (default true), `xschem_compat_netlist_line` | COMPAT |
| `settings/project_settings.hpp` | `kXschemCompat`, `kPortableNetlistPaths`, `ProjectSettings::portable_netlist_paths` (default false); `setting_names()` is magic-compat, netgen-compat, xschem-compat, portable-netlist-paths, script-strict | COMPAT |
| `tests/unit/schem/` and `tests/unit/schem/<sub>/` | `hefesto_schem_tests` (shared) and one executable per subdirectory once it has a `*_test.cpp` (data: `tests/data/schem/<sub>`) | SCAFFOLD; the D3 owners |
| `tests/unit/schem/scaffold_test.cpp` | the `[m8-scaffold]` tests of the stubs' documented values | SCAFFOLD; owners replace them with `[cap:][req:]` tests |
| `tests/unit/schem/compat_isolation_test.cpp` | "xschem-compat off leaves layout output unchanged" (GDS before and after `setting set xschem-compat off`) | COMPAT |
| `docs/diagnostics/codes/schem.md` | one section per workstream with codes (FORMAT, SYMLIB, NETLIST, EDIT, CMD, SIMINT) and the generated table | SCAFFOLD (sections: their workstreams) |

### Stub values

`read_document` and `build_deck` fail with `CORE-NOTIMPL-007`; `netlist` returns `failed` with that diagnostic and an empty text; `SearchPath::from_xschemrc` returns an empty result with that diagnostic; `write_document` returns an empty text; `parse_properties` finds nothing, `get_token` returns nullopt and `set_token` returns its input; `pin_order` returns file order; `SymbolCache::resolve` always returns the placeholder; `TclBridge::evaluate` returns `kTclFailure`; `evaluate_expr` returns the expression unchanged; `expand_bus` returns the name itself; `edit_operations` is empty; `vector_name` returns the net unchanged; the `schematic` and `xschem` commands answer `CORE-NOTIMPL-007`. Neither command is registered: CMD wires `schematic` into `cmd::register_all_commands` and `xschem` into the project interpreter when it implements them.

### Differences from "Area contracts", D4 and D5

- **Document.** D4's "records in file order" is one list per record kind, each in read/creation order (the writer regroups by layer, "Canonical write order"); embedded `[` ... `]` blocks are `Document::embedded`. `Document` gains `path` (empty when unsaved), because relative references resolve against the containing schematic's folder (Q-SCHEM-30) and `SymbolCache::resolve` receives only the document. Coordinates are plain `double`: D6's "numeric fields keep their source text" is not in the model, since "Numeric fields" writes every value with `%.16g` (scenario "Re-formatting a hand-edited coordinate"); FORMAT may add source text if a golden case needs it.
- **read_document** reports the warnings of "Malformed input" through `ReadOptions::warnings` (a `diag::DiagnosticSink*`), since `diag::Outcome<Document>` carries only the failure.
- **SymbolCache::resolve** takes a third argument, `ResolveContext` (search path, file system, warning sink), since the cache holds no reference to them. `SearchPath::from_xschemrc(platform::FileSystem&, const XschemrcInput&)` returns `XschemrcResult`; how it obtains its fresh sandboxed interpreter (D7) is SYMLIB's choice.
- **TclBridge** is constructed over a `script::Interpreter*` (not owned; nullptr fails every evaluation).
- **vector_name** takes the `Session` as well (`vector_name(const Session&, const HierPath&, net)`): step 2 of "Simulator vector names" resolves port nets through the parent schematics. `HierPath` is in `schem/edit/hier_path.hpp` (EDIT owns "Hierarchy path").
- **EDIT and CMD.** There is no `cmd::Command` class in the core; commands are `cmd::CommandSpec` handlers. EDIT's operations are therefore `EditOperation {option, cmd::Handler}` entries that CMD's `schematic` dispatcher looks up; a handler reaches the session through `CommandContext::editor()->schem`. The Tcl interpreter type is `script::Interpreter` (from `script::create_project_interpreter`), not `script::TclInterpreter`.
- **NetlistOptions** carries the project setting `portable-netlist-paths` as `portable_paths`, so `netlist` needs no `ProjectSettings`.
- **D5.** `xschem_compat_netlist_line` returns the empty string when `xschem-compat` is ON (no line). The settings' JSON keeps `setting_names()` order in the settings store; `hefesto.project` writes its settings object with sorted keys, as before.
- **Edits outside the area.** `src/core/CMakeLists.txt` (`HEFESTO_CORE_AREAS`), `tests/unit/CMakeLists.txt` (area list), `diag/codes.cpp`, `diag/codes.hpp` and `src/diag/area_codes.hpp` (the `schem` area, `CORE-NOTIMPL-007`), `docs/diagnostics/codes.md`, `editor/editor_state.hpp` (`schem`), `compat/` and `settings/` (D5), and, because the setting list grew: `app/settings_layers.cpp` and `app/project_file.cpp` (now loop over `setting_names()` instead of naming three settings), the `setting` help text in `cmd/builtins.cpp`, and the expected texts of `tests/unit/core/cmd_test.cpp`, `tests/unit/core/settings_test.cpp` and `tests/unit/app/project_file_test.cpp`.

### Round 1 contract additions (FORMAT, SYMLIB, NETLIST)

Recorded by INTEGRATION when the round-1 branches were merged (`m8-round1`). The headers are the contract; this list says what each workstream added to the scaffold's headers.

- **FORMAT.** `schem/file/save.hpp` (new): `file_kind_of`, `load_document`, `SaveOptions`, `save_document` (through `FileSystem::write_atomic`, refusing with `SCHEM-FILE-006` when the file changed on disk). `schem/file/transform.hpp` (new): `Placement`, `transform_point`, `transform_text`, `transform_arc_start`, `box_centre`. `properties.hpp`: `decode_value`, `is_true`, `is_false`. `read_write.hpp`: `brace_string`, `ReadOptions::source_name`. `document.hpp`: `Document::disk_digest`. `read_document` never fails; the embedded write rule is documented on `EmbeddedSymbol`. Numeric fields are plain `double`s printed as `%.16g`; D6's "source text kept" is not implemented (spec issue on D6 vs "Numeric fields", raised by the scaffold).
- **SYMLIB.** `Pin::sim_pinnumber`; `Symbol::property()`; a `pin_order` overload with a `diag::DiagnosticSink*`; `ResolveContext::xschem_compat` (last member, default `true`); `SearchPath::aliases()`, `parse()`, `prepended()` and `LibraryAlias`, with the `.` element stored literally; `XschemrcInput::variables`; `kMaxSymbolDepth` = 40; `SymbolCache::forget(path)`. New headers `schem/sym/types.hpp`, `schem/sym/instance.hpp`, `schem/lib/reference.hpp`, `schem/lib/schematic_symbol.hpp`. SYMLIB uses FORMAT's `is_true` and FORMAT's `transform.hpp` for placed graphics. Embedded symbols are cached per document path and block content (unsaved documents share the empty path), and `forget(path)` also drops the embedded symbols of the document saved there.
- **NETLIST.** New headers `schem/net/connectivity.hpp` (`SchematicNets`, `name_nets` -> `NetNaming`, `pin_point`, `touches`; pin positions use FORMAT's `transform_point`, the one orientation transform, which mirrors only for flip 1 as "Orientation transform" states), `schem/net/substitute.hpp` (`PropertySource`, `PropertyString`, `PropertyMap`, `select_format`, `resolve_parameter`, `substitute_from_parents`, `build_element` -> `ElementText`; `boolean_value` is FORMAT's `is_true`/`is_false`), `schem/net/codes.hpp`; `bus.hpp` (`parse_bus`, `expand_bus`, `replace_bus_chars`, `join_bits`, `kMaxBusBits`); `expr.hpp` (`evaluate_expr`, `evaluate_expr_value`, `substitute_expr`, `format_g`, `format_engineering`, `lex_number`, `kMaxExprDepth`). D9's `%lf` scan uses `std::strtod` in the "C" locale, not `std::from_chars`: `from_chars` for `double` is unavailable on the macOS 14.0 deployment target (FORMAT does the same). `ComputedLabel` (netlist.hpp) will gain a kind field (instance or wire) with 8.4, so EDIT/CMD can apply Q-SCHEM-29 wire `lab` values too.
