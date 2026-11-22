# Diagnostic codes: extract area

Codes `EXTRACT-<TOPIC>-<NNN>` emitted by `src/core/src/extract/` (the extraction-style subset of the `extract` tech section reader that LEF and DEF write read (Open Question 2, decided (a); built to the M7 spec extract-tech-section, which M7 extends);
`openspec/changes/m6-lef-def-oasis/design.md` D1/D2). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/extract/codes.cpp` (owner:
INTEGRATION) from one fragment per workstream, each owned by that
workstream. The rows are sorted while compiling
(`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/extract/tech/codes.inc` | `EXTRACT-STYLE-*` | EXTSUB (M6), EXTTECH (M7) |
| `src/core/src/extract/node/codes.inc` | `EXTRACT-NODE-*` | EXTNODE |
| `src/core/src/extract/file/codes.inc` | `EXTRACT-FILE-*` | EXTNODE |
| `src/core/src/extract/cap/codes.inc` | `EXTRACT-CAP-*` | EXTCAP |
| `src/core/src/extract/device/codes.inc` | `EXTRACT-DEV-*` | EXTDEV |
| `src/core/src/extract/hier/codes.inc` | `EXTRACT-HIER-*` | EXTHIER |
| `src/core/src/extract/command/codes.inc` | `EXTRACT-CMD-*` | EXTCMD |
| `src/core/src/extract/length/codes.inc` | `EXTRACT-LEN-*` | EXTCMD |

The M7 subdirectories (`openspec/changes/m7-extraction/design.md` D2/D3)
were added by the M7 scaffold (tasks.md 3.2) with no code; while M7 is in
progress, an `extract` option or extractor function the M7 scaffold
declared before its workstream implemented it fails with the core code
`CORE-NOTIMPL-008` ([../codes.md](../codes.md)).

Every option and function the M6 scaffold declared is implemented; its stub
code `CORE-NOTIMPL-006` is retired ([../codes.md](../codes.md)).

## STYLE

Codes of the extraction-style subset: style selection, `device` classes and types, antenna tie-down types, substrate and active types (`src/core/src/extract/tech/`; capability extract-tech-section; design D10). Owner: EXTSUB (M6), EXTTECH (M7).

`extract::StyleView` reads the raw `extract` section for one style each
time a writer asks for it, so it reports to the sink it is given, and only
for lines of the selected style that its subset parses: `EXTRACT-STYLE-001`
is Magic's `Malformed line for keyword <kw>.  Correct usage:` for a
`style` line, or a `device` line with fewer arguments than its class
needs; `EXTRACT-STYLE-002` is `Illegal device.  Legal devices are:` with
the class list. A type list that does not parse is reported with the
technology's own type-list code. Magic prints the section's other messages
(and `The following types are not handled by extraction ...`) when the
technology loads; those stay with the technology loader and M7.

`extract::HeightView` (the 3D height subset M10 added to the same reader,
`openspec/changes/m10-3d-view/design.md` D5) reports only its own lines
of the selected style, so reading both views never reports a line twice:
`EXTRACT-STYLE-003` is `Layer height <h> must be numeric` or `Layer
thickness <t> must be numeric`; `EXTRACT-STYLE-004` is `Units must be
microns or lambda.  Using the default value (lambda).`; `EXTRACT-STYLE-005`
is `Unknown planeorder plane <p>`, `Planeorder index must be [0..k]`,
`"noplaneordering" specified after "planeorder".` or `"planeorder"
specified after "noplaneordering".`; `EXTRACT-STYLE-006` is the
end-of-style check's `Warning: Extraction Style <name>` with its
`Conflicting planeorder` / `Missing planeorder` lists and the default
order Magic falls back to (one diagnostic, Magic's lines). A bad type list
of a `height` line is reported with the technology's type-list code.

`extract::load_styles` (M7 EXTTECH, `openspec/changes/m7-extraction/design.md`
D7) runs the full reader once per style at tech load and reports what
Magic prints then: the first style's line errors, its plane-order warning
and the non-electrical list; `StyleTable::select` with a sink replays a
style's messages as `ExtLoadStyle` prints them. `EXTRACT-STYLE-001` also
covers a line with the wrong number of arguments for its keyword and an
`antenna` line with a calculation type and one ratio (Q-MISC-5, ignored in
both modes). `EXTRACT-STYLE-007` is a non-numeric value (`Capacitance value
<v> must be a number`, `Resistance value <v> must be a number`, `Contact
resistivity <v> must be an integer value (in milliohms/square).`, `Device
resistivity <v> must be numeric`, `Cannot parse cap scale value "<v>"`);
`EXTRACT-STYLE-008` the capacitance-rule type and plane errors;
`EXTRACT-STYLE-009` a default-capacitance shorthand before any
`planeorder`; `EXTRACT-STYLE-010` the `device` / `fet` errors (parameter
name too long, `Not enough terminals for subcircuit`, overlapping drain
and source types, `Fet substrate node <n> is not a global name`);
`EXTRACT-STYLE-011` `Non-numeric limit`; `EXTRACT-STYLE-012` the `antenna`
errors, including Q-MISC-3's `No antenna calculation type given for
layer(s) ...` (the rule is stored with no model); `EXTRACT-STYLE-013`
`Unknown antenna model`; `EXTRACT-STYLE-014` the `resist` value messages;
`EXTRACT-STYLE-015` a non-positive `step`; `EXTRACT-STYLE-016` the
zero-step reset of the micron conversion; `EXTRACT-STYLE-017` the ignored
`contact` sizes; `EXTRACT-STYLE-018` the non-electrical list;
`EXTRACT-STYLE-019` an unknown or ambiguous plane name; `EXTRACT-STYLE-020`
`Illegal keyword.  Legal keywords are:`. A `sideoverlap` to space without a
`/plane` suffix (Magic reads past the missing `/` and crashes) reports
`Must specify plane for sideoverlap to space` and ignores the line in both
modes.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=extract
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=extract`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `EXTRACT-STYLE-001` | error | An extract-section line has the wrong number of arguments, or a `style`, `device` or `antenna` line is malformed (Magic's `Malformed line for keyword ...` text; Q-MISC-5); the line has no effect. |
| `EXTRACT-STYLE-002` | error | A `device` line names an unknown device class (`Illegal device.  Legal devices are:`); the line has no effect. |
| `EXTRACT-STYLE-003` | error | A `height` line of the selected extraction style has a non-numeric height or thickness (`Layer height ... must be numeric`); the line has no effect. |
| `EXTRACT-STYLE-004` | error | A `units` line names neither microns nor lambda (`Units must be microns or lambda.`); lambda is used. |
| `EXTRACT-STYLE-005` | error | A `planeorder` or `noplaneordering` line is rejected or out of order (unknown plane, index out of range, one after the other); Magic's text. |
| `EXTRACT-STYLE-006` | warning | The plane orders of the extraction style are not a permutation of 0..k (`Warning: Extraction Style ...`); every plane gets its declaration order. |
| `EXTRACT-STYLE-007` | error | A numeric argument of an extract-section rule is not a number (`Capacitance value ... must be a number`, `Resistance value ...`, `Contact resistivity ... must be an integer value`, `Device resistivity ... must be numeric`, `Cannot parse cap scale value`); Magic's value is used. |
| `EXTRACT-STYLE-008` | error | A capacitance rule names an invalid type or plane combination (overlap of a type with itself or on one plane, a repeated overlap pair, a shield on a shielded plane, space inside an edge, sideoverlap plane errors, fringing on one plane, space in overlap types); Magic's text. |
| `EXTRACT-STYLE-009` | error | A `defaultareacap`, `defaultperimeter`, `defaultoverlap` or `defaultsideoverlap` line comes before any `planeorder` line (`Cannot parse ... line without plane ordering!`); the line has no effect. |
| `EXTRACT-STYLE-010` | error | A `device` or `fet` line has a bad parameter name, too few subcircuit terminals, overlapping drain and source types or a non-global fet substrate node; Magic's text. |
| `EXTRACT-STYLE-011` | warning | A device parameter limit is not a number (`Non-numeric limit ... for parameter ...`); the limit parsing stops there. |
| `EXTRACT-STYLE-012` | error | An `antenna` line has an unknown calculation type, a non-numeric ratio, or no calculation type and no default (Q-MISC-3: stored as none); Magic's text. |
| `EXTRACT-STYLE-013` | warning | A `model` line names an unknown antenna model (`Unknown antenna model ...`); no flag is set for it. |
| `EXTRACT-STYLE-014` | warning | A `resist` value is neither an integer nor `None` (`Resist argument must be integer or "None".`, `Unparseable resist argument`, `Resist argument truncated to integer value.`). |
| `EXTRACT-STYLE-015` | error | A `step` value is not positive (`Hierarchical interaction step size must be > 0`); Magic aborts the technology load, the style keeps its step. |
| `EXTRACT-STYLE-016` | warning | The step size is zero after unit conversion (`Warning:  zero step size!  Resetting to default.`); 100 is used. |
| `EXTRACT-STYLE-017` | info | A `contact` line gives size, border or spacing values, which are ignored (`Contact size value ignored (using GDS generation rules).`). |
| `EXTRACT-STYLE-018` | info | User types no extract line mentions and that connect to nothing are non-electrical (`The following types are not handled by extraction and will be treated as non-electrical types:`). |
| `EXTRACT-STYLE-019` | error | An extract-section line names an unknown or ambiguous plane (`Unrecognized plane name ...`, `Ambiguous plane name ...`); the line stops there. |
| `EXTRACT-STYLE-020` | error | An extract-section line starts with an unknown keyword (`Illegal keyword.  Legal keywords are:`); the line is ignored. |
<!-- END DIAGNOSTIC CODES -->
