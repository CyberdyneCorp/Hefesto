# Open decisions from the spec audit

This list consolidates every "Needs decision" item from the six group audits (`G1.md` … `G6.md`), the cross-group findings that could not be fixed without a product decision, and the items found during consolidation. Duplicates are merged. Each entry says which capabilities it affects, the question, the options, a recommendation, and a priority.

Priority is taken from the milestone of the earliest affected capability in `docs/roadmap.md`:

- **Blocks M0–M2:** an affected capability is in M0, M1 or M2, so the decision is needed before that work starts (roadmap principle 2).
- **Blocks a later milestone:** the first affected capability is in M3 or later. The milestone is named.
- **Cosmetic:** wording, naming or bookkeeping only. No behavior or output changes.

Entries are ordered by priority, then by milestone.

| Priority | Count | IDs |
|---|---|---|
| Blocks M0–M2 | 13 | D-1 … D-13 |
| Blocks a later milestone | 29 | D-14 … D-42 |
| Cosmetic | 4 | D-43 … D-46 |
| **Total** | **46** | |

---

## Blocks M0–M2

### D-1: Amend X-1 to copy Magic's hash order where it depends only on names and insertion history

- **Affects:** `hefesto-output-determinism` (M0), `ext-flattening-hierarchy`, `def-write`, `lef-write`, `netlist-spice-options`, `cell-file-format`, `cell-io-safety`. Open questions Q-LEFDEF-30, Q-LEFDEF-92, Q-NETLIST-2, Q-CELL-11, Q-CELL-28. Source: G3 Needs decision 1 and cross-group 6.
- **Question:** X-1 says hash-ordered records are written sorted by name. Four G3 specs instead copy Magic's string-keyed hash order exactly. In some places that order also decides results, such as the LEF layer-map winner and implicit port numbering, so a sort changes output that canonicalized comparison cannot hide. Which rule wins?
- **Options:**
  - (a) Amend X-1: where Magic's order depends only on string keys and insertion history, copy it exactly (byte-identical). Use the name/coordinate sort only for orders that depend on memory addresses or the platform, such as `.ext` cap terminal order, Q-NETLIST-1, Q-NETLIST-26 and Q-NETLIST-45. Move the hash definition, now duplicated in `def-write` "Table enumeration order" and `ext-flattening-hierarchy` "Container iteration order", into one owner.
  - (b) Keep X-1 as written. Rewrite the four specs to sort, and accept the semantic differences, recording them in `hefesto-magic-compat-mode`.
- **Recommendation:** (a). Make `hefesto-output-determinism` the owner of the "Magic string hash table" definition. Then decide whether `.mag` elements (Q-CELL-11) and backup order (Q-CELL-28, D-9) keep the sort, which is allowed under (a) because they never change results.
- **Priority:** blocks M0–M2. `hefesto-output-determinism` is M0, and `.mag` element order is M1.
- **Status:** resolved (2026-11-06): option a — X-1 amended: name-keyed Magic hash order is reproduced exactly (table defined once in `hefesto-output-determinism`); the sort stays only for address-keyed or platform-dependent order and for `.mag` elements and backup/archive cells, whose order never affects results.

### D-2: Q-LVS-28, a separate `netgen-compat` setting or reuse of `magic-compat`

- **Affects:** `hefesto-magic-compat-mode` (M0), `lvs-*` (M9). Open questions Q-LVS-7, Q-LVS-9, Q-LVS-20, Q-LVS-24, Q-LVS-28. Source: G4 Needs decision 1 and cross-group 3.
- **Question:** several netgen quirks are to be "replicated behind the compatibility switch", but no capability says which switch. Does `magic-compat` also govern netgen quirks, or is there a separate `netgen-compat` setting?
- **Options:** (a) reuse `magic-compat`; (b) add a `netgen-compat` setting, default ON, defined next to `magic-compat` in `hefesto-magic-compat-mode`.
- **Recommendation:** (b), as Q-LVS-28 recommends, so that LVS fidelity can be toggled without changing layout output. Generalize `hefesto-magic-compat-mode` to own the set of reference-tool compatibility settings. D-21 covers xschem.
- **Priority:** blocks M0–M2. `hefesto-magic-compat-mode` is M0 and defines the settings.
- **Status:** resolved (2026-11-06): option b — `netgen-compat` (default ON) added next to `magic-compat` in `hefesto-magic-compat-mode`, which now owns all reference-tool compatibility settings; Q-LVS-28 resolved, xschem still open in D-21.

### D-3: Allowed `Resolution:` values

- **Affects:** `hefesto-magic-compat-mode` (M0), decision X-2, every open-questions file. Source: G6 Needs decision 5.
- **Question:** X-2 lists `fix | compat | n/a`, but `hefesto-magic-compat-mode` also uses `platform`. Is `platform` allowed?
- **Options:** (a) add `platform` to X-2; (b) drop it and record platform answers as `n/a` with a pointer to the deciding item.
- **Recommendation:** (a). Platform answers are common (X-3, X-4, P-13). Also allow `decided by <ID>` as free text after the value, as the existing lines already do.
- **Priority:** blocks M0–M2.

- **Status:** resolved (2026-11-06). `platform` is allowed; X-2 in cross-cutting.md now lists `fix | compat | platform | n/a`.

### D-4: Strict-mode switch for script errors (Q-SCRIPT-2)

