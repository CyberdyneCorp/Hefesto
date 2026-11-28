<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2026 The Hefesto Authors -->

# M9 paraphrase review of the LVS specs

Task 2.2 of `openspec/changes/m9-lvs-simulation` (design D2, "Paraphrase review"; gate G1; capability `hefesto-licensing`, Requirement "Two-team clean-room process for specs written from GPL source", rule "Spec review for paraphrase", applied to netgen by Q-LVS-30). The eight `lvs-*` specs and `docs/decisions/open-questions/LVS.md` were written by reading netgen's GPL source. Before the LVS implementation team writes golden inputs (task 3.5) or code, their text must describe behaviour, not code.

This record has both spec-team signatures: the first review (round 1) and, in "Second review (round 2)", an independent second review by a different spec-team session.

## Scope and method

Reviewed: `openspec/specs/lvs-comparison-algorithm`, `lvs-device-reduction` (first, because they describe internal procedures), then `lvs-netlist-input`, `lvs-setup-file`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports`, `lvs-command`, as on `main` at `cbe15a8c`, and every entry of `LVS.md`.

What counts as paraphrase (design D2, as in the M8 precedent `docs/decisions/m8-paraphrase-review.md`):
1. a source identifier (C function, struct field, flag macro, Tcl variable) that is not a file-format name, command, option, property key or message the user sees;
2. the structure of netgen's C or Tcl: its data structures (lists, flags, signature strings, bit sets), its loop and branch structure, its internal state variables;
3. pseudo-code that mirrors C (assignment arrows, step lists that restart a loop).

What does not: command names, options and results (`run converge`, `verify unique`, `equate pins`), message texts (including `Need to initialize data structures first!` and `No device <c> found for Property<Type>()`, which the user sees), property keys and record content that reports print (`M`, `S`, `_tag` with its `+` and `S` markers), report and JSON formats, C-standard notations used only to state a number syntax (`strtod`), and arithmetic that defines an observable result. The `(netgen ref: ...)` traces and the `Evidence:` lines stay: they are the traceability record, and implementers do not follow them (Q-LVS-30; `docs/contributing/cleanroom.md`, implementation rule 3).

Every requirement body and scenario was read. The candidates were also searched mechanically: backticked identifiers, numbered step lists of three or more steps, and the words flag, bit, list, loop, iteration, pointer, field, buffer, array, hash, key, signature, state, variable, `←`.

Rule for each rewrite: no normative behaviour changes, no `Resolution:` line changes, and no requirement heading changes. Where a behavioural statement would have needed a decision or new information, the item is left as it was and listed under "Open items".

## Findings and rewrites

Kinds: 1, 2, 3 as above.

### `lvs-comparison-algorithm`

| ID | Requirement | Original wording (summary) | Kind | Why it is a risk | Rewritten behaviour |
|---|---|---|---|---|---|
| LPR-1 | Comparison graph | "placed in one shared partition structure. Each vertex remembers which circuit it came from." | 2 | Describes netgen's storage (one node list with a circuit field). | Vertices of both circuits are partitioned together; a class can hold vertices of both circuits and each vertex's circuit is known. The vertex order sentence is kept as an order. |
| LPR-2 | Partition legality | "merged into a single illegal class placed first in the class list. This merging SHALL raise the sticky 'bad match' condition." | 2 | "Class list" and "sticky" name a list and a flag variable. | The combined illegal class comes first in class order (the order in which classes are listed and visited); the comparison enters the "bad match" condition whenever illegal classes are combined and stays in it until reset, even if later rounds leave no illegal class. |
| LPR-3 | Iterative refinement | Five numbered steps with assignment pseudo-code `key(d) ← old_key(d) ⊕ Σ …`, `key(n) ← Σ …`, "re-partition". | 2, 3 | Mirrors the loop body of netgen's iteration routine statement by statement. | One explanatory sentence says what a round separates (devices by the classes their pins reach, then nets by the device classes they lead to, weighted by pin labels). The keys are stated as definitions ("the new key of a device d SHALL be its previous key ⊕ the sum, over …"), with the dependency that matters stated as such: net keys are computed after device classes are split, from the new device keys. The arithmetic, the labels and the split rule are unchanged; the `run converge` and exhaustive-mode text is unchanged. |
| LPR-4 | Match verification | "A failed check SHALL set the property-error state"; "The property-error state SHALL be one of three values". | 2 | A state variable. | A failed check is remembered as a property error of the comparison; the remembered property outcome is one of no errors, errors found, or a check that could not compare (Q-LVS-10). The result codes −1, −2, k, 0 stay: they are this spec's labels for the results, mapped to user-visible `verify` results in Requirement "verify command" (see "Reviewed and kept"). |
| LPR-5 | Symmetry resolution | Resolvers (a), (b), (d): devices "grouped under fresh keys", nets "get a shared fresh key", "pair … under a fresh key", the extra devices "return to the original class". | 2 | Describes the mechanism (assigning hash keys) instead of the effect on the partition. | Each matching set becomes a class of its own (keeping equally many devices per circuit, the surplus staying in the original class); the named net pair becomes a class of its own; arbitrary resolution takes the first symmetric class in class order and makes its last circuit-1 and last circuit-2 vertex, in vertex order, a class of their own. Messages and stage order unchanged. |
| LPR-6 | Pseudo-random labels | "netgen uses a Numerical-Recipes `ran2`-style generator scaled to [0, 2^31−1)." | 2 | Names the implementation of an informative detail, which invites reproducing it; not observable. | Dropped: "Which generator netgen uses, and the exact label values, are not observable in results." The determinism and equality properties (the normative part) are unchanged. |
| LPR-7 | Long-running comparisons | "netgen implements this with an interrupt flag checked between rounds." | 1, 2 | Internal flag. | "netgen, too, can be interrupted between rounds." |

### `lvs-device-reduction`

| ID | Requirement | Original wording (summary) | Kind | Why it is a risk | Rewritten behaviour |
|---|---|---|---|---|---|
| LPR-8 | Combination flags | "carry two flags"; a merge rule is "a set of bits"; "two legacy 'critical' bits". | 2 | A bit-set representation. | Two combination settings per class; a merge rule is any combination of the six named settings, plus two legacy critical settings. Heading kept (it reads as plain English and tests tag it). |
| LPR-9 | Reduction schedule | "in this exact loop": four numbered steps with "repeat from step 2". | 2, 3 | Transcribes the control flow of the reduction driver. | Stated as alternation: one parallel pass, then a series phase (series passes up to the first that merges nothing); reduction ends after a series phase that merges nothing at all, or after a parallel pass (other than the first) that merges nothing. Equivalent case by case to the old steps; the scenario is unchanged. |
| LPR-10 | Parallel combination | "Signature": class name plus `_<net number>` per pin, with `_nc` for a no-connect; "a second signature SHALL also be registered"; "flagged no-connect"; merging steps "appended to the earlier device's list". | 1, 2 | The signature string is netgen's internal hash key, and `_nc` an internal token; "registered", "flagged" and "list" describe data structures. | "Identical connections": same class and, pin by pin, the same net, where all no-connect nets (under the three unchanged conditions) count as one net; with permutable pins, also identical after the first declared pair's nets are exchanged on one device. Result of a merge: the later device removed, the earlier keeps its records followed by the later's, a device without records contributes `{M = 1}`, and the `_tag` series-group markers are maintained so that each device's series groups stay identifiable. Scenarios unchanged. See open item OI-2. |
| LPR-11 | Series combination | "Chains. References in other candidate nets to the removed device SHALL be redirected … A candidate net … SHALL be dropped." | 2 | A candidate list and pointer redirection. | Within one pass a merged device takes part in later merges in place of the two it replaced, so a chain reduces in one pass; a net whose two pins come to be on one device is not merged. The merge conditions, the result and the `_tag` rule are unchanged. |

### Other specs

| ID | Spec, requirement | Original wording (summary) | Kind | Why it is a risk | Rewritten behaviour |
|---|---|---|---|---|---|
| LPR-12 | `lvs-netlist-input` "Netlist database model" | "flags: top, placeholder, case-insensitive, duplicate, matched, verilog, series-combinable, no-parallel" | 1, 2 | The list of netgen's cell flag macros, in their order. | "its flags, which say whether it is a file's top cell, a placeholder, read case-insensitively, a renamed duplicate, paired for comparison, read from Verilog, series-combinable, or excluded from parallel combination", each pointing at the requirement that defines it; one sentence adds that the model states what the engine represents, not how it is stored. |
| LPR-13 | `lvs-netlist-input` "Name matching and case sensitivity" | "Hash keys used to find cells by name SHALL fold only letters, not brackets" | 2 | Names the hash table. | "Looking up a cell by name SHALL fold letter case only, not brackets (see Q-LVS-3)." Same meaning; see open item OI-1. |
| LPR-14 | `lvs-property-matching` "Series/parallel network reconciliation" | "Reconciliation loop … Each iteration … restart the loop"; "the absorbed records are marked with multiplier 0 and removed". | 2, 3 | Loop wording and an internal deletion marker. | "Reconciliation rounds": rounds end after a round that changes nothing; within a round the six items keep their order (it decides the final records, which reports print), and "the round ends here and a new round starts" replaces "restart the loop"; absorbed records "are removed"; Completion counts rounds. |
| LPR-15 | `lvs-property-matching`, same requirement, "Missing values" | "netgen reads the wrong field (see Q-LVS-18)" | 2 | A struct field. | "netgen uses a meaningless value instead of the definition's default (see Q-LVS-18)". |
| LPR-16 | `lvs-setup-file` "property series and parallel controls" | "replaces other series bits"; "`merge` SHALL replace all merge bits of the key". | 2 | Bit-set representation (as LPR-8). | "replaces the other series settings"; "`merge` SHALL replace the whole merge rule of the key". |
| LPR-17 | `lvs-hierarchy` "lvs per-pair flow" | Eight numbered steps transcribing netgen's `lvs` Tcl procedure, with its variables `equatePins`, `forceMatch`, `checkFlatten`, `doFlatten`, `childMismatch`, `pinMismatch` and `u ← verify unique`. | 1, 2, 3 | Tcl variable names and the procedure's branch structure. | Stated as outcomes and their consequences: **Outcome** (not equivalent; or u = `verify unique` after `run resolve`, with the three log lines and `verify nodes` for u = 0, and the property-error list for u = −3, −4); **Mismatched pair** (not equivalent or u ∈ {0, −2, −4}; for a non-top pair, flattened unless a cell is in the no-flatten list, otherwise the black-boxed message, failed-subcell list and flatten prohibition); **Pin matching** (runs for the top pair, every equivalent pair with u ≠ 0 and every black-boxed pair; forced for u = −1 and black-boxed pairs; result ≥ 0 runs `equate classes`; 0 on the top pair is a top-level pin mismatch; any other result ≥ 0 cancels flattening); **Flattening**. Log order is stated as the order of the items. Checked case by case against the old steps (top or not; equivalent or not; each u; in or out of the no-flatten list); both scenarios are unchanged. |

### `LVS.md` ("Magic behavior" fields only)

`Resolution:` lines, `Options:` and `Evidence:` lines are unchanged.

| ID | Entry | Original wording (summary) | Kind | Rewritten behaviour |
|---|---|---|---|---|
| LPR-18 | Q-LVS-3 | "the case-insensitive name hash folds only letters … hash lookups miss. Both fold tables are indexed by a plain `char` … the hash table has 128 entries and the match table has 256 but is indexed negatively." | 2 | Finding a cell by name folds only letter case, so a lookup by `a<1>` does not find `a[1]`; on signed-`char` platforms a name with a byte ≥ 0x80 is folded by reading outside netgen's fold tables (undefined behaviour: wrong matches or a crash). |
| LPR-19 | Q-LVS-4 | "(a `ran2`-style generator)" | 2 | Replaced by a pointer to `lvs-comparison-algorithm` "Iterative refinement" (as LPR-6). |
| LPR-20 | Q-LVS-5 | "frees every pair but does not clear the list head. Later use reads freed memory." | 2 | After `permute forget <cell>`, a later use of that cell's permutations reads released memory (use after free: wrong results or a crash). |
| LPR-21 | Q-LVS-8 | "the property-record sort comparators return only 1 or 0, never −1, and are passed to `qsort`" | 1, 2 | The sort of property records does not define a consistent order (its comparison never answers "less than"), so the order depends on the C library's sort algorithm. |
| LPR-22 | Q-LVS-9 | "uninitialized pointers are used" | 2 | "netgen's behaviour is undefined (uninitialised data)". |
| LPR-23 | Q-LVS-10 | "sets the property state to 'failure' (−1)" | 1 | "is recorded as a property check that could not compare". |
| LPR-24 | Q-LVS-16 | "this writes past the line buffer" | 2 | "a long right-column entry overruns the line netgen builds (memory corruption or a crash)". |
| LPR-25 | Q-LVS-18 | "the default is read from the integer field of the definition instead of the double field" | 2 | netgen does not use the definition's double default but a meaningless value (the definition read as an integer). |
| LPR-26 | Q-LVS-22 | "uses an uninitialized option index" | 1 | "behaves unpredictably (the option is left uninitialised)". |
| LPR-27 | Q-LVS-23 | "the 'port connection errors' pass writes into the net-correspondence array after it has been freed" | 2 | After the ports are paired, netgen's check for port connection errors writes to memory it has already released (use after free). |
| LPR-28 | Q-LVS-24 | "Its test treats circuit 2 as a placeholder whenever it has *any* flag set" | 1, 2 | netgen applies the relaxation whenever circuit 2's cell has any of its cell attributes set (`lvs-netlist-input` "Netlist database model"), not only when it is a placeholder. |
| LPR-29 | Q-LVS-26 | "treated as the same net (`_nc`)" | 1 | The internal token is dropped (as LPR-10). |

## Reviewed and kept

| Where | Why it stays |
|---|---|
| `lvs-comparison-algorithm` "Class identity and pin labels" ("a 64-bit hash of its name") and the two pin-label rules | The identity is named as a hash because collisions are part of the stated behaviour (Q-LVS-4, Q-LVS-29); no code structure is described. The pin-label rules are equality constraints, not an algorithm. |
| `lvs-comparison-algorithm` "Initial partition" (initial keys: class identity, fanout) and the key arithmetic of "Iterative refinement" | Arithmetic that defines the exact partition, which is observable (`exact`: identical classification and partition, per-round state through `iterate`, `print` and `symmetries`). As in M8's kept device-model hash, it cannot be stated more simply without losing the collision behaviour; it is stated as definitions, not as code (LPR-3). |
| `lvs-comparison-algorithm` result codes −1, −2, k, 0 in "Match verification", "Symmetry resolution", "verify command" | The spec's labels for the four results; each is mapped to user-visible `verify` results and messages in the same spec and to `uresult` in `lvs-reports` "Result classification". |
| `lvs-comparison-algorithm` "Reset and stepping commands", "verify command", "Forced equivalences" | Command syntax, results and message texts (including `Need to initialize data structures first!`). |
| `lvs-device-reduction` "Series combination" merge conditions and `_tag` marker rule | Observable: the merged records and their `_tag` values appear in property reports and network dumps; the scenario pins them. |
| `lvs-device-reduction` "Zero-valued device removal" ("not the top of the compare queue") | The compare queue is user-visible (`compare hierarchical`, `print queue`). |
| `lvs-hierarchy` "Compare queue construction", "Preparing a cell pair", "Pre-match flattening", "Flattening semantics", "Duplicate and shorted pins", "Final result" (numbered lists; "matched") | Each list is the order of observable output or precedence; "matched" is the plain state "paired in the queue", visible through which cells are flattened. |
| `lvs-netlist-input` "SI value parsing" ("C `strtod` syntax", extended precision), "Name matching" fold table, "Format selection" (`ReadNetlist: don't know type of file '<f>'`) | Number syntax by reference to the C standard; the fold table defines observable equality; the message is user-visible text. |
| `lvs-setup-file` (all), including `No device <c> found for Property<Type>()` | Command syntax and messages. |
| `lvs-property-matching` sort rule, comb folding, tolerance formulas | Arithmetic and ordering that decide the reported records. |
| `lvs-reports`, `lvs-command` | No finding: output formats, list/JSON shapes and command behaviour, described by what is written and returned. |
| `LVS.md` Q-LVS-6 ("the *key text* is a number"), Q-LVS-12 ("passed to the logger as the format string"), Q-LVS-17 ("returns only 2, 1 or 0"), Q-LVS-25 ("without its two count arguments"), Q-LVS-29 ("hashing the name plus a pseudo-random uppercase letter") | Each states an observable effect or a defect class in general terms (format-string misuse, missing arguments), not netgen's code; Q-LVS-17's values are `equate pins` results. |
| `Evidence:` lines and `(netgen ref: ...)` traces | Traceability, which implementers do not follow (Q-LVS-30). |

