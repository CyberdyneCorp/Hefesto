# Open Questions: LVS area

These entries cover the capabilities `lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports` and `lvs-command`.

The reference is netgen, not Magic: `RTimothyEdwards/netgen` @ `93dcda14d36349d0b0ca63168d3cbbab20fe2ece` (version 1.5.327). The "Magic behavior" field below therefore describes **netgen behavior**, and "Evidence" cites netgen sources.

Decision X-2 (tiered bug handling) applies to these entries, with the compatibility switch chosen in Q-LVS-28. Contributors resolving these entries SHALL follow the clean-room rule in capability `hefesto-licensing`.

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
- Recommendation: replicate behind the compatibility switch (it changes results). Cover with golden case NG-PROP-07.

### Q-LVS-8: Inconsistent sort comparators
- Kind: bug
- Magic behavior: the property-record sort comparators return only 1 or 0, never −1, and are passed to `qsort`. The resulting order depends on the C library's sort algorithm (glibc merge sort versus BSD/macOS). Series/parallel reconciliation can therefore differ between Linux and macOS netgen builds.
- Evidence: base/netcmp.c:4246-4280, base/netcmp.c:4413, base/netcmp.c:4599
- Options: replicate glibc behavior | fix (stable sort with a proper three-way comparator)
- Recommendation: fix with a stable sort. Capture golden outputs from Linux netgen and note any macOS divergence.

### Q-LVS-9: Parallel signature uses only the first permutable pair
- Kind: quirk
- Magic behavior: only the first declared pin permutation produces an alternate parallel signature, so devices with several permutable pairs may not merge. If the pin names are not found on an instance, uninitialized pointers are used.
- Evidence: base/netgen.c:3335-3384
- Options: replicate | fix (generate signatures for all permutation-equivalent pin orders)
- Recommendation: replicate the single pair behind the compatibility switch, because merging more changes the device counts that are reported. Fix the uninitialized use (tier 1).

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

### Q-LVS-16: Column widths: no validation and right-column overflow
- Kind: bug
- Magic behavior:
  - `format` with a width ≤ 0 reports an error but applies the widths anyway.
  - The right column is truncated at the *left* column width. When w2 < w1 this writes past the line buffer.
- Evidence: tcltk/tclnetgen.c:2073-2082, base/netcmp.c:1539-1545, base/netcmp.c:1582-1590
- Options: fix (reject widths ≤ 0; truncate the right column at its own width)
- Recommendation: fix (tier 1). With default and equal widths the output is identical.

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
- Recommendation: replicate behind the compatibility switch. The structured result always carries the status.

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

### Q-LVS-23: Pin matching writes to freed memory
- Kind: bug
- Magic behavior: after port pairing, the "port connection errors" pass writes into the net-correspondence array after it has been freed.
- Evidence: base/netcmp.c:8144, base/netcmp.c:8409
- Options: fix
- Recommendation: fix (tier 1). It has no effect on printed output.

### Q-LVS-24: Placeholder test uses logical instead of bitwise AND
- Kind: bug
- Magic behavior: the size-1-array pin-name equivalence is meant for placeholder cells. Its test treats circuit 2 as a placeholder whenever it has *any* flag set, so the relaxation applies far more often than intended.
- Evidence: base/netcmp.c:7935
- Options: replicate | fix (test the placeholder flag)
- Recommendation: replicate behind the compatibility switch, because it changes pin pairing results. Measure with golden case NG-PIN-06.

### Q-LVS-25: Orphan-net summary printed with missing arguments
- Kind: bug
- Magic behavior: the console summary line for orphan nets is printed without its two count arguments (undefined behavior).
- Evidence: base/netcmp.c:540
- Options: fix
- Recommendation: fix (tier 1). It is console-only and not part of `comp.out`.

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

### Q-LVS-28: Compatibility switch for netgen quirks
- Kind: ambiguity
- Magic behavior: n/a. This is a Hefesto policy question.
- Evidence: decision X-2; capability `hefesto-magic-compat-mode`
- Options: reuse `magic-compat` | add a separate `netgen-compat` setting (default ON)
- Recommendation: a separate `netgen-compat` setting, so LVS fidelity can be toggled independently of layout output.

### Q-LVS-29: `equate classes -unique` uses a random suffix
- Kind: quirk
- Magic behavior: a fresh class identity is made by hashing the name plus a pseudo-random uppercase letter. The letter depends on the generator state, which depends on everything compared before.
- Evidence: base/netcmp.c:7290-7312
- Options: replicate | fix (a deterministic unique identity)
- Recommendation: fix. The identity is not observable except through collisions (see Q-LVS-4).
