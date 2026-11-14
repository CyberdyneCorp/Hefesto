# Diagnostic codes: mask area

Codes `MASK-<TOPIC>-<NNN>` emitted by `src/core/src/mask/` (mask generation: the mask planes, the cifoutput and cifinput readers, the operator kernels and flat and hierarchical generation;
`openspec/changes/m5-mask-gds-cif/design.md` D1/D2). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/mask/codes.cpp` (owner:
INTEGRATION) from one fragment per workstream, each owned by that
workstream. The rows are sorted while compiling
(`src/core/src/diag/sorted_codes.hpp`):

| Fragment | Topics | Owner |
|---|---|---|
| `src/core/src/mask/plane/codes.inc` | (none: the plane codes use `MASK-OPER-*` in ops/codes.inc) | OPS |
| `src/core/src/mask/style/codes.inc` | `MASK-STYLE-*` | STYLES |
| `src/core/src/mask/input/codes.inc` | `MASK-INPUT-*` | INPUT |
| `src/core/src/mask/ops/codes.inc` | `MASK-OPER-*` | OPS |
| `src/core/src/mask/gen/codes.inc` | `MASK-GEN-*` | GEN |

While M5 is in progress, an option or function the scaffold declared before
its workstream implemented it fails with the core code `CORE-NOTIMPL-005`
([../codes.md](../codes.md)).

## STYLES

Codes of the cifoutput reader, output style selection and rescale (`src/core/src/mask/style/`; capability mask-output-styles; design D5).

- `MASK-STYLE-001` is Magic's `TechError` for a `cifoutput` line, with Magic's text (for example `Unknown statement "foo".`, `Grow/shrink distance must be greater than zero.`, `Calma layer and type numbers must be 0 to 255.`); the magic rendering prints the line context (`<file>: line <n>: section cifoutput:`). A rejected operator is not appended; a bad value is reset as Magic resets it.
- `MASK-STYLE-002` is the same for the messages the capability calls warnings, after which the statement still applies: `Layer "<name>" contains no material.`, `Why are you attaching labels to a temporary layer?`, `Why assign a Calma number to a temporary layer?`.
- `MASK-STYLE-003` (native only) names a `style` line that Magic's prefix test would drop (`style gds` after `style gdsfoo`). Hefesto registers it at the end of the style list in both `magic-compat` settings (Q-MASK-10, fix).
- `MASK-STYLE-004` (native only) warns after a unit normalisation (the load, and every later rescale) of a style that uses `bloat-all`, has a non-zero `gridlimit` and has an expander other than 10, where the operators' and the grid guard's readings of the value disagree (Q-MASK-8, amended 2026-11-14). A style without `bloat-all` reduces its units on purpose, as the PDK `density` and `metfill` styles do, and is not warned about.
- `MASK-STYLE-007` (native only, magic-compat OFF only) warns that a unit normalisation truncated the style's scale factor: Magic divides by the smaller of the operands' common factor and gcd(scale factor, expander), which need not divide the scale factor (a 10/10 `bloat-all` style rescaled by 1/3 becomes 3/10). Hefesto copies the arithmetic in both modes (Q-MASK-29).
- `MASK-STYLE-005` and `MASK-STYLE-006` are `cif ostyle` failures with Magic's texts: `CIF output style "<name>" is ambiguous.` and `"<name>" is not one of the CIF output styles Magic knows.` The current style is unchanged.

The tech-load messages of a style are those of its load pass: the first style's at `tech load`, another style's each time `cif ostyle` loads it, as Magic re-reads the section.

## INPUT

Codes of the cifinput reader and the shared read engine (`src/core/src/mask/input/`; capability mask-input-styles; design D9).

Tech load and `cif istyle`:

- `MASK-INPUT-001` is Magic's `TechError` for a `cifinput` line, with Magic's text (for example `Must define layer before specifying operations.`, `Grow/shrink distance must be greater than zero.`, `Unrecognized CIF layer: "<name>"`, `Calma layer and type numbers must be 0 to 255.`, `CIF input style "<name>" bad scalefactor; using 1.`); the magic rendering prints the line context (`<file>: line <n>: section cifinput:`). The statement is skipped, or its value reset as Magic resets it.
- `MASK-INPUT-003` is Magic's `Input style <name>: scaleFactor=<S>, multiplier=<M>` line, printed after a style is loaded and normalised: the first style's at `tech load`, another style's each time `cif istyle` loads it (before the lambda ratio is applied, as CIFReadTechFinal prints it).
- `MASK-INPUT-004` and `MASK-INPUT-005` are `cif istyle` failures with Magic's texts: `CIF input style "<name>" is ambiguous.` and `"<name>" is not one of the CIF input styles Magic knows.` An exact name wins over prefix matches in both modes (Q-MASK-303, fix), so `cif istyle gds` with styles `gds` and `gds2` selects `gds`.
- Native warnings, dropped by the magic rendering: `MASK-INPUT-002`, a style name that is a strict prefix of an earlier one (Q-MASK-302: merged with magic-compat ON, its own style OFF); `MASK-INPUT-006`, a decimal `scalefactor` with a unit keyword (Q-MASK-300: ON sets the multiplier to the unit, OFF multiplies it); `MASK-INPUT-007`, `boundary` or `mask-hints` in a `layer` rule or a `copyup` templayer, where they have no input meaning (Q-MASK-307); `MASK-INPUT-008`, `labels` under a templayer ignored (Q-MASK-311: always with magic-compat OFF, and in both modes when the templayer's CIF index is no layout type, as sky130A's `obspoly` is); `MASK-INPUT-009` (magic-compat OFF only), a scale normalisation that truncated S or a distance, because Magic divides by min(gcd(M, distances), gcd(S, M)) rather than the common factor, which Hefesto copies in both modes.

Read engine (the CIF and GDS readers may route these through their warning level and formatting):

- `MASK-INPUT-010` / `MASK-INPUT-011` are Magic's `Input off lambda grid by <r>/<d>; grid redefined.` and `...; snapped to grid.`, once per read: an off-grid coordinate subdivides the internal grid of the whole session (layout, DRC rules, output styles) or is snapped (`cif rescale no`, or the input style's `gridlimit` refuses the subdivision).
- `MASK-INPUT-012` is `CIF style <name>: units rescaled by factor of <n> / <d>` after a mid-read subdivision of the read units.
- `MASK-INPUT-013` is `Warning:  Cell <name> boundary was redefined.` when a boundary templayer gives a cell a different FIXED_BBOX.
- `MASK-INPUT-014` and `MASK-INPUT-015` (native notes, once per read) report input `grow`/`shrink` while the output style has grow-euclidean (Q-MASK-306: Euclidean with magic-compat ON, square OFF) and an input `tagged` rule with a scale factor other than 1 (Q-MASK-310: the label area unscaled ON, times S OFF).
- `MASK-INPUT-016` is a format reader driving the engine out of order (a cell inside a cell, an end or a call without a cell); `MASK-INPUT-017` (native) is the resource cap of the copyup copy into arrays of more than 2^20 elements (design D21).

## OPS

Codes of the mask planes and the operator kernels (`src/core/src/mask/plane/`, `src/core/src/mask/ops/`; capability mask-output-operators; design D6).

These are Magic's generation errors (CIFError): the text is `CIF error in
cell <cell>, layer <layer>: <message>` with Magic's message, located at the
lower-left corner of the offending area (internal units). Magic adds them as
feedback and prints nothing, so they are `native_only`. They are not
reported when the style has `options no-errors` or no cell is being
generated.

- `MASK-OPER-001` "tile inverted by shrink": bloat-min or bloat-max moved a
  tile's sides past each other; nothing is painted for it.
- `MASK-OPER-002` "no room for contact cuts in area!": a contact strip (one
  cut wide) holds no cut row and is not exactly one cut in size.
- `MASK-OPER-003` "contact strip with no room for cuts!": a strip shortened
  at an end touching more material holds no cut, and its area exceeds twice
  the squared pitch.
- `MASK-OPER-004` "no room for contacts in area!": no cut of `squares`,
  `squares-grid` or `slots` fits a contact region after three placements.

## GEN

Codes of flat and hierarchical generation, `cif see|paint|coverage|statistics` and the interaction finder (`src/core/src/mask/gen/`; capability mask-hierarchical-generation; design D8).

No codes yet.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=mask
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=mask`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `MASK-INPUT-001` | error | Error in a `cifinput` section line (Magic's message); the statement is skipped or its value reset. |
| `MASK-INPUT-002` | warning | Input style name that is a strict prefix of an earlier one (Q-MASK-302); native only. |
| `MASK-INPUT-003` | info | The loaded input style's scale (`Input style <name>: scaleFactor=<S>, multiplier=<M>`). |
| `MASK-INPUT-004` | error | `cif istyle`: the name is a prefix of more than one input style and equals none. |
| `MASK-INPUT-005` | error | `cif istyle`: no input style starts with the name. |
| `MASK-INPUT-006` | warning | Decimal `scalefactor` with a unit keyword, where the magic-compat settings differ (Q-MASK-300); native only. |
| `MASK-INPUT-007` | warning | `boundary` or `mask-hints` in a `layer` rule or a `copyup` templayer (Q-MASK-307); native only. |
| `MASK-INPUT-008` | warning | `labels` under a templayer ignored (Q-MASK-311: magic-compat OFF, or an index that is no layout type); native only. |
| `MASK-INPUT-009` | warning | Input scale normalisation truncated the scale factor or a distance, as Magic does; magic-compat OFF only. |
| `MASK-INPUT-010` | warning | Read geometry off the internal grid: the grid was subdivided (`Input off lambda grid by <r>/<d>; grid redefined.`). |
| `MASK-INPUT-011` | warning | Read geometry off the internal grid: snapped (`Input off lambda grid by <r>/<d>; snapped to grid.`). |
| `MASK-INPUT-012` | warning | The read units were subdivided mid-read (`CIF style <name>: units rescaled by factor of <n> / <d>`). |
| `MASK-INPUT-013` | warning | A boundary templayer redefined a cell's FIXED_BBOX (`Warning:  Cell <name> boundary was redefined.`). |
| `MASK-INPUT-014` | info | Input `grow`/`shrink` while the output style has grow-euclidean (Q-MASK-306); native only, once per read. |
| `MASK-INPUT-015` | info | Input `tagged` rule run with a scale factor other than 1 (Q-MASK-310); native only, once per read. |
| `MASK-INPUT-016` | error | Read engine used out of order by a format reader (a cell inside a cell, an end or a call without a cell). |
| `MASK-INPUT-017` | warning | Copyup planes not copied into an array of more than 2^20 elements (resource cap); native only. |
| `MASK-OPER-001` | warning | Mask generation: a bloat-min or bloat-max tile was inverted by shrink. |
| `MASK-OPER-002` | warning | Mask generation: no room for contact cuts in a contact strip. |
| `MASK-OPER-003` | warning | Mask generation: a shortened contact strip has no room for cuts. |
| `MASK-OPER-004` | warning | Mask generation: no room for contact cuts in a contact region. |
| `MASK-STYLE-001` | error | Error in a `cifoutput` section line (Magic's message); the statement is skipped or its value reset. |
| `MASK-STYLE-002` | warning | Warning for a `cifoutput` section line (Magic's message); the statement still applies. |
| `MASK-STYLE-003` | warning | Output style name that Magic's prefix test would drop; registered at the end of the list (Q-MASK-10); native only. |
| `MASK-STYLE-004` | warning | Non-zero `gridlimit` in a style with `bloat-all` whose expander is not 10: applied in mask units (Q-MASK-8); native only. |
| `MASK-STYLE-005` | error | `cif ostyle`: the name is a prefix of more than one output style. |
| `MASK-STYLE-006` | error | `cif ostyle`: no output style has the name or starts with it. |
| `MASK-STYLE-007` | warning | Unit normalisation truncated the output style's scale factor, as Magic does (Q-MASK-29); magic-compat OFF only. |
<!-- END DIAGNOSTIC CODES -->
