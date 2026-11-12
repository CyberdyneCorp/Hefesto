# M4 decision table: the 34 open SCRIPT questions

M4 ("Scripting and PDK integration", docs/roadmap.md) is blocked by 34 entries in `docs/decisions/open-questions/SCRIPT.md` that have no `Resolution:` line (`python3 scripts/check_milestone_readiness.py --milestone M4`). Each recommendation below was checked against Magic 8.3.694 (fc5daa9e, `build/ref/magic`) and, for PDK entries, against open_pdks @ c6df28c1 and IHP-Open-PDK @ 5e6d592e (fetched read-only) and the installed PDKs in `.cache/pdks/ciel`. The full reasoning is in each entry's `Recommendation:` line. **All 34 were approved on 2026-11-11**: the five judgement items below with the option the user chose (each the recommended one), the other 29 as drafted. Each entry now carries a `Resolution:` line, and `python3 scripts/check_milestone_readiness.py --milestone M4` reports `M4: ready`.

Recommendation kinds (X-2, docs/decisions/cross-cutting.md):

- **replicate**: do what Magic (or the PDK code) does, in both modes.
- **compat**: replicate with `magic-compat` ON and use the corrected behavior with it OFF (tier 2).
- **fix**: corrected behavior in both modes (tier 1, or tier 3 when there is no Magic output to match).
- **platform**: a Hefesto platform decision (X-3/X-4).
- **other**: none of the above.

## Judgement items (decided 2026-11-11)

These five involve product or security trade-offs. Each lists the options, with the recommended one first, and the user's decision.

1. **Q-SCRIPT-42 and Q-SCRIPT-34: PDK menu code that writes to Tk widgets (same as D-32).** The sky130, gf180mcu and sg13g2 `addtechmenu` callbacks write to `${frame}.titlebar.mbuttons.*` menus, call `tk_getOpenFile`, and probe `titlebar cget`. Under X-3 they run on iPadOS too.
   - (b) **Recommended.** Intercept the few known operations and map them to native menu actions in both shells, and report the statements that were skipped. This is little work and the PDK menus appear, but a PDK that adds a new widget call silently loses that item until mapped.
   - (a) A Tk-compatible widget shim with Magic's path names. This is the most compatible option, but a pseudo-Tk layer has to be maintained on two shells.
   - (c) Skip the callbacks and report them. This is the simplest option, but the PDK "Devices" menus and "Import SPICE" are missing.
   - **Decision (2026-11-11): (b)**, in both shells (desktop and iPad), with skipped statements reported. Settles D-32.
2. **Q-SCRIPT-46: keep a declarative (non-script) PDK profile? (same as D-30).**
   - (b) **Recommended.** Drop it. iPadOS curated-pack mode refuses PDKs without bundled scripts.
   - (a) Keep it for uncurated PDKs, but only if App Review rejects imported scripts.
   - (c) Also use it when the user declines script consent. This contradicts `hefesto-security`, which opens the file read-only with the `minimum` tech.

   The choice depends on the outcome of the App Review inquiry (X-3 risk).
   - **Decision (2026-11-11): (b).** Settles D-30.
3. **Q-SCRIPT-9: which PDK revision is the reference.** The unit tests use open_pdks `0fe599b2` (2024-08) and IHP `ddb601a4` (2025-01), set in `scripts/fetch_pdk.py`. The spec and the SCRIPT golden cases pin open_pdks `c6df28c1` (2026-11) and IHP `5e6d592e` (2026-09). The rc files differ between the two generations:
   - the older ones read `MAGTYPE` and call `query_mylib_ip`/`query_my_projects`;
   - gf180mcuD lists 5 libraries instead of 9;
   - the older IHP file does not source its generator and sets `SUB` to `VSUB`.

   Options:
   - (a) **Recommended.** Move `fetch_pdk.py` to the spec pins for M4, re-run the M1–M3 PDK-based tests on them, and add one smoke case for the older rc form.
   - (b) Keep the old pins and re-derive the spec's PDK contract from them. The spec would then describe PDKs that are two years old.
   - (c) Keep both pins permanently. Both generations stay covered, but at twice the capture and CI cost.
   - **Decision (2026-11-11): (a).** Any M1–M3 result that changes on the new pins is fixed or recorded through an approved decision.
