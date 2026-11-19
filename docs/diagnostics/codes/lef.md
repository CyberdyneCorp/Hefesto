# Diagnostic codes: lef area

Codes `LEF-<TOPIC>-<NNN>` emitted by `src/core/src/lef/` (LEF/DEF tokenizer and message reporting, the tech `lef` section and the LEF layer table, the via geometry generator, LEF read and write, and the `lef`/`def` commands;
`openspec/changes/m6-lef-def-oasis/design.md` D1/D2). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/lef/codes.cpp` (owner:
INTEGRATION) from one fragment per workstream, each owned by that
workstream. The rows are sorted while compiling
(`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/lef/lex/codes.inc` | `LEF-LEX-*` | LEFR |
| `src/core/src/lef/tech/codes.inc` | `LEF-TECH-*` | LEFTECH |
| `src/core/src/lef/via/codes.inc` | `LEF-VIA-*` | LEFVIA |
| `src/core/src/lef/read/codes.inc` | `LEF-READ-*` | LEFR |
| `src/core/src/lef/write/codes.inc` | `LEF-WRITE-*` | LEFW |
| `src/core/src/lef/command/codes.inc` | `LEF-CMD-*` | LEFR, LEFW |

While M6 is in progress, an option or function the scaffold declared before
its workstream implemented it fails with the core code `CORE-NOTIMPL-006`
([../codes.md](../codes.md)).

## LEX

Codes of the LEF/DEF tokenizer, message reporting and error limits (`src/core/src/lef/lex/`; capabilities lef-read, def-read; design D6). Owner: LEFR.

Reader messages keep Magic's LefError form, `<LEF|DEF> read, Line <n> (Error|Warning|Message): <text>`, under the code of the reader that reports them (`LEF-READ-*`, `DEF-READ-*`). LEF and DEF messages share one set of counters, as LefError's do.

- `LEF-LEX-001`: the notice printed instead of the 101st message of one kind: `<LEF|DEF> Read:  Further errors will not be reported.`, `<LEF|DEF> read:  Further warnings will not be reported.` or `<LEF|DEF> read:  Further messages will not be reported.` (Magic's capitalisation). It has the severity of the kind it caps; later messages of that kind are only counted.
- `LEF-LEX-002`: the totals at the end of a read, `<LEF|DEF> Read: encountered <k> error[s] total.` and the same for warnings, printed only when nonzero; the counters are then reset.
- `LEF-LEX-003`: END matching (LefParseEndStatement, LefSkipSection): `Bad file read while looking for END statement` when the input ends after an END, and `Section <N> has no END record!` when a skipped section has none (a nameless skip, PORT or OBS, prints `(null)` as Magic does). Both carry the `LEF` prefix, also in a DEF read.

## TECH

Codes of the tech `lef` section, the LEF layer table, the via table and the non-default rules (`src/core/src/lef/tech/`; capability lef-tech-section; design D5). Owner: LEFTECH.

- **`LEF-TECH-001` to `LEF-TECH-008`.** The `TechError` messages of
  LefTechLine, with Magic's text, printed while the technology is read
  (after the `drc` section's messages). They never fail the load: Magic's
  section handler always returns TRUE. Each carries the file, the line and
  the section `lef`, so the magic rendering prints TechPrintLine's context.
  A bad type name also gets DBTechNoisyNameMask's own `TECH-LINE-001`
  message before `LEF-TECH-003`.
- **`LEF-TECH-005`** is magic-compat OFF only. Magic stops counting at the
  second type, so with magic-compat ON a third type is dropped (an
  obstruction line keeps the first two; any other line gets `LEF-TECH-004`).
- **`LEF-TECH-008`** names the record's normal type, or its obstruction
  type when it has none (Magic indexes its type-name table with -1 there).
  An `ignore` line that names a mapped LEF name also reports it and leaves
  the name unchanged (Magic reads uninitialized variables there). Both are
  fixed in both modes.

## VIA

Codes of the via geometry generator: LEF VIA and VIARULE definitions, generated DEF vias, cut growing and the DEF-write contact regions (`src/core/src/lef/via/`; capabilities lef-read, def-read, def-write; design D6, D8, D9). Owner: LEFVIA.

The texts are Magic's message bodies; the LEF and DEF readers add the
`LEF Read, Line N:` / `DEF Read, Line N:` context and count them.

- `LEF-VIA-001`, `LEF-VIA-002`: `Cut type "<name>" redefined.` (LEF VIA)
  and `Composite via "<name>" redefined.` (DEF VIAS). The old geometry is
  dropped; when the name is one of several aliases of a tech `lef` layer,
  only that name gets the new record, of class VIA (Q-LEFDEF-8 fix).
- `LEF-VIA-003`, `LEF-VIA-004`: cut growing (lef-read "Cut growing",
  Q-LEFDEF-17), printed together: `Cut size for magic type "<t>" (<E> x <E>)
  does not match LEF/DEF` and `Via cut size (<w> x <h>).  Magic layer cut
  size will be used!`, sizes in half internal units. With magic-compat ON
  `<t>` is the via entry's normal type at that moment, as Magic prints it
  (the top layer for a generated via); with it OFF the cut's type.
- `LEF-VIA-005`, `LEF-VIA-006`: `Improper layer order for VIARULE.` and
  `No cut layer specified in VIARULE.` (def-read "Generated DEF vias").
- `LEF-VIA-007` (Hefesto only): `Via "<name>": <PARAM> not given; using the
  VIARULE value.` or `...; using 0.` for an omitted LAYERS, CUTSIZE,
  CUTSPACING or ENCLOSURE (Q-LEFDEF-61 fix; Magic reads an uninitialised
  value).
- `LEF-VIA-008`: `Cannot parse via PATTERN "<pattern>"; every cut is kept.`
  (magic-compat OFF only, Q-LEFDEF-62).
- `LEF-VIA-009`, `LEF-VIA-010`: DEF write's `Warning: via size is <z> but
  area width|height is <n>!` (def-write "VIAS section content"); the
  height test is Magic's inverted one with magic-compat ON (Q-LEFDEF-107).
- `LEF-VIA-011` (Hefesto only): `No LEF layer corresponding to magic type
  "<t>" of via "<name>"; RECT skipped.` where Magic writes `(null)`
  (Q-LEFDEF-115 fix).

## READ

Codes of the LEF reader, abstract views and annotation (`src/core/src/lef/read/`; capability lef-read; design D6). Owner: LEFR.

- `LEF-READ-001`: LEF read errors in Magic's LefError form: `Don't know how to parse layer "<name>"` with `Try adding this name to the LEF techfile section`, `Bad Layer statement`, `Layer <name> maps to a magic contact layer; must be defined in lef section of techfile`, `Layer <name> is only defined for obstructions!`, `Attempt to reclassify layer <name> from <old> to <new>` (Magic prints `(null)` for an old class of BOUND; Hefesto names it), `Layer END statement missing.`, `END statement out of context.`, `No layer defined for RECT.`, `Bad port geometry: RECT requires 4 values.`, `Bad X value in polygon.`, `Bad Y value in polygon.`, `Missing Y value in polygon point!`, `Bad macro SIZE; requires values X BY Y.`, `Bad macro ORIGIN; requires 2 values.`, `Bad origin in macro FOREIGN; requires 0 or 2 values.`, `Improper DIRECTION statement`, `Improper USE statement`, `Improper SHAPE statement`, `Pin END statement missing.`, `Geometry (PORT or OBS) END statement missing.` and `Macro END statement missing.`. With magic-compat OFF only (Hefesto's): `Bad RECT ITERATE: requires DO <nx> BY <ny> STEP <dx> <dy>.` and `Bad ROWCOL: requires two counts whose product is at most 65536.` in a VIA (Q-LEFDEF-7), and `Bad X value in path.` and its kin for a PATH (Q-LEFDEF-15).
- `LEF-READ-002`: LEF read warnings: `Unknown layer type "<t>" in LEF file; ignoring.`, `Nondefault rule "<name>" was already defined.` and `Renaming this rule "<name>_<i>"` (Q-LEFDEF-9: the first free index, printed once), `Cell "<name>" was already defined in this file.  Renaming this cell "<name>_<k>"` and `   Macro does not define size:  computing from geometry` (three leading spaces, as Magic prints it).
- `LEF-READ-003`: LEF read messages: `Unknown keyword "<t>" in LEF file; ignoring.`, `Unknown keyword "<t>" in NONDEFAULTRULES definition; ignoring.`, the NONDEFAULTRULE value messages (`WIDTH specified without layer.`, `No rule for non-default <width|spacing|extension>.`, `No layer for non-default <...>.`, `Layer value specified without layer.`). `SPACING specified without layer.` and `WIREEXT specified without layer.` carry the `DEF` prefix, as in Magic. With magic-compat OFF only: `Unknown via "<name>" in LEF geometry; ignoring.` for a PORT or OBS `VIA` naming no via (Q-LEFDEF-15).
- `LEF-READ-004`, `LEF-READ-005`: `Reading LEF data from file <path>.` before the read and `LEF read: Processed <n> lines.` after it (lef-read "lef read side effects"); the `LEF-LEX-002` totals follow. Magic's `This action cannot be undone.` is not printed: the read is one undo step (Q-LEFDEF-22).
- `LEF-READ-006`: `Cannot open input file <path> (<reason>).` when no candidate of the file lookup opens (Q-LEFDEF-24); `<path>` is the last name tried, ending in `.lef`. Nothing is read.

