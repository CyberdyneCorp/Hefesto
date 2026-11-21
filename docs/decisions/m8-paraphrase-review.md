<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright 2026 The Hefesto Authors -->

# M8 paraphrase review of the schematic specs

Task 1.2 of `openspec/changes/m8-schematic` (design D2, "Paraphrase review"; capability `hefesto-licensing`, Requirement "Two-team clean-room process for specs written from GPL source", rule "Spec review for paraphrase"). The six `schematic-*` specs and `docs/decisions/open-questions/SCHEM.md` were written by reading xschem's GPL source. Before an implementation workstream starts, their text must describe behaviour, not code.

## Scope and method

Reviewed: `openspec/specs/schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration`, `schematic-command` (as on `main` at `f03da5f2`, plus the gf180 re-pin of task 2.1), and every entry of `SCHEM.md`.

What counts as paraphrase (design D2):
1. a source identifier (C function, variable, macro, awk or Tcl variable) that is not a file-format name, attribute, token, command or message the user sees;
2. the structure of one of xschem's awk or Tcl programs: its pass order, its state variables, its internal marker lines;
3. pseudo-code that mirrors C.

What does not: file-format and attribute names (`lvs_format`, `sim_pinnumber`), format-string tokens (`@prop_ptr`, `@spiceprefix`), Tcl procedure names that PDK files call (`ev7`, `ngspice::get_node`), message texts (including their `LINE`/`xRECT` kind words), C format notations used only to state an output format (`%.16g`), and arithmetic that defines an observable order. The `(xschem ref: ...)` traces and the `Evidence:` lines stay: they exist for traceability, and implementers do not follow them (J-1).

Every requirement body and scenario was read; the candidates were also searched mechanically (backticked identifiers, `awk`, numbered step lists of three or more steps, words such as counter, buffer, pointer, variable, hash).

## Findings and rewrites