4. **Q-SCRIPT-41: scope of the Tcl interpreter.** PDKs define different `magic::i2u`/`u2i`/`spice2float` with the same names, so loading a second PDK silently changes the first PDK's generated geometry.
   - (a) **Recommended if Hefesto can hold several projects in one process.** One interpreter per open project, with Tcl semantics unchanged inside each and a warning on cross-PDK redefinition.
   - (b) One interpreter per process, as in Magic, with the warning, and document "one PDK per session".
   - (c) Per-PDK namespaces for the helpers. This is rejected because it deviates from Tcl semantics and breaks unmodified PDK scripts.
   - **Decision (2026-11-11): (a).** One interpreter per open project, Tcl semantics unchanged inside each, a warning on cross-PDK redefinition of `magic::` helpers.
5. **Q-SCRIPT-11: `./<tech>_toolbars.tcl` in the working directory (security).**
   - (a) **Recommended.** Parse it as data: only `set <name> {layers}` lines are read, and anything else is reported and ignored.
   - (b) `source` it inside the X-3 sandbox, as Magic does. This is compatible with hand-written files that contain other Tcl, but a file in any opened folder then runs code without the consent prompt that PDK scripts get.
   - **Decision (2026-11-11): (a).**

## Decision table

Every row is approved (2026-11-11); the Decision column is the `Resolution:` recorded in SCRIPT.md. Confidence: **high** means the evidence decided the answer. **user decision** means one of the judgement items above.

### Tag callbacks and substitution

| ID | Question | Decision | Why | Confidence |
|---|---|---|---|---|
| Q-SCRIPT-3 | Tag `%`-substitution: results and arguments are inserted unescaped, the string is rescanned, and an unknown `%x` or `%%` makes Magic hang | **fix**: single pass, each value inserted as one quoted word | The hang is crash-class (tier 1). Well-formed values give the same words as in Magic; only text that Magic would evaluate changes. | high |
| Q-SCRIPT-4 | Output escaping: `$name` prints the variable's value when such a variable exists | **fix**: always print literally | Text only, like the approved Q-SCRIPT-15/16/17. Golden stdout cases with `$names` get a deviation entry. Compat is the alternative if stdout parity matters more. | high |
| Q-SCRIPT-5 | `$VAR` in search paths is cached at its first value for the whole session | **compat**: cache with it ON, re-resolve with it OFF; warn in both modes | It decides which cell file a later `use` reads (tier 2). PDK rc files set their variables once, so PDK flows are the same in both modes. | high |
| Q-SCRIPT-13 | The per-window wrapper callback key contains a space, so `Winopts(.f,callback)` never runs | **fix**: also run the no-space key; keep the callback scope | GUI hook with no output. PDK callbacks need `$framename` in scope. | high |

### Sandbox and security

| ID | Question | Decision | Why | Confidence |
|---|---|---|---|---|
| Q-SCRIPT-11 | The toolbar code `source`s `./<tech>_toolbars.tcl` from the working directory | **fix**: parse it as data (option a) | Running code from any opened folder without consent goes against X-3 and `hefesto-security`. Files the dialog writes round-trip. | user decision (item 5) |
| Q-SCRIPT-46 | Keep a declarative PDK profile under X-3? | **platform**: drop it (b); curated-pack mode refuses PDKs without bundled scripts | Every platform runs Tcl, and a declined consent is already handled by `hefesto-security`. | user decision (item 2) |

### PDK scripts and menus

