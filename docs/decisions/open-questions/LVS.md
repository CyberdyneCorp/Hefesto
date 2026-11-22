# Open Questions: LVS area

These entries cover the capabilities `lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports` and `lvs-command`.

The reference is netgen, not Magic: `RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece` (version 1.5.327). The "Magic behavior" field below therefore describes **netgen behavior**, and "Evidence" cites netgen sources.

Decision X-2 (tiered bug handling) applies to these entries, behind the `netgen-compat` setting chosen in Q-LVS-28 (capability `hefesto-magic-compat-mode`). Contributors resolving these entries SHALL follow the clean-room rule in capability `hefesto-licensing`.

### Q-LVS-1: Legacy `.ext` reader drops modern device records
- Kind: doc-mismatch
- Magic behavior: netgen's `readnet ext`/`extflat` parses `device` records but creates no device. Only obsolete `fet` records become transistors, so a current Magic `.ext` file loads as nets with no devices. The documented flow is `ext2spice` to SPICE and then netgen.
- Evidence: base/ext.c:239-308, base/ext.c:310-362
- Options: replicate (support the reader with the same gap) | fix (map `device` records to devices) | omit (do not offer `readnet ext`)
- Recommendation: omit from the GUI. Keep `readnet ext` for script compatibility and replicate it. LVS layout input always goes through the internal `ext2spice lvs` path (capability `lvs-command`).

### Q-LVS-2: Report order inherits netgen hash and partition order
- Kind: quirk
- Magic behavior: several orders come from internal lists that are reshuffled by refinement, or from hash iteration. This affects pin table rows, mismatch fragment groups, `Contents of circuit` class lists, the indices of properties of interest, and property-error blocks.
- Evidence: base/netcmp.c:7703-7895, base/netcmp.c:1615-2100, base/query.c:820-830, base/netcmp.c:4970-4990
- Options: replicate (emulate the hash tables and partition lists exactly) | fix (deterministic order, compared order-insensitively)
- Recommendation: fix, following decision X-1. Compare these sections as sets (capability `lvs-reports`, Requirement "Canonical golden comparison").

### Q-LVS-3: Case-insensitive matching versus hashing; high bytes
- Kind: bug
- Magic behavior: case-insensitive equality folds `[{<` and `]}>` into one bracket pair, but the case-insensitive name hash folds only letters. So `a[1]` and `a<1>` compare equal yet hash differently, and hash lookups miss. Both fold tables are indexed by a plain `char`. Bytes ≥ 0x80 therefore index out of bounds on signed-char platforms: the hash table has 128 entries and the match table has 256 but is indexed negatively.
- Evidence: base/objlist.c:193-258, base/hash.c:116-152
- Options: replicate | fix (fold brackets in the hash too; index by unsigned byte)
- Recommendation: fix (tier 1 for the out-of-bounds index). Folding brackets in the hash changes only lookups that netgen fails. Golden cases NG-NAME-02 and NG-NAME-03 measure the effect.
- Resolution: fix — index the fold tables by unsigned byte (out-of-bounds read on signed-char platforms) and fold brackets in the case-insensitive hash too, as the entry recommends (X-2 tier 1, 2026-11-07).

### Q-LVS-4: Hash collisions and pseudo-random labels
- Kind: ambiguity
- Magic behavior: classes are separated by 64-bit wrap-around sums of pseudo-random labels (a `ran2`-style generator). A collision can merge distinct classes, which may cause false symmetries or mask a mismatch. netgen does not detect collisions.
- Evidence: base/netcmp.c:3018-3021, base/netcmp.c:4125-4155, base/pdutils.c:39-114
- Options: replicate (reproduce the generator and seeds bit for bit) | fix (any deterministic generator, plus a collision check that splits classes by an exact structural signature)
- Recommendation: fix. The results are equal except on collisions, which are vanishingly rare. Label values are not observable.

### Q-LVS-5: `permute forget <cell>` leaves a dangling pair list
- Kind: bug
- Magic behavior: forgetting all permutations of a cell frees every pair but does not clear the list head. Later use reads freed memory.
- Evidence: base/netcmp.c:6996-7004
- Options: fix (clear the list)
- Recommendation: fix (tier 1).
- Resolution: fix — clear the pair list when `permute forget` frees it (use-after-free) (X-2 tier 1, 2026-11-07).