| ID | Where | Finding | Kind | Action |
|---|---|---|---|---|
| PR-1 | `schematic-netlisting` "Element line post-processing" | Ten numbered steps in the order of xschem's `spice.awk` program, including its internal marker lines (`**** spice_prefix`, `**** end_element`) and its pass structure ("transformed line by line"). | 2 | **Rewritten** as rules on the result: user-code blocks, continuation lines, trailing `m=1`, comment lines, the notations element text may contain (width marker, `##`, `#<p>#`, `PARAM:`), vector elements, parametric vector names, renames, save lines, subcircuit lines. Dependencies that are observable are stated (rules apply to the joined line; trailing `m=1` is removed before a vector element is split; `.param` values come from preceding lines); the step order that is not observable is dropped. `.subckt` is no longer listed among the devices whose name list repeats, because subcircuit lines have their own rule (in the old text that entry could never apply). The scenarios keep their expected texts. |
| PR-2 | `schematic-netlisting` "Line breaking" | The rule was a transcription of `break.awk`: a running column counter `p`, its increments and resets. | 2, 3 | **Rewritten** as a description of where wraps occur: segment length per field, the zero-length first field of a continuation segment, the quote rule with a quote state that persists across lines. The scenario's expected wraps are unchanged (eleventh and twenty-third fields). |
| PR-3 | `schematic-netlisting` "Format string substitution", `@spiceprefix` row, closing paragraph and scenario "Resistor line before post-processing" | `@spiceprefix` wrote the marker line `**** spice_prefix <v>` and every element ended with the marker line `**** end_element`: the internal protocol between xschem's C netlister and its awk post-processor, never present in a netlist. | 2 | **Rewritten**: `@spiceprefix` writes nothing and sets the element's spice prefix; the text one instance produces is one element, and its prefix applies to it only. The scenario, renamed "Resistor element text", drops the marker line. No PDK file of the three data sets contains `spice_prefix` or `end_element` text, or a format that writes a width marker, `##` or `PARAM:` literally (searched 2026-11-17), so PDK netlists do not depend on the markers. |
| PR-4 | `schematic-netlisting` Purpose and the paragraph on traces | "the post-processing that xschem performs with its `spice.awk` and `break.awk` helper scripts" and "`.awk` traces point at xschem's post-processing scripts" describe the program structure. | 2 | **Rewritten**: the Purpose names the rules by what they do; the trace note is dropped (the traces themselves stay). |
| PR-5 | `SCHEM.md` Q-SCHEM-16, "xschem behavior" | "the short report is suppressed while the netlist counter is 0": an internal variable. | 1 | **Rewritten** as when a short is and is not reported. `Resolution:` unchanged. |
| PR-6 | `SCHEM.md` Q-SCHEM-18, "xschem behavior" | "`spice.awk` relies on gawk's `IGNORECASE`": an awk program variable. | 1 | **Rewritten** as the case sensitivity of the two matches under each awk. `Resolution:` unchanged. |
| PR-7 | `SCHEM.md` Q-SCHEM-20, "xschem behavior" | "the quote state of `break.awk` ... the column counter is reset": program state. | 1, 2 | **Rewritten** as the observable wrapping behaviour. `Resolution:` unchanged. |
| PR-8 | `schematic-netlisting` "Element line post-processing", "Notations in element text" | The width marker `?<w>` and the `##`, `#<p>#`, `PARAM:` notations stay as an intermediate text form (the `@pinlist` output of "Format string substitution"), which reads like an internal protocol unless it is said that users write them. | 2 | **Rewritten** (approved by the maintainer, 2026-11-20): one sentence says they are part of the format-string syntax a user may write in a `format` or `lvs_format` attribute, with the same meaning whether a token produced them or the user wrote them. No requirement restructured; no expected text changed. |
| PR-9 | `schematic-netlisting` "Element line post-processing", "Parametric vector names" | PR-1 restricted the `.param` pairs to "the `.param` lines ... that precede an element". xschem collects the pairs of every `.param` line of the whole netlist text before it expands any name (so a `.param` in a user-code block after the device lines, the usual place, applies), includes user-code lines, reads them before the trailing `m=1` is removed, lets a later pair replace an earlier one, and expands matching first fields of user-code lines too. A dependency that changes output was lost in PR-1 (the text before PR-1 said "if any line starts with `.param`"). | fidelity | **Rewritten** as the whole-netlist rule; "User-code blocks" names "Parametric vector names" as the second exception to "written unchanged". No scenario covers it; no expected text changed. |
| PR-10 | `schematic-netlisting` "Element line post-processing", "User-code blocks"; "Line breaking"; "Netlist file structure" item 2 | The header-code begin and end lines (`**** begin user header code` ...) never appear in a netlist: like the PR-3 markers, they are an internal protocol between the netlister and its post-processor ("Netlist file structure" never writes them, and its item 2 still said "no markers in the final text"). Defining user-code blocks by them also made "Line breaking" exempt header code from wrapping, while xschem wraps header code lines (its line breaker runs after the header lines are gone; architecture-code lines are not wrapped). | 2, fidelity | **Rewritten**: the user-code blocks are the header code and the architecture code blocks (the latter delimited by their written begin and end lines); "Line breaking" exempts architecture code blocks and says header code lines wrap like other lines; item 2 says the header code is a user-code block. No scenario covers a header line over 130 characters; no expected text changed. |
| PR-11 | `schematic-netlisting` "Line breaking", quote rule | Not stated: the quote state is also updated by lines that are not wrapped (user code, `.include` lines and the like), and empty lines leave it unchanged. An implementation that skips the no-wrap lines entirely would wrap differently after them. | fidelity | **Rewritten**: one sentence added. The scenario is unchanged. |
| PR-12 | `SCHEM.md` Q-SCHEM-27, "xschem behavior" | "the body comes from an empty load": the internal loading step. | 1 | **Rewritten** as "the body is empty". `Resolution:` unchanged. |

## Reviewed and kept