| ID | Question | Decision | Why | Confidence |
|---|---|---|---|---|
| Q-SCRIPT-9 | Is the PDK script inventory verified, and against which PDK revision? | **other**: accept the static inventory, move the dynamic trace to golden capture (X-6); (a) move `fetch_pdk.py` to the spec pins, re-run M1–M3 PDK tests, one smoke case for the older rc form | The spec pins and the test pins differ, and the rc files differ between them. | user decision (item 3) |
| Q-SCRIPT-33 | The startup call `techmanager initall` is not a valid option, so it does nothing | **replicate** (no-op) | No effect in Magic. Hefesto's technology manager is native. | high |
| Q-SCRIPT-37 | Command-line regex quirks (`-rcover`, `^-n*` takes every unknown `-x`, `-dnullx`) | **replicate** | It only decides which arguments are options or files, and launchers and PDK flows depend on these spellings. | high |
| Q-SCRIPT-39 | sg13g2 calls `label … +m2`, which Magic rejects, so no label is made | **replicate**, and report upstream to IHP | PDK code runs unchanged, and native generators must produce identical cells. | high |
| Q-SCRIPT-40 | `magic::spice2float` mishandles `n`, `a` and the `*F` suffixes | **replicate** bit for bit, and report upstream | PDK code. X-3 validates native generators against the Tcl output. | high |
| Q-SCRIPT-41 | Two PDKs overwrite each other's `magic::` helpers | **replicate** within an interpreter and warn on cross-PDK redefinition; (a) one interpreter per open project | Tcl semantics must hold for unmodified PDK scripts. | user decision (item 4) |
| Q-SCRIPT-42 | PDK `addtechmenu` writes to Tk widget paths | **platform**: (b) intercept and map in both shells (desktop and iPad), and report what was skipped | Consistent with X-3 and D-32. The earlier "desktop only" predates X-3. | user decision (item 1) |
| Q-SCRIPT-44 | rc files seed `random` from the clock, so GDS prefixes vary between runs | **replicate**; the golden harness runs `random seed 1` | Session state, not a quirk. A seed-pinning setting belongs with Q-MASK-211 (M5). | high |

### PCells

| ID | Question | Decision | Why | Confidence |
|---|---|---|---|---|
| Q-SCRIPT-21 | `inf`/`nan` parameter values abort cell creation | **fix**: hash them as text | Magic makes no cell, so there is nothing to match. | high |
| Q-SCRIPT-22 | The cell-name hash uses only the low 8 bits of non-ASCII characters | **replicate** | Names must match cells that Magic already generated. | high |
| Q-SCRIPT-23 | Changing an existing fixed (`nocell`) device reads undefined `$delx`/`$dely` | **fix**: compute the counts and pitch as the dialog does | Magic raises an error and changes nothing. Both the dialog and the scripted call fail. | high |
| Q-SCRIPT-25 | `add_dependency` without parameter names raises an error | **fix**: call `_defaults` | Always an error in Magic, and no script uses that form. | high |
| Q-SCRIPT-29 | `gencell_change` needs an open dialog, so headless changes fail | **fix**: keep the instance name when there is no dialog | Magic raises an error with redraw left suspended. Headless flows need this. | high |
| Q-SCRIPT-38 | The framework adds `pitchx`/`pitchy`, while the PDKs use `deltax`/`xstep` | **replicate**: there is no defect | Corrected: each PDK's `fixed_dialog` converts `pitchx` to `deltax`, so spacing is kept. | high |
| Q-SCRIPT-45 | sg13g2 generators `load` other cells in the middle of a gencell draw | **replicate**; confirm with SCRIPT-PDK-14 | PDK code. The edit stack is safe (derived). A fixed device placed while editing a subcell goes to the root cell. | high |

### readspice

