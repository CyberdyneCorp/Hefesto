# Diagnostic codes: extract area

Codes `EXTRACT-<TOPIC>-<NNN>` emitted by `src/core/src/extract/` (the extraction-style subset of the `extract` tech section reader that LEF and DEF write read (Open Question 2, decided (a); built to the M7 spec extract-tech-section, which M7 extends);
`openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md` D1/D2). Conventions and the
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

## NODE

Codes of the node pass, the per-node records and unique naming (`src/core/src/extract/node/`; capability extract-nodes-and-labels; design D8). Owner: EXTNODE.

`EXTRACT-NODE-001` is extOutputNodes' `Warning:  Ports "<a>" and "<b>"
are electrically shorted.` for two port labels with different names on
one node (names compared after cutting each at its first `_uq`); with
magic-compat ON it is suppressed whenever lumped resistance extraction is
on, as in Magic, and with it OFF only when full R-C extraction runs in the
same pass (Q-EXTRACT-22). `EXTRACT-NODE-002` (`Processing <cell>`) and
`EXTRACT-NODE-003` (`<cell>: <n> warnings`) are the unique pass's
messages; the non-global duplicates it warns about and the duplicate-label
warning of "Duplicate label warning" are feedback entries
(`extract::NodeFeedback`), not diagnostics. `EXTRACT-NODE-004` reports a
unique pass called without an extraction style.

## FILE

Codes of the `.ext` writer and the atomic output file (`src/core/src/extract/file/`; capabilities extract-file-format, extract-commands "Output path and file location"; Q-EXTRACT-27). Owner: EXTNODE.

`EXTRACT-FILE-001` and `EXTRACT-FILE-002` are writer contract errors
("Reader constraints honored by the writer"): a record whose area and
perimeter list does not hold one pair per resistance class, and a file
without header records; no text is produced. `EXTRACT-FILE-003` reports
an output file that cannot be opened, written or renamed over its target;
the previous file is left as it was. `EXTRACT-FILE-004` is ExtFileOpen's
`Path "<dir>" does not exist;  attempting to create it.` and
`...  saving locally.` for an `extract path` directory.

## CAP

Codes of capacitance and lumped resistance (`src/core/src/extract/cap/`;
capabilities extract-capacitance and extract-resistance-lumped;
`openspec/changes/m7-extraction/design.md` D9). Owner: EXTCAP.

`EXTRACT-CAP-001` is Magic's `Warning:  Ports "<a>" and "<b>" are
electrically shorted.` (ExtBasic.c:815-838), which `extract::shorted_ports_warning`
builds for the node writer when a node carries two port labels whose names
differ after each is cut at its first `_uq`. Q-EXTRACT-22 decides when it is
suppressed: with magic-compat ON whenever lumped resistance is on (Magic's
test of `EXT_DORESISTANCE`, so the default configuration never prints it);
with it OFF only when extresist runs in the same pass.

`EXTRACT-CAP-002` reports a call of `substrate_caps`, `find_coupling` or one
of the single-kind coupling functions without the cell, the extraction style
or the technology in `CapInputs`; it is a programming error and nothing is
computed.

## DEV

Codes of device recognition (`src/core/src/extract/device/`; capabilities
extract-devices and extract-substrate; design D10). Owner: EXTDEV.

The device pass reports what Magic's extOutputDevices prints on the
console: `EXTRACT-DEV-001` a region no record matches; `EXTRACT-DEV-002`
more terminals than the records declare; `EXTRACT-DEV-003` to
`EXTRACT-DEV-006` record selection by substrate and identifier layers;
`EXTRACT-DEV-007` parameter ranges no compatible record accepts
(Q-EXTRACT-21); `EXTRACT-DEV-008` a gate with several lengths;
`EXTRACT-DEV-009` a capacitor or diode with an empty terminal boundary (the
equivalent-square fallback); `EXTRACT-DEV-010` a malformed `device` cell
property (Q-EXTRACT-16); `EXTRACT-DEV-011` equal terminal positions while
sorting. Magic's per-device feedback (`device missing N terminal(s)`,
`Could not determine device boundary`, `Resistor has zero width`,
`Capacitor has zero size`, `L,W estimated for non-rectangular capacitor.`
and the resistor terminal-count warnings) goes to the feedback list, not
to a code, as Magic's DBWFeedbackAdd does. `EXTRACT-DEV-012` is an internal
error of a caller that omits the cell, technology, style or node list.

Subcell device priority (task 7.7, `disable_subcell_devices`) drops a
parent device whose region a subcell device covers; it reports nothing, as
Magic does.

## CMD

Codes of the `extract` command and its run driver (`src/core/src/extract/command/`,
`extract_cell` in `src/core/src/extract/hier/extract_cell.cpp`; capability
extract-commands; design D12). Owner: EXTCMD.

`EXTRACT-CMD-001` is a call of `extract::extract_cell` without an extraction
style or technology in its `ExtractContext`; nothing is written.
`EXTRACT-CMD-002` is ExtCell's `Extracting <cell> into <file>:` and, after a
cell with errors or warnings, `<cell>: N errors M warnings` (TxPrintf, the
standard output); `EXTRACT-CMD-003` the run's totals `Total of N errors
(check feedback entries).` and `Total of M warnings.`, and `extract unique`'s
`N uncorrected errors (see the feedback info)` (TxError). `EXTRACT-CMD-004`
is a run that does not start: `Please name the cell before extracting.`,
`No cell selected`, `No extraction style set.`, `Failure to read entire
subtree of cell.` with `Failed on cell <name>.`, and `extract all`'s
`Warning:  There is nothing here to extract.`; `extract::ensure_extracted`
fails with it. `EXTRACT-CMD-005` is `Cannot open output file.`: the cell is
skipped and its substrate is not prepared. `EXTRACT-CMD-006` reports a
`.ext` that cannot be produced, written or renamed over its target
(Q-EXTRACT-27): the temporary file is removed and the previous file kept.
The option usages and listings (`Usage: extract do option`, `The following
are the extractor option settings:`, the style and path messages) are the
command's report and warning text, as Magic prints them.

## LEN

Codes of path-length extraction (`src/core/src/extract/length/`). Owner:
EXTCMD. None yet: ExtLength.c's flood is tasks.md 9.5; until it lands a run
with an `extract length driver` reports `CORE-NOTIMPL-008`.

## HIER

Codes of hierarchical extraction (`src/core/src/extract/hier/`; capability
extract-hierarchy; design D11). Owner: EXTHIER.

No code is declared. What Magic reports during the hierarchical passes it
adds as feedback (DBWFeedbackAdd), and `extract_subtree` returns it the
same way (`SubtreeResult::feedback`, with the error and warning counts):
`Illegal overlap between <t1> and <t2> (types do not connect)` and `Cannot
find the name of this node (probable extractor error)` (errors), `Warning:
node labels should be inside overlap area` (with `extract warn labels`),
and `System error: expected array element but none found`. A caller that
omits the technology or the style gets the internal error
`CMD-INTERNAL-001`.

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
| `EXTRACT-CAP-001` | warning | Two port labels of different names are on one node (`Warning:  Ports "a" and "b" are electrically shorted.`, ExtBasic.c:815-838); suppressed per Q-EXTRACT-22: with magic-compat ON whenever lumped resistance is on, with it OFF only when extresist runs in the same pass. |
| `EXTRACT-CAP-002` | error | A capacitance entry point was called without a cell, an extraction style or a technology; nothing is computed. |
| `EXTRACT-CMD-001` | error | extract_cell was called without an extraction style or a technology; the cell is not extracted. |
| `EXTRACT-CMD-002` | info | A cell's extraction starts (`Extracting <cell> into <file>:`), or its counts follow it (`<cell>: N errors M warnings`). |
| `EXTRACT-CMD-003` | warning | The totals of a run (`Total of N errors (check feedback entries).`, `Total of M warnings.`). |
| `EXTRACT-CMD-004` | warning | A run cannot start: the root is unnamed, nothing is selected, the subtree cannot be read, no style is set, or there is nothing to extract. |
| `EXTRACT-CMD-005` | warning | A cell's `.ext` file cannot be opened for writing (`Cannot open output file.`); the cell is skipped. |
| `EXTRACT-CMD-006` | error | A cell's `.ext` file cannot be produced or committed; the previous file is left unchanged. |
| `EXTRACT-DEV-001` | warning | A device region matches no extraction record after terminal discovery (`Warning:  No matching extraction type for device at (x y)`); no device is written. |
| `EXTRACT-DEV-002` | warning | A device has more terminals than any record of its type declares (`Warning:  Device has more terminals than defined for type!`); the record is kept. |
| `EXTRACT-DEV-003` | error | The substrate layer under a device matches no record of its type (`No matching device for <type> with substrate layer <layer>`); no device is written. |
| `EXTRACT-DEV-004` | error | No substrate node is found under a device whose record needs one (`Device <type> does not have a compatible substrate node!`); no device is written. |
| `EXTRACT-DEV-005` | error | A device without identifier layer matches no record without one (`No matching device for <type> with no ID layer`); no device is written. |
| `EXTRACT-DEV-006` | warning | An identifier layer over a device matches no record of its type (`ID layer <layer> on non-matching device <type> was ignored.`); the record is kept. |
| `EXTRACT-DEV-007` | warning | No compatible record's parameter ranges accept a device's dimensions (`Device parameters do not match any extraction model.`). |
| `EXTRACT-DEV-008` | warning | A non-rectangular gate has several lengths (`Device has multiple lengths:  scaling all widths to length L`); the widths are scaled to the shortest. |
| `EXTRACT-DEV-009` | error | A capacitor's or diode's terminal boundary is empty (`extract:  Can't get capacitor L and W`); L and W are the equivalent square of the terminal area. |
| `EXTRACT-DEV-010` | error | A `device` cell property names an unknown device class or is malformed (`Extract error:  "device" property has unknown device type: ...`, `... property syntax error`); the normal device output is used. |
| `EXTRACT-DEV-011` | info | Two terminals of a symmetric device have the same position while sorting (`Extract error:  Duplicate tile position, ignoring`); they keep their order. |
| `EXTRACT-DEV-012` | error | The device pass was called without its cell, technology, style or node list (an internal error); nothing is extracted. |
| `EXTRACT-FILE-001` | error | A `node`, `substrate` or `merge` record does not hold one area and perimeter pair per resistance class of the header; the `.ext` text is not produced. |
| `EXTRACT-FILE-002` | error | The `.ext` writer was finished without header records; the `.ext` text is not produced. |
| `EXTRACT-FILE-003` | error | An extraction output file cannot be opened, written or renamed over its target (`Cannot open output file ...`); the previous file is left unchanged. |
| `EXTRACT-FILE-004` | warning | The `extract path` directory does not exist (`Path "..." does not exist;  attempting to create it.`, `...  saving locally.`). |
| `EXTRACT-NODE-001` | warning | Two port labels with different names are on one node (`Warning:  Ports "a" and "b" are electrically shorted.`); suppressed per Q-EXTRACT-22. |
| `EXTRACT-NODE-002` | info | The unique-naming pass starts on a cell (`Processing <cell>`). |
| `EXTRACT-NODE-003` | warning | The unique-naming pass left non-global duplicate labels as feedback (`<cell>: <n> warnings`). |
| `EXTRACT-NODE-004` | error | The unique-naming pass was called without an extraction style or technology; no label is renamed. |
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