## Open items (not rewritten)

- **OI-1. `lvs-netlist-input` "Name matching and case sensitivity" against Q-LVS-3's Resolution.** The requirement says cell lookup by name folds letter case only, not brackets (netgen's behaviour), with no `netgen-compat` split. Q-LVS-3's Resolution is "fix … fold brackets in the case-insensitive hash too" as X-2 tier 1, which applies in both modes. Aligning the requirement with the Resolution changes normative text, so it is left for the user (listed in the round report) and a spec-team change; golden cases NG-NAME-02 and NG-NAME-03 measure it. Closed in the round-1 fix pass (2026-11-25) without a new user decision, because Q-LVS-3 is already decided: the requirement now says a lookup uses the same equality, brackets included, with scenario "Lookup folds brackets" (test `netlist_db_test.cpp` "lookup keys fold brackets as equality does").
- **OI-2. Series-group markers written by a parallel merge.** (Round 3: the spec team's check at the spec pin found that netgen adds group-open and group-close markers on a parallel merge when series markers are present, and the implementation adds none; item R3-7 row b of `m9-decision-table.md`, for the user. Still open.) `lvs-device-reduction` "Parallel combination" item 4 says only that the `_tag` markers are maintained so that series groups stay identifiable; it does not say which marker characters a parallel merge adds to which records. The topology string of `lvs-property-matching` depends on them. This is a fidelity gap, not paraphrase; stating it is new normative text. Proposed for task 3.8 (capture-fixed texts) with an NG-RED case that merges a series chain in parallel with another device, or for the second reviewer.
- **OI-3. `Resolution:` lines with code wording.** Q-LVS-3 ("index the fold tables by unsigned byte", "the case-insensitive hash"), Q-LVS-5 ("clear the pair list"), Q-LVS-16 ("buffer overflow"), Q-LVS-22 ("uninitialised option index"), Q-LVS-23 ("the freed net-correspondence array"), Q-LVS-24 ("the logical-AND placeholder test", "the placeholder flag"), Q-LVS-25 ("missing printf arguments"). They name defect classes and are short, but a few name netgen's structures. As with Q-SCHEM-20 in M8, a rewording needs the user's approval; proposed behavioural wordings, same meaning: Q-LVS-3 "fix — names with bytes ≥ 0x80 are folded without out-of-range reads, and finding a cell by name folds brackets as name equality does"; Q-LVS-5 "fix — after `permute forget`, the cell has no permutations and no released memory is used"; Q-LVS-23 "fix — the check for port connection errors uses no released memory; printed output is unchanged"; Q-LVS-24 "compat — the relaxation applies whenever circuit 2's cell has any attribute set, behind `netgen-compat`; with `netgen-compat` OFF, only to placeholder cells". Not applied. **Closed 2026-11-27:** the user approved the four proposed wordings on 2026-11-27 (`m9-decision-table.md` R3-4); they replace the `Resolution:` lines of Q-LVS-3, -5, -23 and -24 in `LVS.md` verbatim, each keeping its decision date and adding "reworded 2026-11-27, OI-3", with a `Status:` line naming the amendment. The decided behaviour is unchanged (Q-LVS-3, -5, -23 fix in both modes; Q-LVS-24 compat), so no test, scenario or golden case changes. Q-LVS-16, -22 and -25 were named as candidates but had no proposed wording and keep their lines; the entry headings named under "Left as they are" in the second review also stay.

## Effect on decisions

No `Resolution:` line changed. No requirement heading changed, so no `[req:<heading>]` test tag needs retagging. No scenario's expected text changed. The rewrites keep the rules that decide results and output; the NG-ALG and NG-RED golden cases pin the behaviour independently of the wording (design R3).

## Source consulted in this review

This session is on the LVS spec team (Q-LVS-30). To check OI-2 it read one netgen file, `base/netgen.c` at the spec pin `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, through a single web fetch of the raw file. Nothing was cloned, downloaded or saved to disk. The answer is recorded above only as the existence of a gap, not as netgen's code.

## Second review (round 2)

This section is the second spec-team signature. It was made by a session of the LVS spec team that did not take part in the first review (round 1, branch `m9-round1-lvsspec`), on branch `m9-round2-decide`, 2026-11-26. It reviewed the eight `lvs-*` specs and every entry of `LVS.md` as on `main` at `6e48a02f`, again starting with `lvs-comparison-algorithm` and `lvs-device-reduction`, and repeated the mechanical search above on its own before reading the first review's tables. It applied the same three kinds and the same rule for each rewrite (no normative change, no heading, scenario or `Resolution:` change).

### Concurrence with LPR-1 to LPR-29

Concurs with all 29. Each rewrite was checked against the original kind and against the rule; none changes a result, a message, a scenario or a decision:
- LPR-1 to LPR-7 (`lvs-comparison-algorithm`): the refinement keys are stated as definitions, which is the least that keeps the exact partition observable; "Partition legality", "Match verification" and "Symmetry resolution" describe the partition, not storage. Concur.
- LPR-8 to LPR-11 (`lvs-device-reduction`): the reduction schedule as alternation was checked case by case against the old step list (first parallel pass merges nothing; a series phase that merges nothing; a later parallel pass that merges nothing) and is equivalent; "identical connections" keeps the three no-connect conditions. Concur.
- LPR-12 to LPR-17: concur; LPR-17 was checked against both `lvs-hierarchy` scenarios.
- LPR-18 to LPR-29 (`LVS.md` "Magic behavior" fields): concur. They describe defects by effect (out-of-range read, use after free, undefined data), which is enough for an implementer to avoid them without netgen's code.
- "Reviewed and kept": concur with every row, in particular the key arithmetic of "Iterative refinement" and the `_tag` marker rule, which are observable.
- OI-1 (closed in round 1) and OI-2: concur. OI-3: the proposed wordings mean the same as the recorded `Resolution:` lines; this review adds no wording of its own and does not apply them, because a `Resolution:` change needs the user (still pending).

### New findings

Residues of the patterns the first review rewrote elsewhere (cell attributes as "flags", hashing as storage), found by the independent search. Kinds as above.

| ID | Where | Original wording (summary) | Kind | Why it is a risk | Rewritten behaviour |
|---|---|---|---|---|---|
| LPR-30 | `lvs-netlist-input` "SPICE subcircuit calls and placeholders", "Undefined cells" | "the placeholder flag" | 1, 2 | The name of a cell flag (as LPR-12). | "marked as a placeholder (Requirement "Netlist database model")". |
| LPR-31 | `lvs-netlist-input` "Structural Verilog input" | "a placeholder with the verilog flag" | 1, 2 | The name of a cell flag (as LPR-12). | "a placeholder marked as read from Verilog". |
| LPR-32 | `lvs-netlist-input` "Name matching and case sensitivity" | "A null name SHALL never be equal to anything." | 1 | "null" is the C null pointer, not a name the user sees. | "An absent name SHALL never be equal to any name, not even to another absent name." Same meaning: "never equal to anything" already excluded two absent names. |
| LPR-33 | `lvs-device-reduction` Purpose | "the per-class enable flags for combination"; "the commands that set the flags" | 2 | Residue of LPR-8's bit-set wording in the Purpose. | "the per-class settings that allow combination"; "the commands that change these settings". |
| LPR-34 | `LVS.md` Q-LVS-9 | "only the first declared pin permutation produces an alternate parallel signature" | 2 | The parallel "signature" is netgen's internal key, removed from the spec by LPR-10. | "when looking for parallel devices, netgen tries only the first declared pin permutation, so devices that are parallel only under another permutable pair are not merged". The heading (`Parallel signature uses only the first permutable pair`), `Options:` and `Resolution:` lines are unchanged. |
| LPR-35 | `LVS.md` Q-LVS-2 | "orders come from internal lists that are reshuffled by refinement, or from hash iteration" | 2 | Names netgen's lists and hash-table iteration. | "several orders are not stable: they change as refinement splits classes, or they follow from how names hash". |
| LPR-36 | `LVS.md` Q-LVS-11 | "instance counts adjusted in hash order" | 2 | Hash-table iteration order. | "instance counts adjusted in an order that follows how names hash". |

Left as they are: the entry headings of Q-LVS-5 ("dangling pair list"), Q-LVS-8 ("sort comparators"), Q-LVS-9 ("Parallel signature") and Q-LVS-24 ("logical instead of bitwise AND"), because headings are not changed in this review; with OI-3 they are candidates for the user's rewording. The `Options:` lines of Q-LVS-9 ("generate signatures") and Q-LVS-24 ("test the placeholder flag") likewise stay, as the first review decided for `Options:` lines.

### New open item

- **OI-4. The value printed as `(<flag>)`.** `lvs-property-matching` "When properties are checked" prints `Circuit1 instance <i> pin <n> connections are tied together|no connects (<flag>)`. The message is user-visible and stays, but the spec does not say what `<flag>` prints. Stating it is new normative text, so it is a fidelity gap for task 3.8 (capture-fixed texts), with an NG-PROP case whose parallel instances disagree on a no-connect pin, not a paraphrase rewrite. **Round 3 (2026-11-27):** the LVS spec team checked the value against netgen at the spec pin (a pinned raw web fetch of `base/netcmp.c`, nothing saved): netgen prints `0` for pins tied together and a non-zero value for no connects, while the implementation prints `1` for no connects and `2` for tied together. The spec is left unchanged and the difference is item R3-7 row d of `m9-decision-table.md`, for the user. OI-4 stays open until then; the exact non-zero value is for the NG-PROP capture of task 3.8.

### Source consulted in this review

This session is on the LVS spec team (Q-LVS-30) and never edits `src/core/**/lvs/**`, `tests/unit/lvs/**` or the inputs of `tests/golden/LVS/**`. It read one netgen file, for the header re-check of task 2.3: `base/netgen.c` at `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, through one web fetch of the pinned raw URL, reading only its header lines. Nothing was cloned, downloaded or saved to disk. The paraphrase review itself used no netgen source.

## Round 3 (2026-11-27)

OI-3 is applied and closed (user approval of 2026-11-27). OI-2 and OI-4 stay open as R3-7 rows b and d of `m9-decision-table.md`. Source consulted: the LVS spec-team session of branch `m9-round3-decide` fetched the pinned raw URLs of `base/netcmp.c`, `base/netgen.c`, `base/flatten.c` and `base/spice.c` at `93dcda14d36349d0b0ca63168d3cbbab20fe2ece`, asking only for observable behaviour (printed texts, which net survives, which records result). Nothing was cloned, downloaded or saved to disk. What it learned is recorded only as behaviour, in the specs and in R3-7.

## Sign-off

| Role | Session or person | Date | Result |
|---|---|---|---|
| LVS spec team, first reviewer (findings and rewrites) | LVS spec-team agent session of M9 round 1 (branch `m9-round1-lvsspec`) | 2026-11-25 | done: 29 findings rewritten (LPR-1 to LPR-29), 3 open items |
| LVS spec team, second reviewer | LVS spec-team agent session of M9 round 2 (branch `m9-round2-decide`), not the first reviewer | 2026-11-26 | signed: concurs with LPR-1 to LPR-29, OI-1 and OI-2; 7 new findings rewritten (LPR-30 to LPR-36); new open item OI-4; OI-3 left for the user |
| LVS spec team, round 3 (closing OI-3) | LVS spec-team agent session of M9 round 3 (branch `m9-round3-decide`) | 2026-11-27 | OI-3 applied after the user's approval of 2026-11-27 and closed; OI-2 and OI-4 checked against netgen and raised to the user (R3-7 b, d); both still open |