| ID | Question | Decision | Why | Confidence |
|---|---|---|---|---|
| Q-SCRIPT-19 | `readspice` uses the Tcl `case` command, which Tcl 9 removed | **fix**: use `switch` | Same result as Tcl 8, and it runs on any Tcl version. | high |
| Q-SCRIPT-32 | `+` continuation spacing, and "port order was modified" always printed | **compat** for the spacing (it can join two tokens) and **fix** for the message | A joined token changes pin matching (tier 2). The message is text only and always printed. | high |

### GUI managers

| ID | Question | Decision | Why | Confidence |
|---|---|---|---|---|
| Q-SCRIPT-26 | `drc_save_report` writes a raw count list and coordinates that are wrong outside internal units | **fix** the units, keep the file format | Byte-identical in Magic's only correct case, as in Q-SCRIPT-16/17. | high |
| Q-SCRIPT-27 | The library manager rebuilds every node on each refresh, leaks files, and `subst`s paths | **fix** (native manager) | GUI only. `subst` on a path evaluates code. | high |
| Q-SCRIPT-28 | The reorder dialog's Load always fails, names the wrong file, and uses `exec cat` | **fix**, keep the file format | Magic's Load never works, and `exec` is denied by the sandbox. | high |
| Q-SCRIPT-31 | The text helper passes `default` as the label position | **replicate**: there is no defect | Corrected: `label t default` reads `default` as the default font, which gives the intended label. | high |
| Q-SCRIPT-34 | Tk path names of the GUI managers on native shells | **platform**: native hooks and dialog API, no general Tk paths | Under X-3, PDK scripts also run on iPadOS. The widget calls themselves are handled by Q-SCRIPT-42/D-32. | user decision (item 1) |
| Q-SCRIPT-36 | Edit > "Rotate 90 degree" calls the non-existent `magic::clock` | **fix**: `magic::clockwise 90` | Always fails in Magic. Same effect as `rotate 90`. | high |

### Misc (command API)

| ID | Question | Decision | Why | Confidence |
|---|---|---|---|---|
| Q-SCRIPT-6 | `port last` counts non-port and removed-port labels | **compat** | It seeds new port indices written to `.mag` (tier 2). `readspice` is affected only in the removed-port case. | high |
| Q-SCRIPT-7 | `cif scale` returns a float widened to double (0.009999999776…) | **compat** | Visible to scripts (tier 2). The PDK helpers round it, so PDK cells are the same in both modes. | high |
| Q-SCRIPT-8 | Order of `drc listall why` and `drc.out` | **replicate** Magic's string-hash order | Corrected: the order is deterministic (string-keyed table, X-1/D-1), which gives byte-identical `drc.out`. | high |
| Q-SCRIPT-14 | A keyword ending in a tab never matches exactly | **compat** | Affects only `get` and `id`, which are always ambiguous in Magic (tree-wide scan). With compat OFF they work. | high |
| Q-SCRIPT-30 | `mazeroute`: `.net` fallback and chained multi-terminal routing | **replicate** | It is the script's algorithm, and it drives `iroute` (M10). | high |

## Counts

| Kind | Count | IDs |
|---|---|---|
| replicate | 12 | 8, 22, 30, 31, 33, 37, 38, 39, 40, 41, 44, 45 |
| fix | 13 | 3, 4, 11, 13, 19, 21, 23, 25, 26, 27, 28, 29, 36 |
| compat | 5 | 5, 6, 7, 14, 32 (spacing; its message part is a fix) |
| platform | 3 | 34, 42, 46 |
| other | 1 | 9 |

## Entries that do not need to block M4

- **Q-SCRIPT-30**: `mazeroute.tcl` drives `iroute`, which belongs to `router-maze-iroute` (M10). It can be resolved with M10.
- **Q-SCRIPT-44**: the seed matters for output only through the GDS full-dump prefix (`gds-write`, M5), where Q-MASK-211 is still open. The harness rule already exists.
- **Q-SCRIPT-45**: affects only the native sg13g2 generators. The Tcl path runs the PDK code unchanged. It needs the SCRIPT-PDK-14 capture, not a decision.
- **Q-SCRIPT-31 and Q-SCRIPT-38**: the reported defect does not exist (corrected). They can be closed as `n/a` once the specs' wording is fixed.
- **Q-SCRIPT-27, -28, -33, -36**: these concern internals of Magic's Tk wrapper. Hefesto's managers and menus are native, so only the script-visible hooks and file formats matter, and the recommendations keep those.

