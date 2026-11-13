# M8 decision table: the 29 SCHEM open questions and D-21

M8 ("Schematic capture", docs/roadmap.md) cannot start until every open question its specs cite has a `Resolution:` line. `python3 scripts/check_milestone_readiness.py --milestone M8` lists 24 blocking entries, all in `docs/decisions/open-questions/SCHEM.md`. This table covers all 29 Q-SCHEM entries: the 24 blocking ones plus the five already resolved (Q-SCHEM-6, -7, -11, -17, -23), which are listed for completeness. It also covers D-21, the open item in X-2 on whether xschem quirks get their own `xschem-compat` setting. Most of the kinds below depend on that item. That gives 30 rows.

**Status: drafted 2026-11-13, reviewed 2026-11-13, approved 2026-11-13.** The user approved every recommendation, the recommended option of all five judgement items included (see "Judgement items"), with no option changed. Each of the 24 open entries in `SCHEM.md` now carries `Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)` and a `Resolution:` line; the five entries resolved earlier carry a `Status:` line that confirms them. Where the table changes an entry's original recommendation, its `Recommendation:` line ends with `(corrected 2026-11-13, ...)`. D-21 is recorded in `cross-cutting.md` (X-2) and `audit/DECISIONS.md`. `python3 scripts/check_milestone_readiness.py --milestone M8` reports `M8: ready`.

## Clean-room method and sources

P-12 and `hefesto-licensing` Requirement "Clean-room rule for GPL-sourced behavior" require the implementation to be clean-room. While drafting this table, no xschem source was read, cloned or browsed: no C, Tcl or awk files, and no GitHub code views. The xschem binary is not installed on this machine (`which xschem` finds nothing), so no reference behaviour was run either. gawk is not installed either. The sources used are listed below. Each row's **Src** column names the ones it relied on.

- **[E]**: the Hefesto entry in `docs/decisions/open-questions/SCHEM.md`, meaning its behaviour description and options. Its `Evidence:` lines cite xschem source by file and line. This table repeats none of them, and they were not followed.
- **[S]**: Hefesto's own specs: `schematic-*`, `hefesto-magic-compat-mode`, `hefesto-licensing`, `hefesto-scripting` and `simulation-netlist-preparation`.
- **[G]**: `tests/golden/SCHEM/CASES.md`, the golden capture plan.
- **[D]**: Hefesto decisions: `product.md` (P-9, P-12, P-17), `cross-cutting.md` (X-1 to X-4, D-21) and `roadmap.md`.
- **[M]**: the public xschem manual, specifically `xschem_man/developer_info.html` (file format, the `v` record, embedded symbols, `lab` on wires) and `xschem_man/netlisting.html`.

Because no binary was available, no row could be checked against xschem's actual behaviour. Six rows can only be closed after golden capture (Q-SCHEM-14, -15, -21, -22, -27, -28; see "Blocked on capture"). Their recommendation is still the decision; capture only fixes exact texts.

**The clean-room provenance needed a decision.** The header of `SCHEM.md` says the xschem tree "was cloned read-only" and that the expected netlist texts in the specs "are derived from the code". So the specs were written by reading GPL source, and the implementers must work from the specs only. That is a valid two-team clean room only if two things hold: the spec authors and the implementers are kept apart, and the spec text describes behaviour rather than paraphrasing code. This is judgement item J-1. The user approved J-1 (a) on 2026-11-13, so both conditions are now requirements of `hefesto-licensing`.

## Recommendation kinds

These follow X-2 (docs/decisions/cross-cutting.md). They assume D-21 is answered (a), which adds an `xschem-compat` setting.

- **replicate**: do what xschem does, in both modes.
- **compat**: replicate with `xschem-compat` ON and use the corrected behaviour with it OFF (tier 2).
- **fix**: corrected behaviour in both modes. This covers tier 1, and tier 3 when xschem has no defined output to match.
- **platform**: a Hefesto platform decision (X-3/X-4).
- **other**: a scope or process decision.

The standing rule matches M5 and M7. A quirk that changes netlist or file **content** is compat. A row uses replicate instead when the corrected behaviour would itself break PDK netlists (Q-SCHEM-26), or when the quirk only changes byte layout that round-trip identity needs. A row uses fix when xschem's output is unspecified or diagnostic-only. Four compat rows also warn in both modes (Q-SCHEM-12, -14, -19, -21). The warning changes no output.

## Judgement items

