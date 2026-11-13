# Mask Output Styles

## Purpose

Defines the `cifoutput` section of the technology file: how mask output styles, style variants, mask layers (`layer`, `templayer`, `labellayer`), label-to-layer mappings, GDS layer/datatype mappings, style options and scaling are declared, parsed, validated and normalised. The normalised style is the input to mask generation (capability `mask-output-operators`), hierarchical generation (capability `mask-hierarchical-generation`), GDS writing (capability `gds-write`) and CIF writing (capability `cif-format-io`). Hefesto MUST load the same `cifoutput` text into an equivalent style so that, given the same layout, it generates identical mask geometry.

Terminology used throughout the MASK capabilities:

- **Internal unit (u):** the layout database coordinate unit (see capability `layout-geometry-model`).
- **Mask unit (mu):** the coordinate unit of generated mask geometry. One internal unit equals `scaleFactor` mask units; one mask unit equals `10 / expander` nanometres (i.e. one centimicron divided by `expander`).
- **Mask layer:** one entry declared by `layer`, `templayer` or `labellayer`, identified by its 0-based declaration index within the style.

## Requirements

### Requirement: cifoutput section grammar

The `cifoutput` section SHALL be parsed line by line as whitespace-separated tokens according to the following grammar; a line's first token selects the statement and keywords are matched exactly (case-sensitive) except where a prefix rule is stated.

Compatibility: exact

```ebnf
cifoutput-section = "cifoutput" NL , { style-block } , "end" NL ;
style-block   = style-line , { style-stmt } ;
style-line    = "style" , name , [ "variant" , variant-list ] NL ;   (* 2 or 4 tokens *)
variant-list  = suffix , { "," , suffix } ;                           (* no spaces *)
style-stmt    = scale-line | units-line | step-line | grid-line | variant-line
              | layer-line | labels-line | gds-line | minw-line | render-line
              | options-line | op-line ;
scale-line    = "scalefactor" , number , [ token ] , [ unit-word ] NL ; (* 2..4 tokens *)
unit-word     = token starting with "nanom" | token starting with "angstr" ;
units-line    = "units" , token NL ;                    (* "angstr*" sets angstrom GDS units *)
step-line     = "stepsize" , integer NL ;
grid-line     = token-starting-with-"grid" , integer NL ;  (* usually "gridlimit" *)
variant-line  = token-starting-with-"variant" , ( "*" | suffix , { "," , suffix } ) NL ;
layer-line    = ( "layer" | "templayer" | "labellayer" ) , name , [ layer-list ] NL ;
labels-line   = "labels" , type-list , [ "port" | "noport" | "text" ] NL ;
gds-line      = ( "calma" | token-starting-with-"gds" ) , integer , integer NL ;
minw-line     = "min-width" , integer NL ;
render-line   = "render" , name , style , float , float NL ;
options-line  = "options" , option , { option } NL ;
option        = "calma-permissive-labels" | "set-minimum-grid" | "grow-euclidean"
              | "see-no-vendor" | "no-errors" | "string-limit" | other-token ;
op-line       = op-keyword , { argument } NL ;      (* see "Operator statement arguments" *)
layer-list    = layer-item , { "," , layer-item } ;  (* no embedded whitespace *)
layer-item    = [ "*" ] , name | "(" , type-list , ")" ;
```

Statements other than `style` are ignored unless the style being loaded is the current style (see "Style loading and variants"). Any first token that is not a recognised statement or operator keyword SHALL produce the tech error `Unknown statement "<tok>".` and the line is ignored.

(Magic ref: cif/CIFtech.c:558-1157)

#### Scenario: Minimal style

- **GIVEN** a `cifoutput` section containing `style gdsii`, `scalefactor 10 nanometers`, `layer M1 metal1`, `calma 68 20`
- **WHEN** the technology is loaded
- **THEN** one style named `gdsii` exists with one mask layer `M1` whose operation list is a single OR of type `metal1`
- **AND** the layer's GDS layer/datatype are 68/20

#### Scenario: Unknown statement

- **GIVEN** a line `foo 3` inside the current style
- **WHEN** the section is parsed
- **THEN** a tech error `Unknown statement "foo".` is reported and parsing continues with the next line

### Requirement: Style declaration and the style list

A `style` statement SHALL register output style names in declaration order, with `style <name>` registering `<name>` and `style <base> variant <s1>,<s2>,...` registering the names `<base><s1>`, `<base><s2>`, ... (string concatenation, in list order); the first registered name of the whole section is the default style.