- **Affects:** `hefesto-diagnostics` (M0), `scripting-api` (M4), `plugin-api` (M13). Source: G6 Needs decision 4 and cross-group finding on `scripting-api`.
- **Question:** by default, Hefesto keeps Magic's convention that a failed command still returns Tcl OK. `hefesto-diagnostics` adds an opt-in strict mode that raises errors. What is that switch called, and what is its scope: per script, per plugin, or per project? `scripting-api` still says "in compatibility mode" without naming a switch.
- **Options:** (a) a project setting `script-strict` (default off); (b) a per-interpreter command, such as `hefesto::strict on|off`, that a script turns on for itself; (c) both, with the command overriding the setting for its interpreter. Script plugins already get Tcl errors on `HF_E_PERMISSION`.
- **Recommendation:** (c). Keep it separate from `magic-compat`, which governs output quirks, not error signalling. Rewrite `scripting-api` "in compatibility mode" as "by default", and name the switch in `hefesto-diagnostics`.
- **Priority:** blocks M0–M2. `hefesto-diagnostics` is M0.
- **Status:** resolved (2026-11-06): option c — project setting `script-strict` (default off) plus per-interpreter `hefesto::strict on|off` that overrides it, defined in `hefesto-diagnostics` and independent of `magic-compat`; `scripting-api` now says "by default".

### D-5: Missing `Resolution:` lines across the open-questions files

- **Affects:** all areas. Roadmap principle 2 requires a milestone's open questions to be resolved before its work starts. Source: G1, G4 Needs decision 3, G6 cross-group.
- **Question:** 818 of 834 open-question entries have no `Resolution:` line, so every requirement that cites them is provisional. Who resolves them, and in what order?
- **Counts** (entries / with `Resolution:` / without), after consolidation:

  | File | Entries | With | Without |
  |---|---|---|---|
  | AGENT | 6 | 0 | 6 |
  | CELL | 41 | 1 | 40 |
  | CMDEDIT | 38 | 1 | 37 |
  | DIGITAL | 10 | 0 | 10 |
  | DRC | 23 | 0 | 23 |
  | EXTRACT | 26 | 0 | 26 |
  | LEFDEF | 102 | 0 | 102 |
  | LEGACY | 60 | 1 | 59 |
  | LVS | 29 | 0 | 29 |
  | MASK | 139 | 0 | 139 |
  | MISC | 33 | 0 | 33 |
  | MODEL | 30 | 0 | 30 |
  | NETLIST | 48 | 0 | 48 (Q-NETLIST-38 has `Status: resolved` but no `Resolution:` line) |
  | OASIS | 34 | 0 | 34 |
  | PLUGIN | 8 | 0 | 8 |
  | RELIABILITY | 12 | 0 | 12 |
  | SCHEM | 29 | 1 | 28 |
  | SCRIPT | 46 | 3 | 43 (one more has a resolved `Status:` line) |
  | SIM | 19 | 0 | 19 |
  | SIMADV | 12 | 5 | 7 |
  | TAPEOUT | 17 | 1 | 16 |
  | TECH | 32 | 0 | 32 |
  | UI | 40 | 3 | 37 |
  | **Total** | **834** | **16** | **818** |

- **Options:** (a) resolve per milestone, just before it starts; (b) resolve everything up front; (c) first add lines mechanically wherever a decided item already answers the entry (for example the X-1 ref list: Q-LEFDEF-108, Q-MASK-14/204/218/323/336/408, Q-CELL-10, Q-EXTRACT-5/7, Q-NETLIST-1), then use (a).
- **Recommendation:** (c). The M1 entry gate needs TECH, CELL, MODEL and the viewer part of UI. M2 needs CMDEDIT and the rest of UI.
- **Priority:** blocks M0–M2.

### D-6: Spec-convention amendments for trace and compatibility syntax

- **Affects:** `docs/spec-conventions.md` and every spec (validation and the golden harness of `hefesto-testing-strategy`, M0). Source: G2 (OASIS traces), G3 Needs decision 5, G6 cross-group (tool refs).
- **Question:** three practices in the specs go beyond the letter of the conventions:
  - qualified `Compatibility:` lines such as `exact (content); behavioral (order)`, used by 38 requirements;
  - traces to tools other than Magic, such as `tt ref:`, `mpw_precheck ref:`, `ciel ref:`, `IHP ref:` and `librelane ref:` (31 requirements cite only such tools), alongside the established `netgen`, `xschem` and `ngspice` refs;
  - combined traces `(Hefesto decision: P-6; Magic ref: …)` on `exact` and `behavioral` OASIS requirements.

  Are these allowed?
- **Options:** (a) bless all three, and document the syntax in §1 and §2.3, with a pinned commit for each tool in the spec's Purpose; (b) forbid them, and split the requirements.
- **Recommendation:** (a). Splitting would add about 70 requirements and no precision. The golden harness reads the first class as the class for the main content.
- **Priority:** blocks M0–M2.
- **Status:** resolved (2026-11-07): option a — `docs/spec-conventions.md` §1 and §2.3 now allow qualified `Compatibility:` lines (first class = main content), traces to other reference tools with the tool's pinned commit in the spec's Purpose (pins added to `pcell-toolkit`, `scripting-api`, `reliability-em-check`), and combined `(Hefesto decision: …; <tool> ref: …)` traces.

### D-7: Is Magic's Tcl build the normative reference?

- **Affects:** `app-startup`, `cell-library-resolution` (M1), `ui-command-dispatch`, `ui-tools-and-input`, `layout-undo` (M2), `ui-gui-managers` (M4). Source: G1 Needs decision 2, G5 Terminology (build vocabulary).
- **Question:** X-3 puts Tcl in every Hefesto build. Where Magic's Tcl and non-Tcl builds differ, should the Tcl build be normative? Examples:
  - the `tool` command;
  - `echo`, `send` and `source` registration;
  - the startup-file rules;
  - `$VAR` sources ("without Tcl, step 1 is skipped");
  - the `undo` refusal message while recording is disabled.