Five items needed the user's decision on product, legal, scope or security grounds. On 2026-11-13 the user took the recommended option (a) of each, with the details below, and the recommended default of every other row.

- **J-1 (a).** Two-team process: spec authors never implement the capability they specified, and implementers never follow source references (`Evidence:` lines, `(xschem ref: ...)` traces). It is written into `hefesto-licensing`. Spec text is reviewed for code paraphrase. No xschem source tree is reachable from implementation sessions. Golden inputs are Hefesto-authored only (xschem's own example files are not used). One counsel review of the process before Desktop 1.0, not blocking M8.
- **D-21 (a).** `xschem-compat`, default ON, owned by `hefesto-magic-compat-mode` beside `magic-compat` and `netgen-compat`; written into a netlist only when OFF (one `** xschem-compat=off` line); `SCHEM.md` moves out from under `magic-compat`.
- **Q-SCHEM-10 (a).** Re-draw only the symbols the three PDKs use: names, pins and properties kept, graphics independent; the interface table is extracted under the J-1 process with counsel sign-off; the list comes from an inventory script.
- **Q-SCHEM-4 (a).** SPICE only in M8; other-format records are preserved through round trips; Verilog is revisited with P-17.
- **Q-SCHEM-25 (a).** Attribute scripts always run sandboxed; launcher scripts are confirmed once per document; `hefesto-security` sign-off before M8 exits.

The options as drafted:

1. **J-1: Clean-room provenance of the SCHEM specs (legal).** The specs and `SCHEM.md` were written by reading xschem's GPL source and cite it by line. The implementation must not use that source.
   - (a) **Recommended.** Keep the two-team process and write it down. Add a scenario to `hefesto-licensing` saying that spec authors who read GPL source never implement the matching capability, and that implementers never follow `Evidence:` paths into xschem. Have each spec text reviewed for literal code paraphrase, such as variable names, awk program structure, or pseudo-code that mirrors C. Keep any xschem source tree off the machines and out of the context of implementation sessions (none is present on this machine), and use only Hefesto-authored files as golden-case inputs (see "Blocked on capture" for SCHEM-RT-05). Have counsel review the process once, before Desktop 1.0, without blocking M8.
   - (b) Re-derive the netlisting specs (`schematic-netlisting` Requirements "Element line post-processing", "Line breaking" and "Flat netlists") from public documentation and black-box golden captures only, and drop the source-line refs from specs and `SCHEM.md`. This is safer and costs about one spec pass plus the golden capture, which is needed anyway.
   - (c) Get a legal opinion before any M8 implementation starts.
2. **D-21: Whether xschem gets an `xschem-compat` setting (product).**
   - (a) **Recommended.** Add `xschem-compat` (default ON) to `hefesto-magic-compat-mode`, governing entries in `SCHEM.md` that record `Resolution: compat`. As with `netgen-compat`, it changes no layout, extraction or LVS output. Unlike `magic-compat`, it is written into a netlist only when OFF (one `** xschem-compat=off` comment line after the first `** sch_path:` line), so default netlists stay byte-identical to xschem's (`schematic-netlisting` is `Compatibility: exact`). This keeps one mental model for all three reference tools, and lets users get valid SPICE where xschem's output is broken (Q-SCHEM-21, -14).
   - (b) No setting. Each quirk is decided once (replicate or fix), with no toggle. Specs and tests are simpler, but every "fix" breaks byte identity with xschem netlists permanently, and every "replicate" keeps known-wrong output.
   - (c) Reuse `magic-compat` for xschem quirks. This is what the settings table in `hefesto-magic-compat-mode` does today by default (`magic-compat` governs "every open-questions file except those listed below", and `SCHEM.md` is not listed). It is not recommended: switching `magic-compat` OFF to fix layout quirks would also change schematic netlists, which is the coupling D-2 removed for netgen.
   - The D-21 audit entry also listed "replicate always" (no setting, every quirk replicated). It is covered by (b) with every row decided as replicate, which would keep invalid SPICE for Q-SCHEM-21.
3. **Q-SCHEM-10: The clean-room `devices/` symbol library (legal and cost).** PDK schematics reference xschem's GPL `devices/*.sym` (127 files).
   - (a) **Recommended.** Re-draw, clean-room, only the symbols that the three PDKs and their examples reference. Specify each by file name, pin names and order, pin positions and netlisting properties, all of which form the functional interface. Draw the graphics independently. Generate the list by an inventory script over the PDK trees, which are Apache-2.0. This needs counsel sign-off that matching the interface (names, pins, `format` templates) is acceptable. `format` strings are short functional templates, but they are the closest thing to copied text in M8. The interface has no public source other than the GPL `devices/*.sym` files themselves: pin positions in particular must match exactly, or PDK wires stop connecting. So the interface facts are extracted from those files by the spec side of the J-1 process, and `schematic-symbols-and-libraries` Requirement "System library" already quotes `type`, `format` and `template` values from them. Counsel's sign-off covers that table.
   - (b) Ship no system library, and on desktop require a user-installed xschem library via `XSCHEM_LIBRARY_PATH`. This has no legal exposure, but PDK examples break on iPadOS and on any host without xschem.
   - (c) Offer xschem's GPL library as a separately downloaded, unmodified data pack, and argue mere aggregation. This needs counsel, and it conflicts with the spirit of P-9.
4. **Q-SCHEM-4: Netlist formats beyond SPICE (scope and cost).**
   - (a) **Recommended.** SPICE only in M8. Keep every `G`/`V`/`F`/`E` record and `*_format` attribute through round trips. Revisit Verilog when P-17 (digital flow, desktop) needs mixed-signal top levels.
   - (b) SPICE plus Verilog in M8, for mixed-signal flows with LibreLane. This costs about one extra netlister capability and its golden cases.
   - (c) SPICE plus Spectre. No open-source simulator in Hefesto's flow consumes Spectre, so this is not recommended.
5. **Q-SCHEM-25: Script permission model (security).** xschem's default is to ask before running `tcleval`, and the sky130 `xschemrc` sets it to `yes`.
   - (a) **Recommended.** As specified: always evaluate `tcleval` attribute values in the X-3 sandbox (no process, socket or native-extension access, and no file access outside granted folders), and confirm launcher `tclcommand` scripts once per document. Ignore `xschem_execute_scripts` and say so in the log. This needs a `hefesto-security` review sign-off before the M8 exit.
   - (b) Replicate xschem's `ask`/`yes`/`no` setting (default `ask`), applied on top of the sandbox. This is safest for users, but every PDK testbench triggers prompts, and on iPad the prompts block netlisting.
   - (c) Option (a), plus a per-PDK trust flag in the PDK manager, so that launchers from a trusted PDK need no confirmation.

## Decision table

Every row is approved (2026-11-13); the Recommendation column is the decision recorded in the entry's `Resolution:` line. **Src** names the sources a row used. **Caps** names the capabilities the row affects: `sff` is schematic-file-format, `ssl` schematic-symbols-and-libraries, `sed` schematic-editing, `snl` schematic-netlisting, `ssi` schematic-simulation-integration and `scmd` schematic-command. Rows marked *(resolved)* already have a `Resolution:` line and are listed only for completeness.

### Cross-cutting

| ID | Question | Kind | Recommendation | Alternatives | Rationale | Caps | Src |
|---|---|---|---|---|---|---|---|
| D-21 | `xschem-compat` setting | **other** (judgement J-2) | add `xschem-compat`, default ON, owned by `hefesto-magic-compat-mode`; it governs `SCHEM.md` entries with `Resolution: compat`; it changes no layout, LVS or netgen output; stored per project; recorded in the netlist (`** xschem-compat=off`) only when OFF, so default netlists stay byte-identical | no setting (decide each quirk once; "replicate always" is a special case); reuse `magic-compat` (today's default reading of the settings table) | same model as `magic-compat` and `netgen-compat` (D-2); lets users get valid SPICE without losing byte identity by default; reusing `magic-compat` would couple layout and schematic fidelity | hefesto-magic-compat-mode, snl, sff, hefesto-settings | [D][S] |

### File format

| ID | Question | Kind | Recommendation | Alternatives | Rationale | Caps | Src |
|---|---|---|---|---|---|---|---|
| Q-SCHEM-1 | Version string written by Hefesto | **replicate** | write `xschem version=3.4.8RC file_version=1.3`, a pinned compatibility version, bumped only with a re-validation | echo the version that was read; write `hefesto-<v>` | the manual says the `v` record is first and carries both keys [M]; the entry says xschem ignores the `version` key [E] (the manual does not say which key is checked); a fixed value keeps diffs stable across Hefesto releases | sff, scmd (`xschem version`) | [E][S][M] |
| Q-SCHEM-2 | Embedded symbol block quirks (no `F`, polygons after texts, `L 1` wires) | **replicate** | write `[ ... ]` blocks as xschem does, in both modes | fix (write like a `.sym`) | only layout inside the block changes, and the block reads back the same either way; byte identity keeps VCS diffs clean; a compat toggle would add a mode with no user benefit | sff | [E][S][M][G] SCHEM-RT-05 (input to be replaced, see "Blocked on capture") |
| Q-SCHEM-3 | File version unknown during the first record | **compat** | ON: the first record sees no version, so a leading `C` gets no `.sym`; OFF: a file with no `v` is version `1.0` from the first record, so every reference gets `.sym` | replicate in both modes | changes reference content (which symbol loads), so the standing rule makes it compat; the OFF behaviour is the obvious intent of the `1.0` default; only hand-written or very old files are affected | sff | [E][S][G] SCHEM-RT-08 |
| Q-SCHEM-29 | Netlisting rewrites wire `lab` attributes | **replicate** | write computed `lab` values on the next save, as the spec says, without marking the document modified | keep computed names out of the file (compat OFF) | the manual documents that "the lab attribute is set by xschem when creating a netlist", so xschem-saved PDK files already carry them, and the round-trip identity of RT-02 depends on it; a compat variant would make OFF files differ from every xschem-saved file | sff, snl | [E][S][M] |

### Symbols, libraries and scripting

| ID | Question | Kind | Recommendation | Alternatives | Rationale | Caps | Src |
|---|---|---|---|---|---|---|---|
| Q-SCHEM-4 | Verilog, VHDL, Spectre and tEDAx netlisters | **other** (judgement J-4) | SPICE only in M8; keep all `G`/`V`/`F`/`E` records and `*_format` attributes through round trips; revisit Verilog with P-17 | add Verilog; add Spectre | P-12 scope is SPICE for simulation and LVS; the manual lists four netlisting modes, but no Hefesto flow consumes the others yet | ssl, snl, sff | [E][S][M][D] |
| Q-SCHEM-5 | Pin sort by `sim_pinnumber` is unstable | **fix** | stable sort (file order on ties) in both modes; warn on duplicate `sim_pinnumber` | replicate the C library's order | xschem's order on ties depends on the C library, so there is nothing portable to match (the X-1 rule for platform-dependent sorts); duplicates are a user error | ssl, snl | [E][S][G] SCHEM-PR-03 |
| Q-SCHEM-6 | Symbol `drc` scripts *(resolved)* | **platform** | run in the sandboxed Tcl (X-3, 2026-11-07) | none | already resolved | ssl, hefesto-scripting | [E] |
| Q-SCHEM-7 | Tcl dependence of PDK symbols *(resolved)* | **platform** | embedded sandboxed Tcl on every platform, iPadOS included (X-3, 2026-11-07) | none | already resolved | ssl, hefesto-scripting | [E] |
| Q-SCHEM-8 | Tcl procedures and variables required by PDK libraries | **other** | inventory script over the three PDK trees (every `tcleval(`, `[`-command and `$::` variable); implement exactly that list (`ev`, `ev7`, `ev0`, `to_eng`, `from_eng`, `ngspice::*`, `xschem` subset); specify each procedure's semantics from the public manual and from expected values computed for the PDK symbols, never from xschem's Tcl; keep the inventory as a test that fails when a PDK bump adds a procedure | broader xschem Tcl API | the smallest clean-room surface; the inventory doubles as a regression guard | ssl, hefesto-scripting, scmd | [E][S][D] |
| Q-SCHEM-9 | Web (`http`) symbol references | **platform** | not supported in v1: keep the reference as is, show a missing-symbol placeholder, warn | fetch through the platform network service | no PDK uses it; network fetches at load time are a security and offline risk (X-4) | ssl | [E][S] |
| Q-SCHEM-10 | Clean-room system library and `xschem` command inventory | **other** (judgement J-3) | clean-room re-draw of the referenced `devices/` symbols, specified by name, pins and properties; graphics drawn independently; symbol list and `xschem` subcommand list generated by inventory scripts over the Apache-2.0 PDK trees; unsupported subcommands raise the error the spec defines | require a user-installed xschem library; ship the GPL library as a separate pack | P-9 licensing; PDK examples must work unchanged on every platform | ssl, scmd, hefesto-licensing | [E][S][D] |
| Q-SCHEM-11 | Symbol generators *(resolved)* | **platform** | Tcl-only generators in the sandbox (X-3, X-4, 2026-11-07) | none | already resolved | ssl | [E] |
| Q-SCHEM-23 | `xschemrc` evaluation on iPadOS *(resolved)* | **platform** | sandboxed Tcl with synthetic `env()` (X-3, X-4, 2026-11-07) | none | already resolved | ssl, hefesto-files-and-pdks | [E] |
| Q-SCHEM-25 | Script permission model | **platform** (judgement J-5) | as specified: `tcleval` always runs in the sandbox, launchers are confirmed once per document, `xschem_execute_scripts` is ignored with a log line; needs `hefesto-security` review sign-off before the M8 exit | replicate `ask`/`yes`/`no`; add a per-PDK trust flag | the X-3 sandbox already removes the risks the xschem setting guards against; prompts would block iPad netlisting | ssl, hefesto-security, hefesto-scripting | [E][S][D] |

### Editing

| ID | Question | Kind | Recommendation | Alternatives | Rationale | Caps | Src |
|---|---|---|---|---|---|---|---|
| Q-SCHEM-24 | Touch editing details | **platform** | both the gesture set of `schematic-editing` and a floating tool palette; the gestures stay provisional until a usability test of finger vs Pencil wire drawing; desktop keeps xschem-style default bindings, rebindable | gestures only; palette only | follows X-5; iPad ships in M14, so the test can run before the iPad scope freezes without blocking M8 desktop work | sed, hefesto-input-and-touch | [E][S][D] |

### Netlisting

| ID | Question | Kind | Recommendation | Alternatives | Rationale | Caps | Src |
|---|---|---|---|---|---|---|---|
| Q-SCHEM-12 | `expr()` number lexing swallows `-` | **compat** | ON: `-` belongs to the number token (`expr(2-1)` = 2); OFF: a number stops at `-` unless the `-` follows `e`/`E`; both modes warn wherever the two lexings differ (a digit-started run with an inner `-` not after `e`/`E`) | replicate only (the entry's original recommendation) | changes netlist values, so compat under the standing rule; PDK templates use spaces, so PDK netlists are identical in both modes | snl | [E][S] |
| Q-SCHEM-13 | Absolute paths in netlist comments | **platform** | default: write host absolute paths as xschem does; a per-project "portable netlist paths" setting writes `$PDK_ROOT/...`- or document-relative paths; golden comparison normalises these lines (already in CASES.md) | always relative; omit the lines | comments only; byte identity on desktop by default; iPad sandbox paths are long and leak the container layout, so the portable option is useful there and for VCS | snl, hefesto-settings | [E][S][G] |
| Q-SCHEM-14 | Two subcircuits with the same cell name | **compat** | ON: one `.subckt`, from the first one found; OFF: the second gets a unique name (`inv_1`, ...) and its instance lines use it; both modes warn | replicate only; error | silently wrong netlists are the worst LVS failure mode; OFF gives a correct netlist without breaking default identity; the warning is in both modes | snl | [E][S] |
| Q-SCHEM-15 | Flat netlist text | **replicate (behavioural)** | flat mode is electrically equivalent to xschem's (same devices, nets and values); compare by a canonical parsed form, not bytes; no exact reproduction of xschem's flattening text rules | exact reproduction; drop flat mode | PDK flows use hierarchical netlists; exact text would mean reproducing a GPL awk program's corner cases, the highest clean-room risk in M8 for little value | snl | [E][S] |
| Q-SCHEM-16 | Top-level shorts not always reported | **fix** | always report shorts, including `-nohier` runs | replicate | diagnostic only; the netlist text is unchanged, and a missed short is a missed error | snl | [E][S] |
| Q-SCHEM-17 | Simulation preparation accepts schematic decks *(resolved)* | **fix** | done (2026-11-06): `schematic` source added to `simulation-netlist-preparation` | none | already resolved | ssi, simulation-netlist-preparation | [E] |
| Q-SCHEM-18 | awk dialect in post-processing | **replicate** | gawk semantics (case-insensitive `.param` and `value=` matching) on every platform | mawk/BWK semantics | X-1 rule: one deterministic behaviour on all platforms, the one PDK users mostly get; golden captures need gawk installed as `awk` (it is not installed on this Mac) | snl | [E][S][G][D] |
| Q-SCHEM-19 | `extra` exclusion uses substring match | **compat** | ON: drop a template key that occurs anywhere in `extra`; OFF: drop only whole words of `extra`; both modes warn when a dropped key is not a whole word | replicate only | changes `.subckt` parameter lists, so compat; the warning catches the `VPW`/`VPWR` case | snl | [E][S] |
| Q-SCHEM-20 | Line breaker quote state | **replicate** | carry the quote state across lines and reset the column counter as xschem does, in both modes | fix | changes only where long lines wrap; SPICE meaning is unchanged; covered by golden case SCHEM-NL-12 | snl | [E][S][G] |
| Q-SCHEM-21 | Multi-line templates break `.subckt` lines | **compat** | ON: copy the template LFs onto the header; OFF: replace LFs by spaces; both modes warn; the inventory checks whether any PDK subcircuit symbol has a multi-line template | replicate and warn only | xschem's output is invalid SPICE, so OFF must be able to produce a valid netlist; if the inventory finds no PDK case, both modes are identical on PDK data | snl | [E][S] |
| Q-SCHEM-22 | Trailing `m=1` removed everywhere | **compat** | ON: remove a trailing ` m=1` from every line; OFF: from device lines only, keeping comments, `.param` and `.control` text | replicate only (the entry's original recommendation) | on user code and `.param` lines the removal changes netlist content, so the standing rule makes it compat; on device lines `m=1` is the default and removing it changes nothing | snl | [E][S] |
| Q-SCHEM-26 | `@` tokens end only at whitespace | **replicate** | token ends as xschem's do, in both modes; warn when a token lookup fails but a shorter identifier prefix exists (`@W/` → `@W`) | fix (stop at non-identifier characters) | a fix would change PDK netlists that rely on tokens such as `@spice_get_voltage(net)`, even with compat OFF; the warning gives users the fix | ssl, snl | [E][S][G] SCHEM-PR-04 |
| Q-SCHEM-27 | Missing subcircuit schematic | **replicate** | write the header and an empty body and warn, as the spec says; the exact text is fixed by golden case SCHEM-NL-14 | always an error | keeps netlists of PDK primitives with absent schematics usable | snl | [E][S][G] |
| Q-SCHEM-28 | Instance-based schematic variants (`schematic=`) | **replicate** | one extra subcircuit per distinct `schematic=`, named after that schematic's cell name; exact naming fixed by golden case SCHEM-NL-15 | none credible | needed for netlist identity; the remaining uncertainty is text, not behaviour | snl | [E][S][G] |

## Counts

| Kind | Count | IDs |
|---|---|---|
| replicate | 9 | SCHEM-1, SCHEM-2, SCHEM-15, SCHEM-18, SCHEM-20, SCHEM-26, SCHEM-27, SCHEM-28, SCHEM-29 |
| compat | 6 | SCHEM-3, SCHEM-12, SCHEM-14, SCHEM-19, SCHEM-21, SCHEM-22 |
| fix | 3 | SCHEM-5, SCHEM-16, SCHEM-17 (resolved) |
| platform | 8 | SCHEM-6, SCHEM-7, SCHEM-11, SCHEM-23 (all resolved), SCHEM-9, SCHEM-13, SCHEM-24, SCHEM-25 |
| other | 4 | SCHEM-4, SCHEM-8, SCHEM-10, D-21 |

Total: 30 rows, of which 25 are open (24 Q-SCHEM plus D-21) and 5 are already resolved. IDs are given without the `Q-` prefix. Kinds follow D-21 (a). If D-21 takes (b), the six compat rows become replicate, except Q-SCHEM-14 and Q-SCHEM-21, which become fix (warn and produce valid SPICE).

## Changes to the entries' original recommendations

- **Q-SCHEM-3:** compat (was replicate). It changes which symbol a reference loads.
- **Q-SCHEM-12, -19:** compat (was replicate and warn). The warning stays in both modes.
- **Q-SCHEM-14:** compat (was replicate and warn). OFF gives a correct netlist.
- **Q-SCHEM-21:** compat (was replicate and warn). OFF gives valid SPICE.
- **Q-SCHEM-22:** compat (was replicate). It changes `.param` and `.control` text.
- **Q-SCHEM-26:** adds a lookup-prefix warning.
- **Q-SCHEM-8:** adds the rule that the semantics are specified from public docs and PDK values, never from xschem's Tcl.
- **Q-SCHEM-13:** the portable-paths option becomes a per-project setting.
- **Q-SCHEM-25:** adds `hefesto-security` sign-off as an M8 exit condition.

## Blocked on capture

The golden captures in `tests/golden/SCHEM/CASES.md` need xschem `3.4.8RC` and gawk, and neither is installed. The capture runner is the only place where running xschem is allowed. Its outputs are black-box behaviour, which is permitted under J-1. Rows Q-SCHEM-14, -21, -22, -27 and -28 take their exact texts from those captures. Q-SCHEM-15 is behavioural, so its captures only check electrical equivalence and fix no text. None of these rows' kind or recommendation depends on the captures.

One capture input conflicts with J-1 and P-9: SCHEM-RT-05 uses xschem's own `xschem_library/pcb/pcb_test1_embed.sch`, which is GPL. Committing it or its saved output would put GPL data in the repository. Replace it with a Hefesto-authored schematic that embeds a symbol, before capture. Done with the spec alignment: `tests/golden/SCHEM/CASES.md` SCHEM-RT-05 now names a Hefesto-authored input. Applying the approved J-1 rule "golden inputs Hefesto-authored only" to the whole inventory, two more cases used xschem's own files and were changed the same way: SCHEM-NL-01 (xschem `tests/SC_Test.sch`) and SCHEM-NL-19 (the `bus_tap` example of `xschem_library/examples`). The Apache-2.0 PDK example schematics stay as inputs: they are not xschem's, and the roadmap's M8 exit criterion requires their round trips.

## Spec changes after approval

Applied to `openspec/specs/*/spec.md` after the 2026-11-13 approval ("Align the M8 specs with the approved decisions"), with `hefesto-settings` and `hefesto-security` added for the per-project settings and the M8 sign-off. The implementation plan is OpenSpec change `m8-schematic`.

- **hefesto-magic-compat-mode:**
  - Add Requirement "xschem-compat setting for xschem quirks", with a settings-table row (`xschem-compat`, xschem, `SCHEM.md`, ON).
  - Add a scenario: with `xschem-compat` OFF, layout, extraction and LVS output are unchanged.
  - State that `xschem-compat` is written into a netlist only when OFF (`** xschem-compat=off`).
  - Add `docs/decisions/open-questions/SCHEM.md` to the files excluded from `magic-compat` in the settings table.
  - Replace the D-21 sentence.
- **cross-cutting.md X-2:** replace "Whether xschem gets an `xschem-compat` setting is open (D-21)" with the decision.
- **hefesto-licensing:**
  - Add a scenario for the two-team rule (J-1).
  - Add the counsel-review item for the `devices/` interface (J-3).
- **schematic-symbols-and-libraries** spec.md:330 (Requirement "System library"): say that the interface table is extracted under the J-1 process and covered by the J-3 counsel sign-off.
- **schematic-file-format:** in Requirement "Version record and header text" (spec.md:150), give both modes for the first-record rule (Q-SCHEM-3).
- **schematic-symbols-and-libraries:**
  - Pin order (spec.md:130): add the duplicate-`sim_pinnumber` warning.
  - spec.md:269: the `http` reference shows a placeholder and warns.
  - spec.md:255: record the security sign-off.
  - spec.md:238: the Tcl procedure list is fixed by the inventory test (Q-SCHEM-8).
- **schematic-netlisting:**
  - spec.md:284: both modes plus the warning (Q-SCHEM-12).
  - spec.md:318: both modes for step 2 (Q-SCHEM-22).
  - spec.md:369: the portable-paths setting (Q-SCHEM-13).
  - spec.md:423: both modes and the warnings (Q-SCHEM-19, -21).
  - spec.md:441: both modes plus a scenario for OFF renaming (Q-SCHEM-14).
  - spec.md:105: always report shorts (Q-SCHEM-16).
  - spec.md:399: stable sort and the duplicate warning for top-level port order (Q-SCHEM-5).
  - spec.md:229: the prefix warning (Q-SCHEM-26).
  - spec.md:492: state that flat mode is compared behaviourally, and drop any byte-level wording (Q-SCHEM-15).
- **schematic-editing:** at spec.md:293, keep "provisional" and add the usability-test gate for M14.
- **schematic-command:** no change beyond the Q-SCHEM-10 inventory test.
- **tests/golden/SCHEM/CASES.md:** replace the SCHEM-RT-05 input with a Hefesto-authored embedded-symbol schematic (J-1).
- **docs/glossary.md:** update the `xschem-compat` row from "proposed" to the decision.