Compatibility: exact

- A `style` line with a token count other than 2, or other than 4 with third token beginning `variant`, is an error (`Wrong number of arguments in style statement.`).
- Before registering, the existing list is scanned and the line is considered already present, in both `magic-compat` settings (Q-MASK-10, resolution: fix): for `style <name>`, if some existing name equals `<name>`; for a variant line, if some existing name equals `<base><si>` for one of its variants. In that case nothing is registered. When a new name is a strict prefix of an existing name, a tech warning names both; Magic's prefix comparison (length `strlen(argv[1])`) would drop the new style silently. Styles that Magic registers keep the same list order and output; a style Magic drops is appended at the end of the list.
- For a variant style, the "default" of that line is the first variant name.
- If no style has yet been loaded at the time the first `style` line is read, the first registered name becomes the style being loaded.
- Statements appearing before any `style` line make the section fail (the line handler reports failure) — there is no implicit style named `default`. See Q-MASK-17.

(Magic ref: cif/CIFtech.c:582-715)

#### Scenario: Variant names

- **GIVEN** `style sky130() variant (vendor),(),(fill)`
- **WHEN** parsed in a section with no earlier styles
- **THEN** the registered names in order are `sky130(vendor)`, `sky130()`, `sky130(fill)`
- **AND** the style loaded by default is `sky130(vendor)`

#### Scenario: Prefix shadowing (verify against Magic)

- **GIVEN** the section declares `style gdsfoo` and later `style gds`
- **WHEN** parsed
- **THEN** both are registered, in the order `gdsfoo`, `gds`, and a tech warning reports that `gds` is a prefix of `gdsfoo` (Magic registers only `gdsfoo`)

#### Scenario: Variant re-read is not registered twice

- **GIVEN** the section declares `style sky130() variant (vendor),()` and later `style sky130() variant (vendor)`
- **WHEN** parsed
- **THEN** the second line registers nothing, because `sky130(vendor)` is already present

### Requirement: Style loading and variant-conditional statements

Exactly one output style SHALL be loaded at a time; loading style S re-parses the `cifoutput` section and applies only the statements that belong to S, where membership is governed by `style` lines and `variant` lines as specified below.

Compatibility: exact

Loading state machine (states NOT_LOADED, PENDING, SUSPENDED, LOADED):