## Also blocking M4 outside SCRIPT.md

`docs/decisions/audit/DECISIONS.md` lists seven items as blocking M4. `check_milestone_readiness.py` does not check them.

| Item | Subject | State (2026-11-11) |
|---|---|---|
| D-30 | declarative PDK profile | **resolved** by Q-SCRIPT-46, option (b) |
| D-32 | Tk menu writes by PDK scripts | **resolved** by Q-SCRIPT-42 and Q-SCRIPT-34, option (b) |
| D-29 | native vs Tcl PCell identity (byte-identical `.mag` and the `timestamp` line) | **resolved** 2026-11-11, as recommended |
| D-31 | `env(PDK_ROOT)` and other variables in the sandboxed Tcl | **resolved** 2026-11-11, as recommended |
| D-33 | `cd` in the Tcl sandbox | **resolved** 2026-11-11, as recommended |
| D-34 | the `irsim`, `netgen` and `xcircuit` helper commands | **resolved** 2026-11-11, as recommended |
| D-36 | `quit -noprompt` from a PDK rc script on iPadOS | **resolved** 2026-11-11, as recommended |

No Q-SCRIPT answer decides D-29, D-31, D-33, D-34 or D-36; the user approved them as recommended on 2026-11-11 (see DECISIONS.md and the M4 plan's "Resolved Questions").

## Corrections made to entries

- **Q-SCRIPT-3**: added that an unknown `%x` or a `%%` makes `TagCallback` loop forever, and that arguments are inserted raw.
- **Q-SCRIPT-5**: changed from fix to compat (the cache changes which file is read). Also corrected: names that do not resolve are not cached.
- **Q-SCRIPT-6**: changed from replicate to compat. The reason given ("readspice is tuned to it") was wrong. Added the removed-port case.
- **Q-SCRIPT-7**: changed from replicate to compat. The PDK helpers round the value, so PDK cells are unaffected.
- **Q-SCRIPT-8**: the order is not undefined but a deterministic string-hash order. Changed from sort to replicate under X-1/D-1.
- **Q-SCRIPT-9**: added that the test PDK pins (`fetch_pdk.py`) are older than the spec pins and differ in the rc contract.
- **Q-SCRIPT-14**: the cited example was wrong. The real cases are `get` and `id`, found by a tree-wide scan. Changed from fix to compat.
- **Q-SCRIPT-23**: both the dialog call and the scripted call fail, not only the non-interactive one.
- **Q-SCRIPT-26**: the wrong units come from `drc listall why`. Added that the units print flag makes the report fail.
- **Q-SCRIPT-31**: there is no bug, because `default` is read as the default font name.
- **Q-SCRIPT-32**: the tokens are not always equivalent, because a `+c` line ending in a space joins two tokens. The spacing became compat.
- **Q-SCRIPT-34 and Q-SCRIPT-42**: the "desktop only" recommendations predated X-3 and are aligned with D-32.
- **Q-SCRIPT-38**: the PDKs do read `pitchx`/`pitchy` (`fixed_dialog`), so spacing is not lost. Changed to replicate with no upstream report. The spec text in `pcell-toolkit` and `scripting-pdk-integration` needs correcting.
- **Q-SCRIPT-45**: added a static derivation (the edit stack is safe; fixed devices reset the edit cell to the root).

## Spec follow-ups (applied 2026-11-11)

All applied in "Align the M4 specs with the approved decisions":

- `scripting-api` "Result formats": `drc listall why` follows Magic's string-hash order (X-1, Q-SCRIPT-8); `drc-commands` "drc why" says the same.
- `pcell-toolkit` "Parameter dictionary model" and the fixed-device note in `scripting-pdk-integration`: the PDKs' `fixed_dialog` reads `pitchx`/`pitchy` (Q-SCRIPT-38).
- `cmd-labels-properties` "Label command": the font word `default` is the default font (Q-SCRIPT-31).
- `ui-gui-managers` "Text helper": the Q-SCRIPT-31 note no longer describes a defect.
- `readspice` "Line preprocessing": the token join, in both `magic-compat` settings (Q-SCRIPT-32).

## User decisions of 2026-11-12 (final round)

Taken on the captures of golden-capture runs 43151033297 and 43173319299; each entry's `Resolution:` line is in `open-questions/`.

- **Q-CELL-45** (new): `cellname list filepath` returns Magic's whole stored path with `magic-compat` ON (its `TCL_STATIC` result sees the `/` put back) and the directory with it OFF. **compat.**
- **Q-SCRIPT-47** (new): `readspice`'s `string map {[ < ] >]}` never matches `D[0]` to `D<0>` with `magic-compat` ON; OFF maps `]` to `>`. **compat.**
- **Q-SCRIPT-45** (changed from replicate): the native sg13g2 fixed devices reproduce Magic's recorded sequence (SCRIPT-PDK-14: the first device of a kind is not placed when there was no box, and the edit cell becomes the window's cell) with `magic-compat` ON, and put back the edit context with it OFF. **compat.**