### Q-LVS-6: `property add` single-word and `{key tol}` parsing
- Kind: quirk
- Magic behavior:
  - A single-word spec is typed by testing whether the *key text* is a number: numeric key → double, otherwise string with tolerance 0. So `property c add w` creates a **string** property `w`, compared exactly.
  - In `{key tol}`, a non-numeric `tol` that is not a type keyword passes an uninitialized tolerance.
- Evidence: tcltk/tclnetgen.c:3800-3846
- Options: replicate | fix (single word → string; non-numeric tol → error)
- Recommendation: replicate the single-word rule, since PDK files may depend on it. Fix the uninitialized tolerance by raising an error (tier 1).

### Q-LVS-7: Relative tolerance for zero or negative values
- Kind: quirk
- Magic behavior: the double tolerance ratio is 2·|a−b|/(a+b):
  - a = b = 0 gives NaN, which counts as a match
  - a = −b ≠ 0 gives ∞, a mismatch
  - negative sums give negative ratios, which always match
  
  The sort routine divides by |a+b|, with the same edge cases.
- Evidence: base/netcmp.c:5817-5835, base/netcmp.c:4266-4280, base/netcmp.c:5090-5100
- Options: replicate | fix (use |a−b| / max(|a|,|b|), with 0 = 0)
- Recommendation: replicate behind the `netgen-compat` setting (it changes results). Cover with golden case NG-PROP-07.
- Resolution: compat — netgen's tolerance ratio 2·|a−b|/(a+b) with its NaN, ∞ and negative-sum outcomes, behind `netgen-compat`; with `netgen-compat` OFF, the ratio is |a−b| / max(|a|,|b|), and two zero values match (X-2 tier 2 as amended by D-2, D-5, approved 2026-11-07, 2026-11-07).

### Q-LVS-8: Inconsistent sort comparators
- Kind: bug
- Magic behavior: the property-record sort comparators return only 1 or 0, never −1, and are passed to `qsort`. The resulting order depends on the C library's sort algorithm (glibc merge sort versus BSD/macOS). Series/parallel reconciliation can therefore differ between Linux and macOS netgen builds.
- Evidence: base/netcmp.c:4246-4280, base/netcmp.c:4413, base/netcmp.c:4599
- Options: replicate glibc behavior | fix (stable sort with a proper three-way comparator)
- Recommendation: fix with a stable sort. Capture golden outputs from Linux netgen and note any macOS divergence.
- Resolution: fix — stable sort with a proper three-way comparator, one order on every platform (platform-dependent sort) (X-1 platform-dependent sorts, 2026-11-07).

### Q-LVS-9: Parallel signature uses only the first permutable pair
- Kind: quirk
- Magic behavior: only the first declared pin permutation produces an alternate parallel signature, so devices with several permutable pairs may not merge. If the pin names are not found on an instance, uninitialized pointers are used.
- Evidence: base/netgen.c:3335-3384
- Options: replicate | fix (generate signatures for all permutation-equivalent pin orders)
- Recommendation: replicate the single pair behind the `netgen-compat` setting, because merging more changes the device counts that are reported. Fix the uninitialized use (tier 1).
- Resolution: compat — only the first permutable pair gives an alternate parallel signature, behind `netgen-compat` (merging more changes the reported device counts); the uninitialised pointers when pin names are missing are always fixed (tier 1); with `netgen-compat` OFF, signatures are generated for every permutation-equivalent pin order (X-2 tier 2 as amended by D-2, D-5, approved 2026-11-07, 2026-11-07).

### Q-LVS-10: Property comparison failure is reported as a property error
- Kind: ambiguity
- Magic behavior: a pair whose properties cannot be compared (class-identity mismatch, malformed records) sets the property state to "failure" (−1). `verify unique` then returns −3, exactly as for property errors. Later pairs are still checked.
- Evidence: base/netcmp.c:6510-6522, tcltk/tclnetgen.c:2705-2712
- Options: replicate | fix (a distinct status)
- Recommendation: replicate in the netgen-compatible codes. Expose a distinct `property-check-failed` detail in Hefesto's structured result.