1. Initially the target name S is set and the state is NOT_LOADED (on the very first tech load, S is the first registered name and the state becomes PENDING at the first `style` line).
2. On a `style` line while PENDING or SUSPENDED: state becomes LOADED (the style's block has ended; all further lines are ignored).
3. On a `style <name>` line while NOT_LOADED: if `<name>` equals S exactly, state becomes PENDING.
4. On a `style <base> variant <list>` line while NOT_LOADED: if S begins with `<base>` and the remainder of S equals one of the listed suffixes exactly, state becomes PENDING.
5. `scalefactor`, `units`, `stepsize` and `grid*` statements are applied when the state is PENDING or SUSPENDED (they are not variant-conditional).
6. A `variant <list>` line (state PENDING or SUSPENDED) sets the state to PENDING if the list contains `*`, or contains a suffix `v` such that `len(S) > len(v)` and S ends with `v`; otherwise the state becomes SUSPENDED. Spaces immediately before a comma inside the list are trimmed.
7. All other statements are applied only while PENDING.

(Magic ref: cif/CIFtech.c:675-715, cif/CIFtech.c:794-838, cif/CIFtech.c:2278-2300)

#### Scenario: Variant-conditional layer

- **GIVEN** `style s variant A,B`, then `scalefactor 10`, `variant A`, `layer X metal1`, `variant *`, `layer Y metal2`
- **WHEN** style `sB` is loaded
- **THEN** the style has `scaleFactor` 10 and only layer `Y`

#### Scenario: Variant with empty remainder does not match (verify against Magic)

- **GIVEN** style name `s()` and a line `variant s()`
- **WHEN** evaluated
- **THEN** the variant does not match because `len(S) - len(v)` is 0, and the state becomes SUSPENDED

### Requirement: Output style selection

Selecting an output style by name SHALL use exact match first and otherwise unique prefix match over the registered names, and SHALL then load that style and re-apply the current lambda scaling.

Compatibility: behavioral

- An exact match wins even if other names share the prefix.
- More than one prefix match without an exact match is an error ("ambiguous"); no style change occurs.
- No match is an error; no style change occurs.
- After loading, the style is rescaled by the database lambda ratio `(lambdaNum, lambdaDen)` as in "Unit normalisation" with `n = lambdaNum`, `d = lambdaDen`.
- If the DRC section references mask layers, the DRC style is reloaded (see capability `drc-cif-rules` / DRC capabilities).

The user-visible command is `cif ostyle <name>` (and `gds` equivalents); message texts are informative.

(Magic ref: cif/CIFmain.c:257-301, cif/CIFtech.c:2278-2300)

#### Scenario: Unique prefix

- **GIVEN** registered styles `gdsii`, `cif`
- **WHEN** the user selects `gd`
- **THEN** style `gdsii` is loaded

#### Scenario: Ambiguous prefix

- **GIVEN** registered styles `sky130(vendor)` and `sky130(fill)`
- **WHEN** the user selects `sky130(`
- **THEN** an ambiguity error is reported and the current style is unchanged

### Requirement: scalefactor statement

`scalefactor <scale> [<ignored>] [nanometers|angstroms]` SHALL set the style's raw `scaleFactor` and `expander` so that one internal unit equals `scaleFactor / expander` centimicrons.

Compatibility: exact

Parsing of `<scale>`:

- No `.`: `scaleFactor = atoi(scale)`, `expander = 1`.
- With `.`: let `I` = integer value of the text before `.`, `F` = integer value of the text after `.`, `p` = number of characters after `.`; `n = I * 10^p + F`, `d = 10^p`; reduce `n/d` by their GCD; `scaleFactor = n`, `expander = d`.
- If the token count is 3 or 4 and the last token begins with `nanom`, `expander *= 10`; if it begins with `angstr`, `expander *= 100`. Any other extra token (historic reducer value or `calmaonly`) is ignored.
- Token count outside 2..4 is a wrong-number-of-arguments error.
- If the resulting `scaleFactor <= 0` it is set to 0 and the error `Scalefactor must be a strictly positive value.` is reported.
- The CIF reducer is reset to 1 (it is recomputed at finalisation).
- If at finalisation `scaleFactor <= 0`, the error `No valid scale factor was given for <style> CIF.` is reported and `scaleFactor` is forced to 1 (no other finalisation occurs).

(Magic ref: cif/CIFtech.c:507-543, cif/CIFtech.c:719-746, cif/CIFtech.c:1880-1888)

#### Scenario: Decimal scale

- **GIVEN** `scalefactor 4.5`
- **WHEN** parsed
- **THEN** raw `scaleFactor = 9` and `expander = 2` (45/10 reduced)

#### Scenario: Nanometres

- **GIVEN** `scalefactor 10 nanometers`
- **WHEN** parsed
- **THEN** raw `scaleFactor = 10`, `expander = 10` (one internal unit = 10 nm)

#### Scenario: Decimal angstroms

- **GIVEN** `scalefactor 2.5 angstroms`
- **WHEN** parsed
- **THEN** raw `scaleFactor = 5`, `expander = 2 * 100 = 200`

### Requirement: units, stepsize and gridlimit statements

The style SHALL accept `units angstroms` (GDS database unit flag), `stepsize <n>` (hierarchical chunk size in internal units) and `gridlimit <n>` (minimum manufacturing grid in mask units) with the validation below.

Compatibility: exact

- `units <w>`: exactly 2 tokens; if `<w>` begins with `angstr` the style's angstrom flag is set (GDS database unit = 1 Å instead of 1 nm; see capability `gds-write`). Any other word is silently ignored. CIF output units are unaffected.
- `stepsize <n>`: exactly 2 tokens; `n = atoi`; `n <= 0` reports `Step size must be positive integer.` and leaves 0 (meaning "no user step size"). Used by capability `mask-hierarchical-generation`.
- Any statement whose first token begins with `grid`: if the second token is an integer string, `gridLimit = atoi`; a negative value reports an error and is set to 0; a non-integer reports `Unable to parse grid limit value.`. Default 0 (no limit). The value is stored as written and is NOT rescaled by unit normalisation; it is applied directly to mask-unit coordinates by the operators, in both `magic-compat` settings (Q-MASK-8, resolution: replicate). Because the grid-scale guard reads the same value as a multiple of the expander, the two readings agree only when the expander is 10; after each unit normalisation the style loader warns when a style with a non-zero gridlimit has an expander other than 10.

(Magic ref: cif/CIFtech.c:754-790)

#### Scenario: gridlimit stored verbatim

- **GIVEN** `scalefactor 10 nanometers` and `gridlimit 5`
- **WHEN** the style is finalised (scaleFactor and expander both reduced to 1)
- **THEN** `gridLimit` remains 5 and operators snap mask-unit coordinates to multiples of 5 mu (= 50 nm)
- **AND** a warning reports that gridlimit is applied with expander 1, not 10

### Requirement: Mask layer declarations

`layer`, `templayer` and `labellayer` statements SHALL each append a new mask layer with the given name at the next index, initially with an empty operation list, no GDS mapping (layer = datatype = -1), `min-width` 0 and flags: `templayer` → temporary (never written), `labellayer` → label layer (its geometry is written as text labels by the writers), `layer` → ordinary output layer.

Compatibility: exact

- Token count must be 2 or 3; otherwise wrong-number-of-arguments.
- With 3 tokens, the third token is a layer list (see "Layer list syntax") and becomes the first operation: an OR of those layers.
- The maximum number of mask layers per style is `TT_MAXTYPES - 1` (the database's maximum tile-type count minus one); exceeding it reports an error and the line is ignored.
- When a new layer is declared and the previous layer has neither any operation nor any `labels` statement, the warning `Layer "<name>" contains no material.` is reported (also checked at section end).
- Layer names need not be unique; see "Layer list syntax" for how references resolve.

(Magic ref: cif/CIFtech.c:843-901, cif/CIFtech.c:1866-1870)

#### Scenario: Inline OR

- **GIVEN** `templayer XPOLY poly,polycontact`
- **WHEN** parsed
- **THEN** mask layer `XPOLY` is temporary and its first operation is `or poly,polycontact`

#### Scenario: Empty layer warning

- **GIVEN** `layer A` followed directly by `layer B metal1`
- **WHEN** parsed
- **THEN** the warning `Layer "A" contains no material.` is reported and `A` remains defined (empty)

### Requirement: Layer list syntax

A layer list SHALL be a comma-separated list whose items each resolve to either a set of database tile types or a set of previously declared mask layers of the current style, using the resolution rules below.

Compatibility: exact

For each item (consecutive commas are skipped):

1. An item beginning with `(` is copied up to `)` and then to the next comma; it is resolved as a database type-list expression (see capability `tech-file-format`) and always denotes tile types.
2. An item beginning with `*` (after the optional parenthesis rule) sets the *all-residues* flag; the `*` is removed.
3. The item name is resolved as a database type name. If it resolves to exactly one type, that type (or for `*`, that type plus every stacked contact type whose residues include it) is added to the type set.
4. Independently, every mask layer declared so far in this style, other than the layer currently being defined, whose name is exactly equal to the item, is added to the mask-layer set.
5. If the name is ambiguous as a type name, or matches both a type and at least one mask layer, the error `Ambiguous layer (type) "<item>".` is reported and the item is skipped.
6. If neither matches, the name is looked up as a type alias; if found its type set is added; otherwise the error `Unrecognized layer (type) "<item>".` is reported.
7. The type `space` is rejected (`"Space" layer not permitted in CIF rules.`) except where a statement explicitly allows it (bloat neighbour lists).

Because only earlier layers exist at parse time, an operation can only reference layers declared before the current layer. If several earlier layers share the referenced name, all of them are referenced (their union).

(Magic ref: cif/CIFtech.c:227-367)

#### Scenario: Mixed list

- **GIVEN** an earlier templayer `XN` and a line `or XN,ndiff`
- **WHEN** parsed
- **THEN** the operation's mask-layer set is `{XN}` and its type set is `{ndiff}`

#### Scenario: Residue expansion

- **GIVEN** a technology with stacked contact `m2c` whose residues include `m1c`
- **WHEN** the list `*m1c` is parsed
- **THEN** the type set is `{m1c, m2c}`

### Requirement: Automatic stacked-contact inclusion

At style finalisation, for every operation whose type set contains a contact type `c`, every stacked (non-user) contact type whose residue set contains `c` SHALL be added to that operation's type set.

Compatibility: exact

(Magic ref: cif/CIFtech.c:1893-1910)

#### Scenario: Via stack

- **GIVEN** `layer VIA1 via1` and a database stacked type `via1/via2` whose residues include `via1`
- **WHEN** the style is finalised
- **THEN** the OR operation of `VIA1` also selects tiles of the stacked type

### Requirement: labels statement and label layer mapping

`labels <types> [port|noport|text]` SHALL associate the current (most recently declared) mask layer with labels attached to each listed database type, maintaining three per-type tables — text layer for ordinary labels (`labelLayer`), text layer for port labels (`portText`), and geometry layer for port labels (`portLayer`) — all initialised to "none" (-1).

Compatibility: exact

For each type `t` in `<types>` (parsed as a database type-list expression), with `L` the current layer index:

| Option | Effect |
|---|---|
| (none) | `labelLayer[t] = L`; `portText[t] = L` |
| `text` | same as no option, in both `magic-compat` settings (Q-MASK-11, resolution: replicate) |
| `noport` | `labelLayer[t] = L` only |
| `port` | `portLayer[t] = L`; if `portText[t]` is still -1 then `portText[t] = L` |

Errors: no current layer → `Must define layer before giving labels it holds.` (ignored); current layer is temporary → warning only (mapping still applied); unknown option → error and wrong-number-of-arguments. Option keywords are matched by prefix of length 4 (`port`), 6 (`noport`) and the exact word `text`. A later `labels` statement for the same type overwrites earlier assignments per the table. How the writers use these tables is specified in capabilities `gds-write` and `cif-format-io`.

(Magic ref: cif/CIFtech.c:903-960)

#### Scenario: Separate port geometry layer

- **GIVEN** layer index 3 `M1TXT` with `labels metal1`, then layer index 4 `M1PIN` with `labels metal1 port`
- **WHEN** parsed
- **THEN** `labelLayer[metal1] = 3`, `portText[metal1] = 3`, `portLayer[metal1] = 4`

#### Scenario: Port only

- **GIVEN** layer index 5 with `labels metal2 port` and no other labels statement for metal2
- **WHEN** parsed
- **THEN** `portLayer[metal2] = 5`, `portText[metal2] = 5`, `labelLayer[metal2] = -1`

### Requirement: GDS layer mapping statement

`calma <layer> <datatype>` or any statement starting with `gds` followed by two numbers SHALL set the current mask layer's GDS layer number and datatype.

Compatibility: exact

- Requires a current layer (else error, line ignored) and exactly 3 tokens.
- Each number must consist of decimal digits only and lie in 0..255; otherwise the error `Calma layer and type numbers must be 0 to 255.` is reported, but the values are still stored as `atoi(token)`.
- Assigning to a temporary layer reports a warning; the values are stored but temporary layers are never written.
- A layer whose GDS layer number is outside 0..255 (including the default -1) is not written to GDS (see capability `gds-write`).

(Magic ref: cif/CIFtech.c:962-977, cif/CIFtech.c:1631-1650, calma/calmaInt.h:117-119)

#### Scenario: Out of range still stored (verify against Magic)

- **GIVEN** `calma 300 0`
- **WHEN** parsed
- **THEN** an error is reported, the stored layer number is 300, and the layer is not written to GDS

### Requirement: min-width, render and options statements

The style SHALL accept `min-width <n>` (per-layer minimum width in mask units used for sliver growth during hierarchical generation; also enables sliver growth for the style), `render <layer> <style> <height> <thick>` (3-D display attributes only) and `options <opt>...` (style flags).

Compatibility: exact

- `min-width`: requires a current layer and exactly 2 tokens; stores `atoi` and sets the style's grow-slivers flag (semantics in capability `mask-hierarchical-generation`).
- `render`: exactly 5 tokens; applies to every layer with the exact name; has no effect on mask geometry (informative for Hefesto). It clears the "current layer" so subsequent operations without a new layer line are errors.
- `options`: at least 2 tokens; each recognised word sets a flag; unrecognised words are silently ignored:

| Word | Flag effect |
|---|---|
| `calma-permissive-labels` | GDS label text is not sanitised (see `gds-write`) |
| `set-minimum-grid` | after tech load, the lambda ratio is forced to `1 : scaleFactor / gridLimit` (integer division) — see Q-MASK-16 |
| `grow-euclidean` | `grow`, `shrink` and `bloat-or` on non-Manhattan edges use the Euclidean variant (see `mask-output-operators`) |
| `see-no-vendor` | accepted and stored, with no effect on generation: vendor paint is always copied (Q-MASK-401, resolution: replicate; capability `mask-hierarchical-generation`) |
| `no-errors` | mask-generation errors are not reported as feedback |
| `string-limit` | GDS strings limited to the historic 32-character limit (see `gds-write`) |

(Magic ref: cif/CIFtech.c:979-1060, utils/tech.c:806-812, cif/CIFmain.c:425)

#### Scenario: Unknown option ignored

- **GIVEN** `options grow-euclidean frobnicate`
- **WHEN** parsed
- **THEN** the grow-euclidean flag is set and no error is reported

### Requirement: Operator statement arguments

Every operator statement SHALL be validated and stored with the argument forms below (distances in the style's written units, before normalisation); the geometric semantics are defined in capability `mask-output-operators`.

Compatibility: exact

| Keyword | Arguments | Validation |
|---|---|---|
| `and`, `and-not`, `or` | `layers` | exactly 1 argument |
| `overlapping`, `nonoverlapping`, `interacting`, `noninteracting` | `layers` | exactly 1 argument |
| `grow`, `grow-min`, `grow-grid`, `shrink` | `d` | exactly 1; `d > 0` else `Grow/shrink distance must be greater than zero.` and line dropped |
| `close` | `[a]` | 0 or 1; absent → 0 (close all); present must be > 0 |
| `bridge` | `d w` | exactly 2; both > 0 |
| `bridge-lim` | `d w layers` | exactly 3; `d, w > 0` |
| `bloat-or`, `bloat-max`, `bloat-min` | `types (types2 dist)+` | ≥ 3 arguments and pairs complete; `types` must be database types; `types2` may be `*` (all types incl. space) or a type list (space allowed); mask layers in `types2` → error `Can't use templayers in bloat statement.`; `bloat-or` negative distance → error and distance 0 |
| `bloat-all` | `types types2 [d]` | 2 or 3 arguments; `types` may be types or mask layers; `types2` all types or all mask layers (mixing → error); optional `d > 0` (clip halo) |
| `squares` | `s` or `border size sep` | 1 arg: `s > 0` and even, giving border = s/2, size = s, sep = s; 3 args: border ≥ 0, size > 0, sep > 0 |
| `squares-grid` | `border size sep [gx [gy]]` | 3–5 args; grids > 0, default 1; `gy` defaults to `gx` |
| `slots` | `sb ss ssep [lb [ls lsep [off [start]]]]` | 3, 4, 6, 7 or 8 args (token counts 4,5,7,8,9); `sb ≥ 0`, `ss > 0`, `ssep > 0`, `lb ≥ 0`, `ls ≥ 0`, `lsep ≥ 0` and `> 0` if `ls > 0`, `off ≥ 0`, `start ≥ 0`; unspecified values 0. See Q-MASK-9 |
| `bbox` | `[top]` | 0 or 1; only the exact word `top` |
| `boundary` | — | no arguments |
| `net` | `name [layers]` | 1 or 2 |
| `tagged` | `text [layers]` | 1 or 2 |
| `mask-hints` | `name` | exactly 1 |
| `maxrect` | `[internal|external]` | prefix `ext` → external, prefix `int` → internal (default); other words error but default kept |
| `orthogonal` | `[fill|remove]` | exact words; default `remove` |
| `not-square` | — | (extra arguments are ignored) |

An operation statement without a current layer reports `Must define layer before specifying operation.`. Rejected statements are not appended. For `bloat-*`, after parsing, the distance of every type in `types` is forced to 0, and all of `types` ∪ named `types2` must lie in one database plane (else error and plane 0 is used). For `bloat-all` with database types the same single-plane rule applies; with mask layers the bloat is marked as operating on mask layers.

(Magic ref: cif/CIFtech.c:1064-1622)

#### Scenario: Single-argument squares

- **GIVEN** `squares 4`
- **WHEN** parsed
- **THEN** border = 2, size = 4, separation = 4, grid 1×1

#### Scenario: Odd single-argument squares rejected

- **GIVEN** `squares 5`
- **WHEN** parsed
- **THEN** the error `Squares must have positive even sizes.` is reported and no operation is added

#### Scenario: Bloat distances of source types zeroed

- **GIVEN** `bloat-or pdiff,ndiff * 20`
- **WHEN** parsed
- **THEN** the distance table maps every type to 20 except `pdiff` and `ndiff` which map to 0

### Requirement: squares-to-grid conversion by gridlimit

When a `squares` or `squares-grid` statement is parsed while the style's `gridLimit` is non-zero, each of its x and y grid values smaller than `gridLimit` SHALL be raised to `gridLimit` and the operation SHALL become `squares-grid`.

Compatibility: exact

The conversion uses the `gridLimit` value in effect when the statement is parsed (a later `gridlimit` line does not convert earlier statements).

(Magic ref: cif/CIFtech.c:1493-1507)

#### Scenario: Conversion

- **GIVEN** `gridlimit 5` followed by `squares 10 34 34`
- **WHEN** parsed
- **THEN** the operation is `squares-grid` with border 10, size 34, sep 34, grid 5×5

### Requirement: Unit normalisation

The style SHALL normalise its scale and every operator distance by the procedure `rescale(n, d)` below, invoked once at finalisation with `(1, 1)` and again whenever the internal-unit-to-lambda ratio changes (at tech load if lambda ≠ 1:1, on style selection with the current lambda, and on grid rescaling).

Compatibility: exact

```
rescale(n, d):
  if scaleFactor is odd:            n *= 2; d *= 2; oddSpace = true
  else if d is odd:
      oddSpace = false
      if any squares op has odd size or sep, or any slots op has odd
         lsize, lsep, ssize or ssep:  n *= 2; d *= 2   (oddSpace stays false)
  scaleFactor *= n; expander *= d
  g = expander
  for each op (in layer order, op order):
      if distance != 0: distance *= d; g = gcd(g, gcd(|distance|, expander))
      squares:  for v in (border, size, sep) if v != 0:
                   v *= d; w = |v|; if v is size or sep: w >>= 1
                   if oddSpace: w >>= 1;  g = gcd(g, gcd(w, expander))
      slots:    same for (sborder, ssize, ssep, lborder, lsize, lsep, offset, start),
                halving ssize, ssep, lsize, lsep
      squares-grid: (border,size,sep,gridx,gridy) v*=d; g = gcd(g, gcd(|v|, expander))
      bridge, bridge-lim: width *= d; g = gcd(g, gcd(|width|, expander))
      bloat-or/max/min/all: each non-zero per-type value *= d; g = gcd(...)
      (or, bbox, boundary, mask-hints, maxrect, orthogonal, net, tagged,
       overlapping-family: no client values)
  g = min(g, gcd(scaleFactor, expander))
  if g <= 1:
      magic-compat OFF: recompute the interaction radius
      return                    (magic-compat ON: radius NOT recomputed; Q-MASK-18)
  scaleFactor /= g; expander /= g
  every distance, squares/squares-grid/slots value, bloat-or/max/min value
  and bridge width is divided by g (bloat-all values are NOT divided)
  recompute the interaction radius
```

`gcd` is the positive greatest common divisor. Because `bloat-all` stores the value 1 per selected type and that value enters the GCD, a style containing `bloat-all` is never reduced when `d = 1`; this is replicated in both `magic-compat` settings (Q-MASK-13, resolution: replicate), which keeps the expander of the PDK styles at 10 and so the GDS UNITS record, the CIF DS scale and the meaning of gridlimit. `close` thresholds are stored as distances and enter the GCD as lengths in both settings; their own area scaling is defined by `close` in capability `mask-output-operators` (Q-MASK-2). With `magic-compat` ON, a call that returns at `g <= 1` keeps the radius computed before, as in Magic; with it OFF the radius is always recomputed at the end of the normalisation (Q-MASK-18, resolution: compat). The DRC section's mask-layer rule distances are rescaled in step (see DRC capabilities).

(Magic ref: cif/CIFtech.c:2424-2720, cif/CIFtech.c:1872-1878, cif/CIFtech.c:1912-1915, cif/CIFtech.c:2296, cif/CIFtech.c:2500-2502, cif/CIFtech.c:2601-2633, cif/CIFtech.c:2648-2649, cif/CIFtech.c:2689-2698, cif/CIFtech.c:1284-1294, cif/CIFtech.c:2022-2031, cif/CIFtech.c:2719, utils/tech.c:814-826)

#### Scenario: Nanometre style reduced

- **GIVEN** `scalefactor 10 nanometers` and a single operation `grow 50`
- **WHEN** finalised with `rescale(1,1)`
- **THEN** scaleFactor = 1, expander = 1, grow distance = 5 (1 mu = 10 nm)

#### Scenario: Odd scale doubling preserved by squares

- **GIVEN** `scalefactor 9`, `squares 12 44 44`
- **WHEN** finalised
- **THEN** scaleFactor = 18, expander = 2, squares border 12, size 44, sep 44 (half-centimicron units; reduction blocked because 44/2/2 = 11 is odd)

#### Scenario: Odd scale with everything even

- **GIVEN** `scalefactor 9`, `grow 30`
- **WHEN** finalised
- **THEN** after doubling (18, 2, grow 60) the GCD is 2 and the result is scaleFactor = 9, expander = 1, grow = 30

#### Scenario: bloat-all blocks reduction (verify against Magic)

- **GIVEN** `scalefactor 10 nanometers`, `grow 50`, and any `bloat-all` operation
- **WHEN** finalised
- **THEN** scaleFactor = 10, expander = 10, grow = 50

### Requirement: CIF reducer

At finalisation the style SHALL compute the CIF output reducer as the minimum, over all non-zero operator values, of `gcd(scaleFactor, value)` starting from `scaleFactor`, where squares and slots sizes/separations are halved first, so that CIF coordinates can be divided by it.

Compatibility: exact

Values considered: every non-zero distance; squares border, size/2, sep/2; slots sborder, ssize/2, ssep/2, lborder, lsize/2, lsep/2, offset, start; squares-grid border, size, sep, gridx, gridy; bridge width; per-type bloat values (all bloat kinds). Processing of a layer's operations stops early once the minimum reaches 1. The reducer is computed before the lambda rescale and is used only by CIF writing (capability `cif-format-io`). Operations whose stored client value is a small flag (the `interacting`, `nonoverlapping`, `noninteracting` operators) are not excluded from the per-type bloat branch in Magic; see Q-MASK-12.

(Magic ref: cif/CIFtech.c:1912-2040)

#### Scenario: Reducer

- **GIVEN** scaleFactor 100 and operations `grow 50`, `squares 20 40 40`
- **WHEN** finalised
- **THEN** reducer = min(100, gcd(100,50)=50, gcd(100,20)=20, gcd(100,20)=20) = 20

### Requirement: Interaction radius

Each loaded (or rescaled) style SHALL carry an interaction radius `R = floor(2 * max(maxG, maxS) / scaleFactor) + 1` in internal units, where `maxG` and `maxS` are the largest per-layer grow and shrink distances in mask units. The exact per-operator accumulation rules and the `scaleFactor <= 0` case are owned by capability `mask-hierarchical-generation` requirement "Halo radius", which calls this value the *halo radius*; this requirement only records that the value is part of the normalised style.

Compatibility: exact

The radius is used by capability `mask-hierarchical-generation` and to expand the search area of flat generation (`mask-output-operators`).

(Magic ref: cif/CIFtech.c:1668-1830)

#### Scenario: Radius

- **GIVEN** scaleFactor 9 (after normalisation), layer A `grow 30`, layer B `shrink 10`
- **WHEN** the radius is computed
- **THEN** maxG = 30, maxS = 10, R = floor(60 / 9) + 1 = 7 internal units

### Requirement: Contact cut size query

The style SHALL answer, for a database type T, the minimum contact dimension `size + 2 * border` (mask units) of the first `squares`, `squares-grid` or `slots` operation found as described below, or 0 if none.

Compatibility: exact

Search: for each layer in order, walk its leading operations while they are `or` operations with no mask-layer references; if such an `or` selects T, scan the following operations: the first `squares`/`squares-grid` returns `(size, border, sep)`; the first `slots` returns `(ssize, sborder, ssep)`; any operation other than `or` or `and-not` stops the scan of that layer. This value is used by other capabilities (e.g. contact sizing in extraction and LEF).

(Magic ref: cif/CIFtech.c:2327-2393)

#### Scenario: Contact size

- **GIVEN** `layer CONT pc` then `squares 5 17 17` in mask units after normalisation
- **WHEN** the contact size of `pc` is queried
- **THEN** the result is 27 with edge 17, border 5, spacing 17

### Requirement: Mask generation error reporting

When mask generation encounters a reportable problem (e.g. no room for contact cuts, tile inverted by shrink), the system SHALL record a feedback entry on the cell being generated with the message `CIF error in cell <cell>, layer <layer>: <message>`, unless the style's `no-errors` option is set or no cell is designated for errors.

Compatibility: informative

(Magic ref: cif/CIFmain.c:418-437)

#### Scenario: Suppressed errors

- **GIVEN** a style with `options no-errors` and a contact area too small for a cut
- **WHEN** mask geometry is generated
- **THEN** no feedback entry is created
