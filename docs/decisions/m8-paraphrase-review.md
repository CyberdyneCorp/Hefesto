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

## Effect on decisions

No `Resolution:` line changed. No expected text of a scenario changed. The rewrites keep the rules that decide output bytes; the golden cases SCHEM-NL-01..23 pin the behaviour independently of the wording (design R2).

## Sign-off

| Role | Session or person | Date | Result |
|---|---|---|---|
| Spec team, first reviewer (findings and rewrites) | spec-team agent session of the M8 gates (branch `m8-gates`) | 2026-11-17 | done |
| Spec team, second reviewer | *pending* | | |

The task's verification needs a second spec-team reviewer to sign this record. Until then task 1.2 stays open.