- **Options:** (a) the Tcl build is normative, and non-Tcl text becomes informative; (b) decide case by case.
- **Recommendation:** (a). State it in `docs/spec-conventions.md`, and remove the "scripting build", "Tcl build" and "builds without …" wording from normative text (`scripting-pdk-integration`, `router-maze-iroute`, `view-3d`, `plot-output`, `ui-command-dispatch`).
- **Priority:** blocks M0–M2.
- **Status:** resolved (2026-11-06): option a — rule 11 of `docs/spec-conventions.md` makes Magic's Tcl build normative and non-Tcl behavior informative; build-conditioned wording requalified across the specs.

### D-8: Crash-backup location on the desktop (Q-CELL-30, with Q-UI-31, Q-SCRIPT-35 and Q-CMDEDIT-35)

- **Affects:** `cell-io-safety`, `hefesto-files-and-pdks` (M1), `cmd-hierarchy` (M2), `ui-gui-managers` (M4). Source: G1 Needs decision 5 and cross-group duplicates. The four entries are now marked as duplicates of Q-CELL-30.
- **Question:** do Linux and macOS keep Magic's `$TMPDIR/MAG<pid>.*` crash backups so that Magic and Hefesto can recover each other's sessions, or use an app-private directory? The duplicate entries recommend different answers, and Q-SCRIPT-35 picks a caches directory that the OS may purge.
- **Options:** (a) Magic's location and process-ID liveness test on the desktop, app-private on iPadOS; (b) an app-private directory everywhere (Application Support, not caches), a per-session lock file, and import of `MAG*` files on the desktop; (c) a caches directory.
- **Recommendation:** (b). Keep the Magic-readable file format. Keep the term "crash backup" for that file and "autosave" for the platform service (see the glossary).
- **Priority:** blocks M0–M2 (M1).
- **Status:** resolved (2026-11-07): option b — app-private crash backup directory (Application Support on macOS/iPadOS, `$XDG_STATE_HOME/hefesto` on Linux, never caches) with a per-session lock file and the Magic-readable format, plus import of Magic's `$TMPDIR/MAG*` backups on the desktop (`cell-io-safety`, `hefesto-files-and-pdks`, `cmd-hierarchy`, `ui-gui-managers`); Q-CELL-30 resolved, duplicates marked.

### D-9: Order of backup and archive cells, and the natural-comparator tie-break (Q-CELL-28, Q-CELL-12)

- **Affects:** `cell-io-safety`, `cell-file-format` (M1). Source: G1 Needs decision 3.
- **Question:** audit G1 applied X-1 (sort by name) to backup and archive cell order, although Q-CELL-28 is not in X-1's ref list. Is that confirmed? And what is the exact tie-break of the natural comparator, where Magic's comparator is not a strict weak order?
- **Options:** for the order, (a) confirm the sort or (b) copy the hash order (see D-1). For the tie-break, (a) a proper prefix sorts first, then byte comparison (Q-CELL-12 recommendation), or (b) another total order.
- **Recommendation:** confirm the sort, because the reader does not depend on order. Use the Q-CELL-12 tie-break.
- **Priority:** blocks M0–M2 (M1).
- **Status:** resolved (2026-11-07): sort confirmed — backup/archive cells are sorted by name; the natural comparator is a total order (natural prefix first, then unsigned byte comparison) in `cell-file-format`; Q-CELL-28 and Q-CELL-12 resolved.

### D-10: Undo scope beyond Magic: native undo events, properties and flatten in place

- **Affects:** `layout-undo`, `layout-properties`, `cmd-hierarchy` (M2), `agent-safety-and-approval` (M11), `simulation-command`, `simulation-measurements-and-statistics` (M9). Open questions Q-MODEL-14, Q-MODEL-28. Source: G1 Needs decision 4 and cross-group finding on the agent.
- **Question:** consolidation added `layout-undo` requirement "Native undo events for Hefesto compound operations", so that an accepted agent proposal (P-10) and `sim` testbench edits are one undo step each, including property changes and cell lifecycle. Is that confirmed? And should ordinary property changes and flatten in place also become undoable, as Q-MODEL-14 and Q-MODEL-28 recommend?
- **Options:** (a) confirm the native requirement and keep Magic's non-recorded list for ordinary commands; (b) also record property changes and flatten in place everywhere (a behavioral improvement with no file impact); (c) narrow the P-10 promise instead.
- **Recommendation:** (a) now, and (b) behind `magic-compat` OFF, as a later change. If (b) is chosen, update the scenario "Plain property change stays non-recorded".
- **Priority:** blocks M0–M2 (M2).
- **Status:** resolved (2026-11-07): options a and b — native undo events for agent proposals and `sim` edits confirmed; ordinary property changes and flatten in place stay non-recorded with `magic-compat` ON and are recorded with it OFF (`layout-undo`, `layout-properties`, `cmd-hierarchy`); Q-MODEL-14, Q-MODEL-28 (and Q-CMDEDIT-31) resolved.

### D-11: `undo 0` and `redo 0` (Q-MODEL-30, new)