| Where | Why it stays |
|---|---|
| `schematic-netlisting` "Device models": h = 5381, h × 33 + c mod 2³², bucket = h mod 367 | Defines the observable order of the `device_model` lines as arithmetic, with no code structure; the order cannot be stated more simply. |
| `schematic-netlisting` "Hierarchical parameters": the four lookup steps | Observable precedence of value sources. The phrases "without predefined tokens" and "keeping backslash escapes" are the closest to implementation wording; SCHEM-NL-03 (passgate parameters) pins the behaviour, and task 3.6 corrects the text if a capture contradicts it. |
| `schematic-netlisting` "Bus and vector notation" grammar, "expr evaluation" | A grammar and an expression language: user-visible syntax, stated in EBNF and as operator tables. "converted by a C `%lf` scan" and "`%.15g`" state number syntax and output format by reference to the C standard. |
| `schematic-file-format` "Numeric fields", "Canonical write order" (`%.16g`, `%d`, record templates) | Output format notation; the consequences are listed as examples. |
| `schematic-file-format` "Brace strings", "Property strings", "Record grammar" | File-format grammar, with read and write rules; no program structure beyond what the format defines. |
| `schematic-file-format` "Malformed input": `missing fields for <KIND> object` with `LINE`, `xRECT`, ... | Message text the user sees; the kind words are part of it. |
| `schematic-symbols-and-libraries` "tcleval values", steps 1–5 | The observable semantics of a `tcleval(` value (which characters are replaced, which variables are set, `subst` semantics), not the structure of xschem's code. |
| `schematic-symbols-and-libraries` "Instance names", steps 1–3 | Observable naming of pasted and placed instances; the remembered last index is visible through the names chosen. |
| `schematic-symbols-and-libraries` "Symbol references", steps 1–8 | Observable resolution precedence. |
| `schematic-symbols-and-libraries` "Text substitution tokens", `@prop_ptr` | A token name users write in symbol texts. |
| `schematic-simulation-integration` "Simulator vector names", "Testbench code blocks" | Observable naming and path-resolution rules. |
| `schematic-editing`, `schematic-command` | No finding: editing and command behaviour is described by user actions and results. |
| `SCHEM.md` Q-SCHEM-5 ("sorted with C `qsort`") | Explains why the tie order is unspecified; no code is described. |
| `SCHEM.md` Q-SCHEM-15 (`flatten.awk`) | Names the mechanism and lists observable effects only; the Resolution makes flat output behavioural. |
| `SCHEM.md` Q-SCHEM-8 ("defined in xschem's Tcl code") | States where the procedures come from, not how they work; their semantics are task 4.3's, from the public manual. |
| `schematic-netlisting` "Net naming order", "Netlist file structure", "Subcircuit body", "Subcircuit definitions", "Schematic resolution" (numbered lists; added by the second reviewer) | Each list is the order of observable output or of observable precedence. "Subcircuit definitions" states the definition order as a list that grows as definitions are written; that is the simplest statement of the observable order, not a copy of a data structure's code. "depth-first" in "Net naming order" decides only the order of short reports. |
| `schematic-file-format` "Malformed input", `schematic-simulation-integration` "Simulator vector names" step 3 ("SPICE subcircuit-call prefix"), `schematic-symbols-and-libraries` "Text substitution tokens" (`@sch_last_modified` time format `%Y-%m-%d  %H:%M:%S`) (added by the second reviewer) | Output formats and message texts the user sees. |
| `SCHEM.md` `Evidence:` lines (for example Q-SCHEM-3 "default set at the end of the loop body") (added by the second reviewer) | `Evidence:` lines are traceability, which implementers do not follow (J-1); the method above keeps them. |

## Second review (2026-11-20)

The second spec-team reviewer checked the record against the specs on `main` @ `09636348`:
- PR-1 to PR-7: each rewritten text describes behaviour, and no scenario's expected text changed (compared with the diff of commit `f39a172a`). Faithfulness was spot-checked against xschem's source at the pinned commit, read in place with nothing saved to the implementation machine: the line breaker (all of PR-2 and PR-7, including the scenario's wraps before the twelfth and twenty-fourth fields), the post-processor (the order of continuation joining, trailing `m=1` removal, parametric name expansion and the per-line rules; the special device list; renames; save and subcircuit lines; the width-marker formula), the netlister's `@pinlist`, `@spiceprefix`, `@savecurrent` and token-end rules, and the order in which the two post-processing programs run. Verdict: PR-2 to PR-7 agreed; PR-1 agreed except for the `.param` scope (PR-9) and the header-code lines (PR-10).
- "Reviewed and kept": every row agreed; three rows added.
- Mechanical sweep of the six specs and `SCHEM.md` (backticked identifiers, `awk`, numbered lists of three or more items, counter/buffer/pointer/variable/hash and similar words): PR-10 and PR-12 are its findings; PR-9 and PR-11 came from the source check; PR-8 is the maintainer's addition.
- Kind "fidelity" marks a text that described behaviour but not xschem's; it is outside the three paraphrase kinds and is recorded because checking the faithfulness of PR-1 was in scope.

Q-SCHEM-20's `Resolution:` said the line breaker "resets the column counter as xschem does", program-state wording. The second reviewer proposed a behavioural wording with the same meaning; the maintainer approved it on 2026-11-20 and it is applied in `SCHEM.md` and in `docs/decisions/m8-decision-table.md`: "replicate — when wrapping long lines, a quote stays open across lines and the field written after a `+` does not count toward the new line's length, as xschem does, in both modes; only where long lines wrap changes. Golden case SCHEM-NL-12."

## Effect on decisions

No `Resolution:` line changed in meaning; Q-SCHEM-20's was reworded with the maintainer's approval (2026-11-20). No expected text of a scenario changed. The rewrites keep the rules that decide output bytes; the golden cases SCHEM-NL-01..23 pin the behaviour independently of the wording (design R2).

## Sign-off

| Role | Session or person | Date | Result |
|---|---|---|---|
| Spec team, first reviewer (findings and rewrites) | spec-team agent session of the M8 gates (branch `m8-gates`) | 2026-11-17 | done |
| Spec team, second reviewer | second spec-team agent session (branch `m8-paraphrase-review-2`), assigned by the maintainer | 2026-11-20 | done with 5 additional findings (PR-8 to PR-12) |