### Q-LVS-11: Pre-match flattening heuristic
- Kind: ambiguity
- Magic behavior: the decision to flatten a subcircuit before matching depends on instance counts adjusted in hash order, with three asymmetric cases. Results can depend on the order in which classes are visited.
- Evidence: base/flatten.c:1673-1960
- Options: replicate (exact emulation including order) | fix (an order-independent rule that agrees on all golden cases)
- Recommendation: behavioral compatibility, validated by the NG-HIER golden cases. Record any divergence as a new entry.

### Q-LVS-12: Report text used as a printf format
- Kind: bug
- Magic behavior: formatted two-column lines and blank separator lines are passed to the logger as the *format string*. A `%` in a net, pin or cell name produces garbage or a crash.
- Evidence: base/netcmp.c:1559, base/netcmp.c:1686, base/netcmp.c:2031
- Options: fix (print literally)
- Recommendation: fix (tier 1). Hefesto writes names literally.
- Resolution: fix — report text is written literally, never used as a format string (crash on `%`) (X-2 tier 1, 2026-11-07).

### Q-LVS-13: JSON file naming and escaping
- Kind: bug
- Magic behavior:
  - **Naming.** The JSON name replaces the log name from its last `.`; a name with no `.` becomes `.json`. A `nolog` log name still produces a JSON file derived from it.
  - **Escaping.** Only backslashes are escaped. Quotes and control characters in names produce invalid JSON.
  - **Empty lists.** An empty `devices` list for one circuit writes `["", ]`.
- Evidence: tcltk/netgen.tcl.in:32-110
- Options: replicate | fix (append `.json` when there is no extension; full JSON escaping; valid empty lists)
- Recommendation: fix the escaping and empty lists (invalid output is data loss for consumers). Replicate the naming for names that have an extension, and append `.json` otherwise.