- **Affects:** `layout-undo` (M2).
- **Question:** in Magic, `undo 0` resumes recording and `redo 0` suspends it, silently. Should Hefesto replicate this?
- **Options:** replicate | treat 0 as a no-op.
- **Recommendation:** replicate behind `magic-compat` (the spec currently states Magic's behavior).
- **Priority:** blocks M0–M2 (M2). Small.
- **Status:** resolved (2026-11-07): replicate behind `magic-compat` — `undo 0` resumes and `redo 0` suspends recording with it ON; both are no-ops with it OFF (`layout-undo`); Q-MODEL-30 resolved.

### D-12: macOS x86_64 in CI

- **Affects:** `hefesto-build-and-distribution`, `hefesto-testing-strategy` (M0), `hefesto-platform-targets`, `simulation-analyses-and-results`, decision P-2, roadmap exit criteria. Source: G6 Needs decision 1.
- **Question:** P-2 supports macOS x86_64, but CI, the determinism matrix and the exit criteria cover only macOS arm64. Add x86_64 to CI, or drop it from P-2?
- **Options:** (a) add a macOS x86_64 CI leg and determinism check; (b) drop x86_64 from P-2 and `simulation-analyses-and-results`.
- **Recommendation:** (b), unless there is a known user base on Intel Macs. Raven and MoltenVK testing on x86_64 is extra cost for a shrinking platform.
- **Priority:** blocks M0–M2 (M0).
- **Status:** resolved (2026-11-07): option b — macOS x86_64 dropped; P-2 amended to macOS 14+ on Apple silicon only (Linux x86_64 and aarch64 stay); `hefesto-platform-targets`, `hefesto-build-and-distribution`, `hefesto-product-operations`, `simulation-analyses-and-results` and `plugin-packaging-and-distribution` updated.

### D-13: Unnamed third-party dependencies and the P-9 component list

- **Affects:** `hefesto-licensing` (M0), `hefesto-pdk-manager` (M1), `plugin-*`, `agent-model-providers`, `hefesto-3d-visualization`, decision P-9. Source: G6 Needs decision 2 and cross-group finding on P-9.
- **Question:** the specs need SHA-256, Ed25519, an HTTPS client, Zstandard, JSON and glTF writing, but name no library. Separately, P-9 lists "Magic, Tcl, ngspice, zlib" as bundled permissive components, but ngspice has LGPL (KLU, numparam) and MPL (OSDI) parts, and the list omits Raven, MoltenVK, the fonts and zstd.
- **Options:** (a) platform crypto and networking on Apple platforms and permissive libraries on Linux (for example libsodium or Monocypher, libcurl, zstd, a header-only JSON library), each added to `hefesto-licensing`; (b) one cross-platform library set everywhere.
- **Recommendation:** (a), and correct the P-9 list to match `hefesto-licensing`.
- **Priority:** blocks M0–M2 (M0 licensing, M1 PDK manager).
- **Status:** resolved (2026-11-07): option a — CryptoKit and URLSession on Apple platforms, libsodium (or Monocypher) and libcurl on Linux, plus zstd, zlib, nlohmann/json and cgltf (or tinygltf) everywhere, each with its license in `hefesto-licensing` and referenced from the PDK manager, plugin, agent-provider, 3D and product-operations specs; P-9 component list corrected.

---

## Blocks a later milestone

### D-14: One project setting for global nets and power names

- **Affects:** `def-write`, `lef-write` (M6), `ext-flattening-names`, `ext-flattening-hierarchy`, `netlist-spice-options` (M7), `hefesto-settings`. Open questions Q-NETLIST-11, Q-NETLIST-33, Q-LEFDEF-44, Q-LEFDEF-102. Source: G3 Needs decision 2.
- **Question:** five specs treat Magic's Tcl `globals` array and `$VDD`/`$GND`/`$SUB` in different ways. Should one setting govern all of them?
- **Options:** (a) one project-level "global nets / power names" setting in `hefesto-settings`, seeded from the Tcl variables when a PDK script sets them; (b) keep each spec's own rule.
- **Recommendation:** (a). Resolve the four open questions together.
- **Priority:** blocks M6.
- **Status:** resolved (2026-11-07): option a — one project setting "global nets and power names" in `hefesto-settings` (`globalNets`, `vdd`, `gnd`, `sub`), seeded from the startup scripts' `globals`, `$VDD`, `$GND`, `$SUB`; used by `def-write`, `lef-write`, `ext-flattening-names`, `ext-flattening-hierarchy` and `netlist-spice-options`, with the trailing-`!` rule kept only where Magic's Tcl build applies it; Q-NETLIST-11, Q-NETLIST-33, Q-LEFDEF-44 and Q-LEFDEF-102 resolved.

### D-15: Tier-1 bugs still written as Magic behavior in extraction and netlisting

- **Affects:** `extresist-command`, `extresist-output`, `extract-devices`, `netlist-sim`, `netlist-spice-options` (M7). Open questions Q-NETLIST-21, Q-NETLIST-27, Q-EXTRACT-13, Q-NETLIST-14, Q-NETLIST-42. Source: G3 Needs decision 3.
- **Question:** these are uninitialized reads, out-of-bounds reads or dangling pointers, which X-2 always fixes. Should they get `Resolution: fix`, with the specs rewritten to the corrected behavior?
- **Options:** fix (tier 1) | treat individually.
- **Recommendation:** fix all five, and define the corrected value for each: `.freq` value, FastHenry z, the diode range check, merge-table bounds, and `-o` handling.
- **Priority:** blocks M7.

### D-16: Order of `use` records in `.ext` (Q-EXTRACT-5)

- **Affects:** `extract-file-format` (M7), `def-write` COMPONENTS (M6). Source: G3 Needs decision 4.
- **Question:** use order comes from Magic's subcell enumeration. X-1 classes it as neither tile order nor hash order. Is it copied exactly, as `def-write` COMPONENTS does for the same enumeration, or canonicalized?
- **Options:** exact | canonicalized.
- **Recommendation:** exact, consistent with `def-write`. Specify the enumeration once, in `layout-hierarchy`.
- **Priority:** blocks M6/M7.

### D-17: `extcheck` on iPadOS

- **Affects:** `netlist-extcheck` (M7), `hefesto-ipad-feature-scope` (M14). Source: G3 cross-group 3.
- **Question:** `netlist-extcheck` is a standalone executable with exit statuses, and iPadOS has no processes. Under the "not listed means included" rule, it is in the iPad scope. Is it?
- **Options:** (a) on iPadOS, an in-app `extcheck` command whose exit status becomes the command result and diagnostics; (b) desktop-only.
- **Recommendation:** (a). Consolidation already listed `ext-flattening-*` and `netlist-sim` explicitly as included.
- **Priority:** blocks M14 (spec shape in M7).

### D-18: Ownership of mask-hint semantics

- **Affects:** `mask-hints` (M3), `mask-output-operators`, `mask-input-styles`, `gds-read` (M5). Source: G2 Needs decision 1.
- **Question:** G2 assigned all hint semantics to `mask-hints`, and the other specs now keep catalog entries that point there. Is that confirmed?
- **Options:** confirm | move the operator semantics back to `mask-output-operators`.
- **Recommendation:** confirm.
- **Priority:** blocks M3.

### D-19: X-2 tier of the templayer hint lookup bug (Q-MASK-308, duplicate Q-MISC-11)

- **Affects:** `mask-hints` (M3), `mask-input-styles` (M5). Source: G2 Needs decision 2.
- **Question:** when two templayers write the same hint name, Magic replaces the earlier hint instead of adding to it. Is that tier 1 (data loss, always fix) or tier 2 (compat quirk)?
- **Options:** tier 1, union always | tier 2, replace under `magic-compat` ON.
- **Recommendation:** tier 1, because input hints are silently discarded. The spec then describes the union.
- **Priority:** blocks M3.

### D-20: `cif unfracture` value as the third word (Q-MASK-413)

- **Affects:** `mask-hierarchical-generation`, `mask-output-operators`, `cif-format-io` (M5). Source: G2 Needs decision 3.
- **Question:** Magic reads the yes/no value from the fourth word, so `cif unfracture yes` only queries. Should Hefesto accept the value as the third word?
- **Options:** always accept both forms | accept the third-word form only with `magic-compat` OFF | replicate.
- **Recommendation:** always accept both forms. This is a superset and changes no output, so record `Resolution: fix`.
- **Priority:** blocks M5.

### D-21: X-2 for xschem quirks

- **Affects:** `schematic-*` (M8). Open questions Q-SCHEM-12, -16, -19, -20, -22, -26. Source: G4 Needs decision 2.
- **Question:** X-2 is written for Magic. The SCHEM entries replicate some xschem quirks with no switch and fix others. Do X-2's tiers, including a compatibility setting, apply to xschem?
- **Options:** (a) apply X-2, with an `xschem-compat` setting defined like `netgen-compat` (D-2); (b) replicate always; (c) case by case without a switch.
- **Recommendation:** (a).
- **Priority:** blocks M8.

### D-22: ngspice `.param` precedence (Q-SIMADV-3, -4, -9)

- **Affects:** `simulation-monte-carlo`, `simulation-corners`, `simulation-sweeps` (M9). Source: G4 Needs decision 4.
- **Question:** sky130 Monte Carlo mode `both`, the gf180 statistical overrides and sweep overrides all assume that a later top-level `.param` overrides an earlier one. Is that true for the pinned ngspice?
- **Options:** confirm with a reference run (X-6) | redesign the overrides so that they do not depend on order.
- **Recommendation:** run the reference deck before M9 starts. This is a verification decision, not a product decision.
- **Priority:** blocks M9.

### D-23: sg13g2 `mm_ok=1` for extracted devices (rest of Q-SIMADV-5)

- **Affects:** `netlist-spice-devices`, `extract-devices` (M7), `simulation-monte-carlo` (M9). Source: G4 cross-group and Needs decision 5.
- **Question:** sg13g2 mismatch needs `mm_ok=1` on every device instance. Layout-extracted netlists never write it, so extracted DUTs show no mismatch and only warn. Should the netlister or the simulation preparation add it?
- **Options:** (a) simulation preparation adds it for sg13g2 extracted DUTs (no change to Magic-compatible SPICE output); (b) the netlister writes it (diverges from Magic); (c) keep the warning.
- **Recommendation:** (a).
- **Priority:** blocks M9.

### D-24: A `measured` waveform-retention mode (Q-SIMADV-11)

- **Affects:** `simulation-sweeps`, `simulation-analyses-and-results`, `simulation-measurements-and-statistics` (M9). Source: G4 Needs decision 6.
- **Question:** should a `measured` retention mode (keep only the vectors used by measurements) become the default above 100 points, mainly for iPad storage?
- **Options:** keep the current defaults | add `measured` as an option | make it the default above 100 points.
- **Recommendation:** add it, and make it the default once its storage cost has been measured.
- **Priority:** blocks M9.

### D-25: Engine recovery after an ngspice fault (Q-SIM-11)

- **Affects:** `simulation-engine-embedding` (M9). Source: G4 Needs decision 7.
- **Question:** the spec resets the engine after a fault on every platform. Q-SIM-11 recommends always replacing the worker process on desktop. Which?
- **Options:** reset everywhere | replace the worker on desktop, reset on iPad.
- **Recommendation:** replace the worker on desktop, and reset on iPad with fuzz tests.
- **Priority:** blocks M9.

### D-26: sky130 descriptor details (Q-SIM-8, Q-SIMADV-2)

- **Affects:** `simulation-netlist-preparation`, `simulation-corners` (M9), `hefesto-pdk-manager`. Source: G4 Needs decision 8.
- **Question:** is `set ng_nomodcheck` still needed with the combined models? Should the desktop descriptor generator scan `libs.tech/combined` instead of `libs.tech/ngspice`?
- **Options:** keep both as specified | drop `ng_nomodcheck` and scan `combined`.
- **Recommendation:** scan `combined` (the bundled descriptor already uses it), and decide `ng_nomodcheck` with a reference run.
- **Priority:** blocks M9.

### D-27: gf180 compatibility mode `hs` or `hsa` (Q-SIMADV-4)

- **Affects:** `simulation-netlist-preparation`, `simulation-engine-embedding` (M9). Source: G4 Needs decision 9.
- **Question:** the PDK's `.spiceinit` sets `ngbehavior=hs`, but the bundled descriptor uses `hsa`. Which one does Hefesto use?
- **Options:** follow the PDK (`hs`) | keep `hsa`.
- **Recommendation:** follow the PDK, unless a reference run shows a difference that `hsa` fixes.
- **Priority:** blocks M9.

### D-28: Linking ngspice's LGPL and MPL parts (Q-SIM-1)

- **Affects:** `simulation-engine-embedding` (M9), `hefesto-licensing`, the iPad build (M14). Source: G6 cross-group; roadmap M9 de-risking.
- **Question:** KLU and numparam are LGPL, and OSDI is MPL. How are they linked into the iPad app, which cannot load external dynamic libraries?
- **Options:** (a) static linking, with object files and relink instructions on request; (b) an embedded, signed dynamic framework; (c) build without KLU and ask upstream to relicense numparam; (d) legal review first.
- **Recommendation:** (d), then (b) if legal review agrees, otherwise (a). Exclude the GPL `table` code models in all cases.
- **Priority:** blocks M9 (must be settled before M14).

### D-29: Native and Tcl PCells, byte-identical `.mag` and the timestamp line

- **Affects:** `hefesto-scripting`, `pcell-toolkit` (M4). Source: G5 cross-group 1.
- **Question:** `hefesto-scripting` requires native and Tcl generator output to be byte-identical `.mag` files, but each saved `.mag` contains a `timestamp` line, so cells generated at different times can never be identical. `pcell-toolkit` uses geometric identity instead. Which definition holds?
- **Options:** (a) byte-identical except the `timestamp` line; (b) the `pcell-toolkit` identity (paint, labels, ports, properties, name); (c) generate both with a fixed timestamp in tests.
- **Recommendation:** (a). It is the stronger check and easy to automate. Make `pcell-toolkit` refer to it.
- **Priority:** blocks M4.
- **Status:** resolved (2026-11-11): option a — native and Tcl generator output are compared as byte-identical saved `.mag` files except the `timestamp` line; `hefesto-scripting` "Native PCell generators validated against Tcl" states it and `pcell-toolkit` "Native generator equivalence" refers to it.

### D-30: Is the declarative PDK profile still needed? (Q-SCRIPT-46)

- **Affects:** `scripting-pdk-integration`, `hefesto-scripting`, `hefesto-security` (M4), the iPad app (M14). Source: G5 Needs decision 1.
- **Question:** under X-3 every platform runs Tcl. Is a declarative (non-script) PDK profile still needed?
- **Options:** (a) keep it for uncurated PDKs if App Review rejects imported scripts; (b) drop it; (c) also use it when the user declines script consent.
- **Recommendation:** (b), and revisit only if the M4 App Review inquiry rejects imported scripts.
- **Priority:** blocks M4.
- **Status:** resolved (2026-11-11): option b, by the approved resolution of Q-SCRIPT-46 (docs/decisions/open-questions/SCRIPT.md) — the declarative PDK profile is dropped; iPadOS curated-pack mode refuses PDKs without bundled scripts, and a declined consent keeps the read-only `minimum`-tech fallback of `hefesto-security`.

### D-31: `env(PDK_ROOT)` and other environment variables in the sandboxed Tcl on iPadOS

- **Affects:** `hefesto-scripting` (M4), `scripting-api`, `scripting-pdk-integration`, `hefesto-files-and-pdks`, `hefesto-pdk-manager` (M1), the iPad app (M14). Source: G5 Needs decision 2.
- **Question:** PDK rc files and device scripts read `$env(PDK_ROOT)`, and `scripting-api` reads `CAD_ROOT` from the environment. iPadOS has no environment variables. What does the sandbox's `env` array contain?
- **Options:** (a) a synthesized `env` array with `PDK_ROOT`, `PDK` and `CAD_ROOT` taken from the PDK store and settings, the same values `hefesto-pdk-manager` uses on desktop, where writes stay local to the interpreter; (b) an empty `env`, with the PDK scripts patched in curated packs.
- **Recommendation:** (a), defined in `hefesto-scripting`, with the same path-variable table as Q-CELL-17.
- **Priority:** blocks M4 (the iPad impact is in M14).
- **Status:** resolved (2026-11-11): option a — the sandbox's `env` array is synthesized with `PDK_ROOT`, `PDK` and `CAD_ROOT` from the PDK store and settings (the Q-CELL-17 path-variable table), writes stay local to the interpreter, on every platform; defined in `hefesto-scripting` "Embedded sandboxed Tcl on all platforms".

### D-32: Tk menu writes by PDK scripts on iPadOS (Q-SCRIPT-34, Q-SCRIPT-42)

- **Affects:** `scripting-pdk-integration`, `ui-gui-managers` (M4), the iPad app (M14). Source: G5 Needs decision 3.
- **Question:** under X-3, the sky130, gf180mcu and sg13g2 `addtechmenu` callbacks also run on iPadOS. They write to `${frame}.titlebar…` menus and call `tk_getOpenFile` and `winfo`. Does the SwiftUI shell intercept these, or report them as skipped?
- **Options:** (a) a Tk-compatible widget shim; (b) intercept the known operations and map them to native menu actions, in both shells; (c) skip them and report.
- **Recommendation:** (b) on every platform, with a report of the statements that were skipped.
- **Priority:** blocks M4.
- **Status:** resolved (2026-11-11): option b, by the approved resolutions of Q-SCRIPT-42 and Q-SCRIPT-34 (docs/decisions/open-questions/SCRIPT.md) — the known widget operations are intercepted and mapped to native menu actions in both shells (desktop and iPad), and every other widget statement is skipped and reported.

### D-33: `cd` in the Tcl sandbox

- **Affects:** `scripting-api`, `hefesto-scripting` (M4). Source: G5 Needs decision 5.
- **Question:** the `scripting-api` scenario "Relative cell path survives cd" uses `cd /tmp`, but the sandbox limits file access to granted paths. Is `cd` allowed, and within which roots?
- **Options:** allow `cd` within granted roots (project, PDK store, temporary area) and fail otherwise | forbid `cd`.
- **Recommendation:** allow it within granted roots, and change the scenario to use a project subfolder.
- **Priority:** blocks M4.
- **Status:** resolved (2026-11-11): as recommended — `cd` is allowed within the granted roots (project, PDK store, temporary area) and fails otherwise (`hefesto-scripting`); the `scripting-api` scenario "Relative cell path survives cd" now uses a project subfolder.

### D-34: The `irsim`, `netgen` and `xcircuit` helper commands

- **Affects:** `scripting-api` (M4), `irsim-interface` (M12), `lvs-command` (M9). Source: G5 Needs decision 6.
- **Question:** these helpers source external Tcl packages that load native libraries, which the sandbox denies. What does Hefesto do with them?
- **Options:** no-ops with a diagnostic | map `netgen` to the built-in LVS (P-5) | omit them.
- **Recommendation:** `netgen` prints a diagnostic that names the built-in `lvs` command, and does not emulate it. `irsim` follows `irsim-interface` (desktop-only). `xcircuit` gives a "not available" diagnostic.
- **Priority:** blocks M4.
- **Status:** resolved (2026-11-11): as recommended — `netgen` prints a diagnostic naming the built-in `lvs` and does not emulate netgen, `irsim` follows `irsim-interface` (desktop only), `xcircuit` gives a "not available" diagnostic; specified in `scripting-api` "Generic Tcl helpers defined at startup".

### D-35: `getnode` on iPadOS

- **Affects:** `irsim-interface` (M12), `hefesto-ipad-feature-scope` (M14). Source: G5 Needs decision 4, G4 cross-group.
- **Question:** `irsim-interface` is desktop-only, but `getnode` is a pure node-naming query with no process. Consolidation stated that the naming rules apply on every platform. Is the `getnode` command available on iPad?
- **Options:** (a) move `getnode` and the naming requirements into an always-available capability, such as `extract-nodes-and-labels`, and include it on iPad; (b) keep `getnode` desktop-only.
- **Recommendation:** (a).
- **Priority:** blocks M12 (and M14 scope).

### D-36: `quit -noprompt` from a PDK rc script on iPadOS

- **Affects:** `app-startup` (M1), `scripting-pdk-integration` (M4), the iPad app (M14). Source: G5 cross-group 7.
- **Question:** PDK rc guards call `quit -noprompt` when the technology is wrong. An iPadOS app cannot quit itself. What happens?
- **Options:** abort loading the project and return to the project browser with a diagnostic | ignore `quit` and continue.
- **Recommendation:** abort and return to the project browser. Define this in `app-startup`.
- **Priority:** blocks M14 (the spec text is needed by M4).
- **Status:** resolved (2026-11-11): as recommended — on iPadOS `quit` (including a PDK rc guard's `quit -noprompt`) aborts loading, returns to the project browser and shows a diagnostic; defined in `app-startup` "quit command and exit status".

### D-37: PNG export for reliability heat maps

- **Affects:** `reliability-visualization` (M10), `plot-output`, `hefesto-rendering`. Source: G5 cross-group 3.
- **Question:** `reliability-visualization` exports PNG "through `plot-output`", but `plot-output` has no PNG format. Who provides it?
- **Options:** (a) a native PNG export from an offscreen render, in `hefesto-rendering`; (b) a native PNG format in `plot-output`.
- **Recommendation:** (a), shared with D-38.
- **Priority:** blocks M10.

### D-38: Owner of SVG and vector export

- **Affects:** `plot-output`, `hefesto-rendering` (M10). Source: G5 cross-group 4.
- **Question:** Magic produces SVG through its Cairo display. Hefesto has no Cairo path (P-3). Which capability produces the SVG of a window's visible contents?
- **Options:** `hefesto-rendering` | a new native requirement in `plot-output`.
- **Recommendation:** `hefesto-rendering`, which owns raster and vector export of a view.
- **Priority:** blocks M10.

### D-39: IR-drop solver and the wording of P-21 (Q-RELIABILITY-9)

- **Affects:** `reliability-ir-drop` (M10), `hefesto-licensing`, decision P-21. Source: G6 Needs decision 3.
- **Question:** P-21 says Hefesto uses "its own sparse linear solver". Q-RELIABILITY-9 recommends Eigen (MPL-2.0). Which is it?
- **Options:** Eigen | Hefesto's own solver | KLU (LGPL, see D-28).
- **Recommendation:** Eigen with fixed threading for determinism. Amend P-21 and add Eigen to `hefesto-licensing`.
- **Priority:** blocks M10.

### D-40: Axis convention for the 3D view

- **Affects:** `hefesto-3d-visualization`, `view-3d` (M10). Source: G5 cross-group 2.
- **Question:** Magic places a layer's top face at z = −h·s_z. `hefesto-3d-visualization` says "from z = 1.0 to z = 1.5". The magnitudes agree, but no capability states the sign or axis convention. Which convention does Hefesto use?
- **Options:** +z up (height increases z) | Magic's negated z.
- **Recommendation:** +z up for Hefesto meshes and glTF/STL export, with a note that Magic's view negates z. Parity is judged on the rendered image.
- **Priority:** blocks M10.

### D-41: Staleness of EM/IR runs on the sign-off checklist

- **Affects:** `reliability-em-check` (M10), `tapeout-signoff-checklist`, `tapeout-shuttle-packages` (M13). Source: G6 cross-group.
- **Question:** the checklist marks results stale by the GDS SHA-256, but EM/IR runs are keyed on the cell modification stamp. How does a cell-keyed run map to a GDS-keyed checklist input? Separately, the EM item says "unless the program profile says otherwise", but no built-in profile sets it.
- **Options:** record the GDS hash of the layout the run analyzed | re-key checklist items by cell stamp.
- **Recommendation:** record, with each EM/IR run, the GDS hash it corresponds to, written when the GDS is exported. State in `tapeout-shuttle-packages` that no built-in profile requires the EM item.
- **Priority:** blocks M13.

### D-42: iPad minimum memory

- **Affects:** `hefesto-product-operations` (M13), `hefesto-performance`, the iPad app (M14). Source: G6 Needs decision 6.
- **Question:** the stated minimum is A14 with 4 GB, but the 3 GB resident bound for the reference full-chip layout was measured on an 8 GB iPad. Can a 4 GB device open that workload?
- **Options:** keep 4 GB with a smaller supported workload | raise the minimum to 8 GB.
- **Recommendation:** keep 4 GB as the minimum for the app, and state that the full-chip reference workload needs 8 GB.
- **Priority:** blocks M14.

---

## Cosmetic

### D-43: `archive` after a usage error (Q-CMDEDIT-34)

- **Affects:** `cmd-hierarchy` (M2). Source: G2 Needs decision 4.
- **Question:** Magic falls through after the usage message, but the option is -1, so nothing happens. The entry has been reworded. Record `Resolution: fix`, meaning stop after the usage message?
- **Recommendation:** yes. Behavior is the same as Magic's.
- **Priority:** cosmetic.

### D-44: Pseudo-code identifiers that mirror Magic names

- **Affects:** `drc-rule-semantics` (`TopType`, `LeftType`, `RightType`), `mask-hierarchical-generation` (`HierLayers`, `YankTypes`, `NonlocalGen`), `router-channel` (`markWanted`, `pickBest`, `collapse`, `makeFeasible`), `plow` (`mustMove`, `slivType`, `farDist`). Source: G2, G5.
- **Question:** these names are defined in the specs but resemble Magic macros, functions or locals. Should they be renamed, for example to `topType(...)`, to keep the specs independent of Magic's implementation (convention rule 2)?
- **Recommendation:** keep them, and add a sentence to convention rule 2 allowing pseudo-code names that are defined in the spec.
- **Priority:** cosmetic.

### D-45: US or British spelling

- **Affects:** `simulation-*`, `schematic-*`, `plot-output`, `view-3d`, `router-*`, `scheme-interpreter`, `openaccess`, `plow`, `scripting-pdk-integration`. Source: G4, G5, G6.
- **Question:** most specs use US spelling. Some requirement titles use British spelling ("initialisation", "optimisation", "behaviour"), and titles are referenced by name. Should everything be normalized?
- **Recommendation:** use US spelling. Rename titles together with every quoted reference in one mechanical change, and keep Magic, netgen, xschem and ngspice message strings verbatim.
- **Priority:** cosmetic.

### D-46: Unify notations across sibling specs

- **Affects:** `cell-file-format`, `cmd-coordinates-and-units`, `tech-loading-and-scaling` (lambda ratio); `gds-read`, `gds-write`, `cif-format-io`, `drc-cif-rules` (`S`, `σ`, `scaleFactor`); `lef-*`, `def-*` (`oscale`, `s`); `mask-input-styles`, `gds-read` (`M`/`m`); the three restatements of the file-opening rules (`tech-loading-and-scaling`, `display-style-files`, `app-startup`). Source: G1, G2, G3 Terminology.
- **Question:** the same quantity uses different symbols in sibling specs, and the same symbol means different quantities. Should they be unified to the notation in `docs/glossary.md`, with one owner for the file-opening rules?
- **Recommendation:** yes. Apply it when each spec next changes. The glossary already records the canonical forms.
- **Priority:** cosmetic.