The messages of the via code that the reader runs (`LEF-VIA-*`) keep their own codes and get the reader's `LEF read, Line <n>` context.

## WRITE

Codes of the LEF writer (`src/core/src/lef/write/`; capability lef-write; design D7). Owner: LEFW.

No codes yet.

## CMD

Codes of the `lef` and `def` commands (`src/core/src/lef/command/`; design D14). The dispatcher is INTEGRATION's; the codes of each option belong to the workstream that owns it. Owner: LEFR, LEFW.

No codes yet.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=lef
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=lef`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `LEF-LEX-001` | info | Message cap notice: `LEF Read:  Further errors will not be reported.` (or DEF; or warnings, messages) after 100 messages of one kind. |
| `LEF-LEX-002` | info | Read totals: `LEF Read: encountered <k> errors total.` (or DEF; or warnings) at the end of a read. |
| `LEF-LEX-003` | error | END matching: `Bad file read while looking for END statement` or `Section <N> has no END record!`. |
| `LEF-READ-001` | error | LEF read error (Magic's LefError text): unknown layer, bad LAYER statement, layer reclassification, missing or misplaced END, malformed RECT, POLYGON, SIZE, ORIGIN or FOREIGN, improper DIRECTION, USE or SHAPE. |
| `LEF-READ-002` | warning | LEF read warning (Magic's LefError text): unknown layer TYPE, duplicate NONDEFAULTRULE or macro renamed, macro without SIZE. |
| `LEF-READ-003` | info | LEF read message (Magic's LefError text): unknown keyword ignored, NONDEFAULTRULE value without a rule or layer, unknown via in macro geometry. |
| `LEF-READ-004` | info | lef read: `Reading LEF data from file <path>.` before the file is read. |
| `LEF-READ-005` | info | lef read: `LEF read: Processed <n> lines.` after the file is read. |
| `LEF-READ-006` | error | lef read: `Cannot open input file <path> (<reason>).`; nothing is read. |
| `LEF-TECH-001` | error | Unknown or ambiguous keyword in the tech lef section; the line is ignored. |
| `LEF-TECH-002` | error | A lef section line names no LEF layer; the line is ignored. |
| `LEF-TECH-003` | error | A lef section line names no magic user type; the line is ignored. |
| `LEF-TECH-004` | error | Two magic types on a lef section line other than obstruction; the line is ignored. |
| `LEF-TECH-005` | error | More than two magic types on a lef section line (magic-compat OFF); the line is ignored. |
| `LEF-TECH-006` | error | A contact type is defined with a keyword other than cut, contact, obstruction or layer. |
| `LEF-TECH-007` | error | A non-contact type is defined as a cut or contact. |
| `LEF-TECH-008` | error | A LEF name is already bound to a magic type; the name keeps its record. |
| `LEF-VIA-001` | warning | A LEF VIA redefines a via or layer name (Cut type redefined). |
| `LEF-VIA-002` | warning | A DEF VIAS definition redefines a via or layer name (Composite via redefined). |
| `LEF-VIA-003` | warning | Cut growing: the via cut is not the output style's cut size for its type. |
| `LEF-VIA-004` | warning | Cut growing: the via cut's size; the magic contact size is used. |
| `LEF-VIA-005` | warning | The middle LAYERS entry of a generated via is not a contact; the layers are reordered. |
| `LEF-VIA-006` | error | No LAYERS entry of a generated via is a contact. |
| `LEF-VIA-007` | warning | A generated via omits a parameter; the VIARULE value or 0 is used. |
| `LEF-VIA-008` | warning | A via PATTERN does not parse; every cut is kept. |
| `LEF-VIA-009` | warning | DEF write: a via's cut is wider than its area. |
| `LEF-VIA-010` | warning | DEF write: a via's cut height check (inverted in Magic) fired. |
| `LEF-VIA-011` | warning | DEF write: a via layer has no LEF name; its RECT is skipped. |
<!-- END DIAGNOSTIC CODES -->