### Q-LVS-14: `.include` home-directory expansion [platform]
- Kind: platform
- Magic behavior: `~/` uses `$HOME` and `~user/` uses the password database.
- Evidence: base/spice.c:800-835
- Options: platform alternative (map `~` to the user's Hefesto documents root; reject `~user`)
- Recommendation: platform alternative on iPadOS. Desktop keeps netgen behavior.

### Q-LVS-15: Environment variables in setup files [platform]
- Kind: platform
- Magic behavior: setup files read `$env(...)`, for example `NETGEN_COLUMNS` and `PDK_ROOT`.
- Evidence: tcltk/netgen.tcl.in:772-783; open_pdks setup files
- Options: platform alternative (`env` array backed by Hefesto settings, per decision X-4)
- Recommendation: platform alternative on all platforms, for determinism.
- Resolution: platform — the setup-file `env` array is backed by Hefesto settings on every platform (X-4, 2026-11-07).

### Q-LVS-16: Column widths: no validation and right-column overflow
- Kind: bug
- Magic behavior:
  - `format` with a width ≤ 0 reports an error but applies the widths anyway.
  - The right column is truncated at the *left* column width. When w2 < w1 this writes past the line buffer.
- Evidence: tcltk/tclnetgen.c:2073-2082, base/netcmp.c:1539-1545, base/netcmp.c:1582-1590
- Options: fix (reject widths ≤ 0; truncate the right column at its own width)
- Recommendation: fix (tier 1). With default and equal widths the output is identical.
- Resolution: fix — reject widths ≤ 0 and truncate the right column at its own width (buffer overflow); output is unchanged for default and equal widths (X-2 tier 1, 2026-11-07).

### Q-LVS-17: Pin-match results −1 and −2 are never produced
- Kind: quirk
- Magic behavior: the pin matcher returns only 2, 1 or 0. The `lvs` branch that flattens a subcell on a −1 pin result, and the message `Attempt to match empty cell to non-empty cell.`, are unreachable.
- Evidence: base/netcmp.c:7636-8525, tcltk/netgen.tcl.in:672-686, tcltk/tclnetgen.c:3180-3186
- Options: replicate
- Recommendation: replicate (dead behavior). Document it in capability `lvs-hierarchy`.

### Q-LVS-18: Missing double property takes a garbage default during merging
- Kind: bug
- Magic behavior: when parallel/series merging compares records and one record lacks a double or value property, the default is read from the integer field of the definition instead of the double field.
- Evidence: base/netcmp.c:5096-5108
- Options: fix (use the double default)
- Recommendation: fix (tier 1, an uninitialized-style read). Golden case NG-PROP-10 records any divergence.
- Resolution: fix — use the double default of the property definition (garbage read) (X-2 tier 1, 2026-11-07).

### Q-LVS-19: List/JSON value for value-type and expression properties
- Kind: bug
- Magic behavior: in `-list`/`-json` property entries, a circuit-1 value of type `value` or `expression`, or a circuit-2 value of type `value`, is not converted. The previous list element (the key) is appended again.
- Evidence: base/netcmp.c:5447-5500
- Options: fix (write the numeric value, or `(unresolved expression)`)
- Recommendation: fix. It affects only structured output.

### Q-LVS-20: Symmetry result missing from the log
- Kind: quirk
- Magic behavior: `verify only` reports `Circuits match with <k> symmetries.` to the console only. If symmetries survive resolution, `comp.out` ends with `Final result: ` and no verdict.
- Evidence: tcltk/tclnetgen.c:2697-2701, tcltk/netgen.tcl.in:705-715
- Options: replicate | fix (also log it)
- Recommendation: replicate behind the `netgen-compat` setting. The structured result always carries the status.
- Resolution: compat — the symmetry verdict goes to the console only and `comp.out` ends with `Final result: `, behind `netgen-compat`; the structured result always carries the status; with `netgen-compat` OFF, `comp.out` also records `Circuits match with <k> symmetries.` (X-2 tier 2 as amended by D-2, D-5, approved 2026-11-07, 2026-11-07).

### Q-LVS-21: Extra `.` line after a unique match
- Kind: quirk
- Magic behavior: `verify only` prints `Circuits match uniquely.` and then a separate line `.` when there are no property errors. Downstream scripts grep for this.
- Evidence: tcltk/tclnetgen.c:2710-2715
- Options: replicate
- Recommendation: replicate (exact format; tools depend on it).

### Q-LVS-22: `ignore` without a keyword
- Kind: bug
- Magic behavior: `ignore <cell>` fails with a wrong-argument error. `ignore <x> <cell>`, where `<x>` is not `class` or `shorted`, uses an uninitialized option index.
- Evidence: tcltk/tclnetgen.c:2880-2916
- Options: fix (error `bad option`)
- Recommendation: fix (tier 1). Keep the wrong-argument error for the one-argument form.
- Resolution: fix — an unknown option word reports `bad option` instead of using an uninitialised option index; the one-argument form keeps its wrong-argument error (X-2 tier 1, 2026-11-07).

### Q-LVS-23: Pin matching writes to freed memory
- Kind: bug
- Magic behavior: after port pairing, the "port connection errors" pass writes into the net-correspondence array after it has been freed.
- Evidence: base/netcmp.c:8144, base/netcmp.c:8409
- Options: fix
- Recommendation: fix (tier 1). It has no effect on printed output.
- Resolution: fix — no write to the freed net-correspondence array (use-after-free); printed output is unchanged (X-2 tier 1, 2026-11-07).

### Q-LVS-24: Placeholder test uses logical instead of bitwise AND
- Kind: bug
- Magic behavior: the size-1-array pin-name equivalence is meant for placeholder cells. Its test treats circuit 2 as a placeholder whenever it has *any* flag set, so the relaxation applies far more often than intended.
- Evidence: base/netcmp.c:7935
- Options: replicate | fix (test the placeholder flag)
- Recommendation: replicate behind the `netgen-compat` setting, because it changes pin pairing results. Measure with golden case NG-PIN-06.
- Resolution: compat — the logical-AND placeholder test, behind `netgen-compat` (it changes pin pairing); with `netgen-compat` OFF, only the placeholder flag enables the size-1-array pin-name equivalence (X-2 tier 2 as amended by D-2, D-5, approved 2026-11-07, 2026-11-07).

### Q-LVS-25: Orphan-net summary printed with missing arguments
- Kind: bug
- Magic behavior: the console summary line for orphan nets is printed without its two count arguments (undefined behavior).
- Evidence: base/netcmp.c:540
- Options: fix
- Recommendation: fix (tier 1). It is console-only and not part of `comp.out`.
- Resolution: fix — print the orphan-net summary with its two counts (missing printf arguments) (X-2 tier 1, 2026-11-07).

### Q-LVS-26: No-connect pins merged as one net for parallel detection
- Kind: ambiguity
- Magic behavior: in non-top cells, all single-connection nets are treated as the same net (`_nc`) when finding parallel devices. Devices whose open pins are unrelated can merge. Top cells are excluded.
- Evidence: base/netgen.c:3252-3300
- Options: replicate | fix
- Recommendation: replicate. It is intended behavior (`property parallel connected` disables it).

### Q-LVS-27: Running PDK setup scripts on iPadOS [platform]
- Kind: platform
- Magic behavior: setup files are arbitrary Tcl executed by netgen.
- Evidence: tcltk/netgen.tcl.in:503-526
- Options: platform alternative (the sandboxed Tcl of decision X-3; curated bundled setup files if App Review rejects user scripts)
- Recommendation: follow the X-3 fallback. Ship validated setup files for sky130A/B, gf180mcuD and sg13g2 in PDK packs.
- Resolution: platform — setup files run in the sandboxed Tcl on every platform; if App Review rejects user scripts, iPadOS uses the validated setup files bundled in curated PDK packs (X-3, hefesto-scripting, 2026-11-07).

### Q-LVS-28: Compatibility switch for netgen quirks
- Kind: ambiguity
- Magic behavior: n/a. This is a Hefesto policy question.
- Evidence: decision X-2; capability `hefesto-magic-compat-mode`
- Options: reuse `magic-compat` | add a separate `netgen-compat` setting (default ON)
- Recommendation: a separate `netgen-compat` setting, so LVS fidelity can be toggled independently of layout output.
- Resolution: decided by D-2 (2026-11-06): a separate `netgen-compat` project setting, default ON, defined in capability `hefesto-magic-compat-mode` next to `magic-compat` (X-2 amendment).

### Q-LVS-29: `equate classes -unique` uses a random suffix
- Kind: quirk
- Magic behavior: a fresh class identity is made by hashing the name plus a pseudo-random uppercase letter. The letter depends on the generator state, which depends on everything compared before.
- Evidence: base/netcmp.c:7290-7312
- Options: replicate | fix (a deterministic unique identity)
- Recommendation: fix. The identity is not observable except through collisions (see Q-LVS-4).

### Q-LVS-30: How the two-team clean-room process applies to the `lvs-*` specs
- Kind: platform
- Status: pending user decision (proposed 2026-11-22 by the M9 plan, `openspec/changes/m9-lvs-simulation` design D2)
- Magic behavior: n/a, because this is a Hefesto process question. The header of this file says that the entries' "Evidence" cites netgen source, so the eight `lvs-*` specs were written by reading GPL source. netgen's `Copying` file is GNU GPL version 1 text. The approved requirement "Two-team clean-room process for specs written from GPL source" in `hefesto-licensing` already applies: it covers any Hefesto spec "written by reading GPL source". Its "Today this covers" list, its `(xschem ref: ...)` rule and its scenarios name only xschem, so the question is how it is applied to netgen, not whether.
- Evidence: this file's header; `openspec/specs/hefesto-licensing/spec.md` (Requirements "Clean-room rule for GPL-sourced behavior" and "Two-team clean-room process for specs written from GPL source"); `docs/contributing/cleanroom.md`; `docs/decisions/m8-decision-table.md` (J-1)
- Options:
  - (a) Apply the M8 process to LVS as it stands:
    - a spec team and an implementation team (roles, not names, for agents);
    - a MODIFIED delta of the requirement that adds the eight `lvs-*` capabilities and this file to its list, extends the source-trace rule to `(netgen ref: ...)` and adds netgen scenarios;
    - a roster rule: a session that read netgen source does not work on the `lvs` command (LVSCMD) or on schematic-driven LVS (SCHEMSIM task 16.3);
    - a paraphrase review of the eight `lvs-*` specs and this file before any LVS implementation starts;
    - `check_cleanroom.py` extended to detect netgen's GPL header and `netgen ref:` traces;
    - golden inputs authored by Hefesto only.
  - (b) Rewrite the `lvs-*` specs using only netgen's public reference manual and black-box runs, so that the requirement no longer applies to them.
  - A single team working from the specs is not an option: it would break the approved requirement.
- Recommendation: (a). The LVS specs were written from source in the same way as the schematic specs, and the M8 process and checks already exist. (b) would discard work that has already been reviewed.

### Q-LVS-31: The IHP netgen setup file is `ihp-sg13g2_setup.tcl`
- Kind: doc-mismatch
- Status: pending user decision (proposed 2026-11-22 by the M9 plan)
- Magic behavior: n/a (PDK data).
  - IHP-Open-PDK @ `5e6d592e` installs `ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl`.
  - The "Purpose" section of `lvs-setup-file` names `sg13g2_setup.tcl` instead, and `tests/golden/LVS/CASES.md` refers to an "sg13g2" setup file.
  - The bare-name rule (`<PDK>_setup.tcl`) in the `lvs-command` requirement "Setup file and input resolution" finds the file only when the PDK name is `ihp-sg13g2`. That is the name the PDK manager uses.
- Evidence: GitHub contents API for `IHP-GmbH/IHP-Open-PDK` `ihp-sg13g2/libs.tech/netgen` @ `5e6d592e4002946a4616f798c357f0f3c06cf3b6`; `openspec/specs/lvs-setup-file/spec.md` (Purpose); `openspec/specs/lvs-command/spec.md` (Requirement "Setup file and input resolution")
- Options: (a) correct the spec and golden texts to `ihp-sg13g2_setup.tcl`, and add a bare-name scenario for `ihp-sg13g2`; (b) also accept `sg13g2` as an alias of the PDK name when looking up the setup file; (c) leave the text as it is.
- Recommendation: (a). The file name comes from the PDK. An alias would be a name that exists only in Hefesto and that netgen users do not type.

### Q-LVS-32: Layout side of `lvs -layout` extracts incrementally
- Kind: ambiguity
- Status: pending user decision (proposed 2026-11-22 by the M9 plan)
- Magic behavior: n/a.
  - Step 1 of the `lvs-command` requirement "Layout netlist from extraction" extracts "as `extract all` would".
  - Design D18 of the M7 plan gives M9 `extract::ensure_extracted` instead. It re-extracts only stale or missing cells and their parents, as Magic's `extract` does. Magic's `extract all` ignores timestamps.
- Evidence: `openspec/specs/lvs-command/spec.md` (Requirement "Layout netlist from extraction"); `openspec/changes/m7-extraction/design.md` D18; Magic `extract/ExtMain.c:284-299`, `:360-384`
  - Magic's incremental check (`extTimestampMisMatch`, `extract/ExtMain.c:968-990`) compares only the `timestamp` line of the `.ext` file with the cell's timestamp. A `.ext` file written under another extract style or technology therefore looks current, although its `tech` and `style` lines (`extract/ExtCell.c:573-582`) say otherwise.
- Options: (a) extract incrementally with `ensure_extracted`, treating a `.ext` file as stale when it is missing, when its timestamp differs, or when its `tech` or `style` line differs from the current ones (an open item against M7's `ensure_extracted`); the netlist equals the `extract all` netlist whenever the `.ext` files are current; (b) always run `extract all`; (c) a setting, defaulting to incremental.
- Recommendation: (a). For an up-to-date hierarchy the result is the same, and LVS reruns on large layouts become fast. The header check closes the one case in which Magic's timestamp check would accept a file that `extract all` would not produce. A user who wants a full re-extraction can run `extract all` first.

### Q-LVS-33: Name map from the layout netlist back to layout objects
- Kind: platform
- Status: pending user decision (proposed 2026-11-22 by the M9 plan)
- Magic behavior: n/a.
  - Three requirements need a map from every device instance and net name in the generated SPICE back to the layout objects: "Layout netlist from extraction" and "Cross-probing" in `lvs-command`, and "Net name map" in `simulation-netlist-preparation`.
  - M7's `netlist::spice::render` returns only the text (`src/core/include/hefesto/netlist/spice/spice.hpp:44-45`).
  - M8's `schem::netlist` returns no map of instances or nets (`src/core/include/hefesto/schem/net/netlist.hpp:99-107`).
  - M7 design D18 makes any change to these entry points after M7 exits an open item for M9.
- Evidence: the files above; `openspec/changes/m7-extraction/design.md` D18 ("Stability")
- Options:
  - (a) Add entry points without changing the existing ones:
    - `netlist::spice::render_with_map`, and a `NameMap` field in `schem::NetlistResult`;
    - the map takes each net name to a hierarchical node with one layout point or schematic wire, and each device name to a use path and device location or to a schematic instance;
    - it is filled by the code that already chooses the names;
    - the existing `render` stays unchanged.
  - (b) M9 rebuilds the map by flattening the `.ext` files again with `extflat` and matching the names.
  - (c) Parse the names back out of the SPICE text.
- Recommendation: (a). The netlister is the only place that knows why a name was chosen (renumbering, `global` merges, case collisions). (b) would duplicate that logic, and (c) cannot recover positions.

### Q-LVS-34: LVS threading
- Kind: platform
- Status: pending user decision (proposed 2026-11-22 by the M9 plan)
- Magic behavior: netgen compares one cell pair at a time on one thread. The `lvs-command` requirement "Execution model" requires a background worker. The `hefesto-architecture` requirement "Concurrency model" requires parallel results to equal single-threaded results.
- Evidence: `openspec/specs/lvs-command/spec.md` (Requirement "Execution model"); `openspec/specs/hefesto-architecture/spec.md` (Requirement "Concurrency model"); netgen reference manual (http://opencircuitdesign.com/netgen/reference.html)
- Options:
  - (a) One background worker per run. Each pair is compared single-threaded in queue order. The comparison state holds no globals, so pairs can run in parallel later.
  - (b) Compare the independent pairs of one hierarchy level in parallel, and write the reports in queue order.
  - (c) Parallel refinement inside a pair.
- Recommendation: (a) in M9. The report order and the pseudo-random choices of Q-LVS-4 and Q-LVS-29 stay deterministic without extra work. (b) is a later optimisation that needs no format change.

### Q-LVS-35: LVS performance target
- Kind: platform
- Status: pending user decision (proposed 2026-11-22 by the M9 plan)
- Magic behavior: n/a. The `hefesto-performance` requirement "Batch throughput relative to Magic" has no LVS line, and netgen is the reference tool for LVS.
- Evidence: `openspec/specs/hefesto-performance/spec.md` (Requirement "Batch throughput relative to Magic"); `tools/golden/container/Containerfile` (netgen is built in the capture image)
- Options:
  - (a) Callgrind instruction counts on the x86_64 runner:
    - at most netgen 1.5.327's count on each of three reference workloads: the `sky130_fd_sc_hd` library cell by cell, a 50k-device flat block, and a hierarchical 1M-device block;
    - after that, at most the recorded baseline + 2 %;
    - netgen is only run as a subprocess in the benchmark job.
  - (b) Wall time against netgen, reported for information only.
  - (c) Absolute wall-time targets on the M3 Pro.
- Recommendation: (a). The requirement already uses this method for DRC, GDS and extraction. Running the netgen binary to measure it does not involve its source.

### Q-LVS-36: LVS input and run limits
- Kind: bug
- Status: pending user decision (proposed 2026-11-22 by the M9 plan)
- Magic behavior: the netgen manual states no limits. Deep `.include` chains, very long continuation lines and deep hierarchies are bounded only by memory and stack. A crash or hang must be fixed in both `netgen-compat` modes (X-2 tier 1).
- Evidence: `openspec/specs/lvs-netlist-input/spec.md` (Requirements "SPICE lexical structure" and "Globals, includes and parameters"); `openspec/specs/lvs-hierarchy/spec.md`; decision X-2
- Options:
  - (a) Fixed limits in both modes. Each has its own diagnostic, and exceeding one ends the run as `error`:
    - include depth 64;
    - one logical line (after continuations) 16 MiB;
    - one name 4 KiB;
    - hierarchy depth 256, with iterative traversal and no recursion;
    - 50 million devices per run.
  - (b) Memory-only limits: the run fails when an allocation fails.
  - (c) Configurable limits, with (a)'s values as defaults.
- Recommendation: (c) with (a)'s defaults. Every PDK and golden input is far below these limits, and hostile inputs fail with a message instead of a stack overflow.