## User decisions of 2026-11-12 (polish round)

Taken on the captures of golden-capture run 43234885440; each entry's `Resolution:` line is in `open-questions/`.

1. The usual rule (`magic-compat` ON copies Magic, OFF gives the intended behavior):
   - **Q-SCRIPT-48**: a no/yes confirmation that cannot be asked (`tech load` over painted layout, `save NAME` over another file, `flush`) goes ahead with ON, as TxDialog's -1 does in Magic, and is declined with OFF. **compat.** The `scripting-api` and `tech-command` specs no longer disagree.
   - **Q-CELL-46** (new): `cellname list childinst` in Magic's id-hash order with ON, placement order with OFF. **compat.**
   - **Q-CELL-47** (new): the `save` file-name answer cut to 7 characters (`sizeof` a pointer) with ON, the whole line with OFF. **compat.**
   - SCRIPT-PCELL-12 passes; SCRIPT-PDK-07 is committed with its Q-SCRIPT-41 deviation (cross-PDK redefinition warnings).
2. Accepted as recorded deviations or deferrals, with their reasons in `tests/golden/deviations.toml`:
   - **Q-DRC-26** (new): DRC-PDK-3 uses IHP's `cap_cmim` generator; IHP-Open-PDK has no `cap_cmomf` generator (task 12.3).
   - **Q-SCRIPT-52** (new): the `magic-compat` ON text of a refused native `load` (Tcl's dlopen text, SCRIPT-API-04).
   - **Q-SCRIPT-49** (new): the command inventories of SCRIPT-API-01 and SCRIPT-PDK-09 differ by design.
   - SCRIPT-API-20 deferred to M7 (`extract style`), with the Q-UI-12 macro set.
   - **Q-SCRIPT-50** (new): SCRIPT-GUI-05's technology menu order is not reproducible.
   - **Q-SCRIPT-51** (new): SCRIPT-GUI-12/-13 keep native crash backups and tool enabling.
3. The DRC throughput target is judged by callgrind instruction count, not wall time: one block pass at most the M3 baseline of 12.14 billion + 2 % (12.38 billion). By count Hefesto is 12.25 billion against Magic's 10.98 billion (1.12 ×). The timed ratio is kept as information. `perf.yml` `drc-profile` gates it with `bench/drc/instruction_gate.py`; see `docs/design/m3-ui-conformance.md` "Performance".
