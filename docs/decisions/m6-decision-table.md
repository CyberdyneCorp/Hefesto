# M6 decision table: the 99 open questions blocking LEF/DEF and OASIS

M6 ("LEF/DEF and OASIS", docs/roadmap.md) cannot start until every open question its specs cite has a `Resolution:` line. `python3 scripts/check_milestone_readiness.py --milestone M6` lists 99: 65 in `docs/decisions/open-questions/LEFDEF.md` and 34 in `OASIS.md`. Each LEFDEF entry was checked against Magic 8.3.694 (fc5daa9e, `/Users/leonardo/trabalho/magic`, read-only; `lef/` is unchanged up to HEAD 9c83a9a6). Magic has no OASIS code, so the OASIS entries were checked against the gds-read and gds-write code they mirror and against KLayout (master `dbOASISReader.cc`, `dbOASISWriter.cc`) and gdstk (`library.cpp`); the SEMI P39 text was not available. Each entry now carries `Status: open (recommendation drafted 2026-11-13)` and a rewritten `Recommendation:` line with the full reasoning; corrected evidence is marked `(corrected 2026-11-13: ...)`, and evidence that was right gets a `Verified 2026-11-13:` line. **Nothing here is approved yet**, and no entry has a `Resolution:` line.

The standing rule for Magic bugs and quirks is compat: `magic-compat` ON copies Magic, OFF does the intended thing. Most LEFDEF rows follow it. Where they do not, the row says why: replicate when Magic defines no corrected behaviour or the quirk is a formatting or arithmetic choice that byte identity needs, fix when the quirk loses data, is undefined behaviour, or has no Magic output to match. Most OASIS rows have no Magic output at all: they are fix (format semantics, aligned with KLayout and gdstk), replicate (the GDS-equivalence rule inherits gds-read or gds-write behaviour), or other (a Hefesto default).

Recommendation kinds (X-2, docs/decisions/cross-cutting.md):

- **replicate**: do what Magic does, in both modes.
- **compat**: replicate with `magic-compat` ON and use the corrected behavior with it OFF (tier 2).
- **fix**: corrected behavior in both modes (tier 1, or tier 3 when there is no Magic output to match).
- **platform**: a Hefesto platform decision (X-3/X-4).
- **other**: none of the above (here: Hefesto OASIS defaults and format choices).

## Judgement items

These three involve scope or product trade-offs that the standing rule does not settle. Options are listed with the recommended one first.

1. **Q-OASIS-2: Buy the SEMI P39 text to confirm the format details that rest on KLayout and gdstk parity**
   Q-OASIS-1, 2, 3, 4, 5, 6, 7, 8, 9, 11 and 66 are drafted from KLayout and gdstk source and close without P39, because a reader has to accept what those tools write anyway. The remaining risk is a detail where KLayout and gdstk both differ from the standard and a commercial reader follows the standard (validation coverage and Q-OASIS-6's PROPERTY association, where the spec now follows KLayout rather than the P39 clause, are the most exposed). Golden cases OASIS-012, OASIS-030 and OASIS-061 with commercial files catch that, but only if such files are obtained.
   - (a) **Recommended.** Keep the drafts as they are, relying on KLayout and gdstk parity plus the commercial-file goldens (OASIS-012, OASIS-030, OASIS-061) as release gates
   - (b) Buy SEMI P39 and check the drafts against it before M6 starts; any difference becomes a spec correction
2. **Q-OASIS-35: GDS output of read-only OASIS cells**
   A cell read from OASIS with `readonly` holds only representational content (the layers `cifinput` maps). Generating GDS from it drops every unmapped vendor layer, which is the data loss the read-only flag exists to prevent. Transcoding avoids that but adds a GDS-side requirement and its tests to M6.
   - (a) **Recommended.** Transcode OASIS to GDS in M6: add a gds-write requirement that decodes the cell from its OASIS file and writes GDS elements, failing with an error on elements GDS cannot express
   - (b) Refuse `gds write` for a hierarchy with OASIS_FILE cells in M6 (error naming them) and add transcoding later
   - (c) Generate GDS from the representational layout with a warning listing unmapped layers (loses vendor data)
3. **Q-OASIS-90: Shared or independent `gds` and `oasis` settings**
   PDK and flow scripts set `gds` read options (readonly, flatglob, rescale) before reading vendor data. With independent settings, a script that switches to `oasis read` silently imports differently; with shared settings, `oasis` is no longer configurable on its own for reading. The PDK script survey the entry asks for could not be done here (no PDK scripts in the workspace).
   - (a) **Recommended.** Share the read-only settings with `gds` (`drccheck`, `flatten`, `flatglob`, `maskhints`, `noduplicates`, `ordering`, `polygon subcells`, `path subcells`, `readonly`, `rescale`, `savepaths`, `unique` and `warning`) and keep the rest independent (`abstract`, `addendum`, `arrays`, `compress`, `contacts`, `labels`, `library`, `lower`, `merge` and `undefined` (write-side), `datestamp` and `nodatestamp` (used on both sides by `gds`; a `gds datestamp` set for reproducible GDS output must not change OASIS cell timestamps), and `magscale` (no effect on OASIS))
   - (b) Fully independent settings (the current spec)
   - (c) Independent settings, with `oasis` values copied from `gds` at first use

## Effort and scope notes

- **Shared generators.** Several OFF paths reuse one piece of code: the DEF generated-via generator also builds LEF via-rule vias (Q-LEFDEF-7) and the DEF VIAS ORIGIN/OFFSET/PATTERN properties (Q-LEFDEF-62); lef-read's polygon decomposition also paints DEF pin and route polygons (Q-LEFDEF-66, Q-LEFDEF-70). Build each once.
- **Q-LEFDEF-1 (tier-1 part).** With a 1:1 grid and no `lef read` that sets WIDTH, Magic's `def read` paints no regular-net wire at all. The both-modes fix (DRC default width) changes every such DEF import relative to Magic; the golden cases for DEF read on PDK techs must be captured with LEF loaded first, or record the deviation.
- **Q-OASIS-35 (judgement item 2).** Transcoding OASIS read-only cells to GDS adds a gds-write requirement and its golden cases to M6. Option (b) keeps M6 smaller and loses no data, at the cost of refusing a mixed-format stream-out.

## Decision table

Each row's full text is the entry's `Recommendation:` line. Where the draft changes the entry's original recommendation, that line ends with `(corrected 2026-11-13, was ...)`.

### LEF technology section and LEF read

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-LEFDEF-1 | Route width and spacing defaults are effectively never filled | **compat** | ON: Magic's scaled sentinels and partial fill; OFF: DRC defaults whenever unset; def read never paints a non-positive width (fix, both modes) |
| Q-LEFDEF-2 | Default spacing written into the width field (read-side consequence) | **compat** | ON: default spacing overwrites the width; OFF: assigned to spacing |
| Q-LEFDEF-3 | Secondary obstruction type and contact area carry over between LAYER statements | **compat** | ON: secondary type (OBS) and cut size carry over; OFF: both reset at every LAYER |
| Q-LEFDEF-6 | Last SPACING statement in a LEF LAYER wins | **compat** | ON: last SPACING wins; OFF: first unqualified SPACING, else the minimum |
| Q-LEFDEF-7 | Unsupported LEF geometry constructs | **compat** | ON: skip via rules, RECT MASK/ITERATE, VIA POLYGON; OFF: support them (shared via generator) |
| Q-LEFDEF-10 | NONDEFAULTRULE statements that leave `;` swallow the next statement | **compat** | ON: unread `;` swallows the next statement; OFF: consume through `;`, skip known VIA bodies |
| Q-LEFDEF-11 | `lef read -annotate` still creates new cells | **compat** | ON: -annotate still creates unknown macros; OFF: skips them, as documented |
| Q-LEFDEF-12 | ORIGIN shifts geometry but not FIXED_BBOX | **compat** | ON: geometry at g-ORIGIN-F, box (F,F+SIZE); OFF: LEF's frame (g-F, box shifted by -ORIGIN-F) |
| Q-LEFDEF-13 | `DIRECTION OUTPUT TRISTATE` reads as OUTPUT | **compat** | ON: reads as output; OFF: tristate |
| Q-LEFDEF-14 | USE values not recognized on read | **compat** | ON: LEF 5.8 uses only; OFF: also SCAN, RESET, TIEOFF |
| Q-LEFDEF-15 | PATH and VIA pin/obstruction geometry ignored | **compat** | ON: PATH and VIA geometry ignored; OFF: painted, with port labels |
| Q-LEFDEF-20 | Ignored layers in VIA definitions report errors | **compat** | ON: per-RECT error; OFF: silent skip (messages only) |
| Q-LEFDEF-21 | LEF annotation alters existing layout and non-port labels | **compat** | ON: annotation paints and converts non-port labels; OFF: labels only |
| Q-LEFDEF-22 | LEF read is not undoable | **platform** | lef read and def read are one undoable transaction |
| Q-LEFDEF-23 | Polygon port labels from multi-plane types | **replicate** | port labels from the polygon's single decomposition (identical output) |

### LEF write

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-LEFDEF-31 | Default route spacing overwrites width | **compat** | ON: what Magic stores; OFF: filled DRC width, spacing and DIRECTION |
| Q-LEFDEF-32 | CUT AREA value and keyword | **compat** | ON: `CUT AREA` (4x area); OFF: standard `WIDTH` |
| Q-LEFDEF-33 | SITE size uses the wrong cell's FIXED_BBOX | **compat** | ON: written cell's FIXED_BBOX; OFF: the site cell's own |
| Q-LEFDEF-34 | Header written only for the edit root; LEF database units are stale otherwise | **compat** | header rule kept; ON: session-sticky units; OFF: recomputed per write |
| Q-LEFDEF-35 | Name legalization is ineffective | **replicate** | names verbatim; warn on space, `;`, `#`, newline |
| Q-LEFDEF-36 | Unhandled port class/use values give an empty keyword | **compat** | ON: empty keywords; OFF: `USE SIGNAL`, no DIRECTION for undefined classes |
| Q-LEFDEF-37 | -pinonly larger than -hide setback has no effect | **compat** | ON: no-op; OFF: removal applied when p < d |
| Q-LEFDEF-38 | Setback strips copy whole tiles | **compat** | ON: whole tiles; OFF: clipped to the strips |
| Q-LEFDEF-39 | Split tile with both halves writable | **compat** | ON: first half only; OFF: both halves |
| Q-LEFDEF-45 | float32 arithmetic in coordinate output | **replicate** | float32 lefPrint arithmetic, `-0.000` kept |
| Q-LEFDEF-46 | Pins without LEF geometry are dropped silently | **replicate** | no PIN record, plus a warning |
| Q-LEFDEF-47 | Too-large -hide distance silently disables hiding | **replicate** | full detail, plus a warning |
| Q-LEFDEF-50 | Ports sharing an index with different names are merged | **replicate** | one PIN per index, error per extra name |
| Q-LEFDEF-51 | Pin removal under -hide -pinonly p is overridden | **compat** | ON: whole chunk painted back; OFF: chunk clipped to the p ring plus the label |
| Q-LEFDEF-52 | Cut layers are written as whole contact areas | **replicate** | whole contact areas on cut layers |

### DEF read

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-LEFDEF-60 | DEF "generated via" flag is never reset between vias | **compat** | ON: flag persists through the section; OFF: reset per via |
| Q-LEFDEF-62 | VIAS ORIGIN/OFFSET/PATTERN/POLYGON/MASK ignored | **compat** | ON: ignored with a message; OFF: implemented per DEF 5.8 |
| Q-LEFDEF-63 | Generated via primary type is the top metal; layer switch fails when routing on the top layer | **compat** | ON: route on the top layer stays there; OFF: switch to the via's other layer |
| Q-LEFDEF-64 | Pin DIRECTION/USE persist across pins | **compat** | ON: carry over between pins; OFF: reset per pin |
| Q-LEFDEF-65 | Pin rectangle list and layer state leak between pins | **compat** | ON: leftover rectangles painted with the next pin; OFF: per-port collection |
| Q-LEFDEF-66 | Pin POLYGON and VIA ignored | **compat** | ON: ignored with a message; OFF: painted and labelled |
| Q-LEFDEF-69 | Unknown special-net "+" keyword swallows the next net | **compat** | ON: next net swallowed; OFF: skip to the next `+` or `;` |
| Q-LEFDEF-70 | POLYGON in routes partially parsed as points | **compat** | ON: vertices become wire points; OFF: painted as a polygon |
| Q-LEFDEF-71 | VIRTUAL route points are connected with wire | **compat** | ON: wire painted; OFF: reference point only |
| Q-LEFDEF-72 | Via orientation and MASK tokens in routes | **compat** | ON: per-token messages, unrotated via; OFF: MASK silent, orientation applied |
| Q-LEFDEF-73 | Route RECT uses truncating division and undefined reference | **compat** | ON: trunc, OFF: floor; undefined reference and failed parse fixed in both modes |
| Q-LEFDEF-74 | Net NONDEFAULTRULE applies only to the next routing statement | **compat** | ON: next routing statement only; OFF: whole net |
| Q-LEFDEF-77 | Polygonal DIEAREA uses only the first two points | **compat** | ON: first two points; OFF: bounding box of all vertices |
| Q-LEFDEF-78 | Fallback design cell name is the raw command argument | **compat** | ON: raw argument; OFF: basename without `.def` |
| Q-LEFDEF-79 | UNITS multiplies cumulatively and accepts integers only | **compat** | ON: multiplies, integer prefix; OFF: sets, warns, strict integer |
| Q-LEFDEF-81 | Annotation terminal lookup uses the edit cell | **compat** | ON: edit cell, unexpanded area; OFF: design cell, area +1 |
| Q-LEFDEF-83 | Duplicate component instance names | **fix** | rename with the cell-file-format rule and warn |
| Q-LEFDEF-84 | Unsupported FILLS/SLOTS/STYLES sections recovered noisily | **compat** | ON: noisy recovery; OFF: section skipped with one message |
| Q-LEFDEF-85 | Placement of unplaced/no-placement components | **replicate** | origin with a warning (UNPLACED); identity otherwise |
| Q-LEFDEF-86 | def read requires a window/box context [platform] | **platform** | read works headless, no window needed |
| Q-LEFDEF-88 | Event restriction and timer-driven progress [platform] | **platform** | document edits blocked; platform progress service |

### DEF write

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-LEFDEF-91 | VIAS section writes uncounted via records | **compat** | ON: underscore filter, uncounted records; OFF: only this write's vias |
| Q-LEFDEF-93 | COMPONENTS placement subtracts the untransformed bbox origin; arrays do not | **compat** | ON: Magic's two formulas; OFF: T(R).ll for every instance |
| Q-LEFDEF-94 | Component count wrong for arrays with reversed index ranges | **compat** | ON: signed product; OFF: true element count |
| Q-LEFDEF-95 | Regular-net endpoint inset uses integer half width | **compat** | ON: integer half width; OFF: exact half width |
| Q-LEFDEF-96 | Dogleg test compares the previous y with x2 | **compat** | ON: compares y with x2; OFF: with y1 |
| Q-LEFDEF-97 | Column-count inaccuracies in net output | **replicate** | Magic's line breaks |
| Q-LEFDEF-98 | NONDEFAULTRULES writes the first layer entry repeatedly; WIREEXT unscaled | **compat** | ON: first entry repeated, unscaled WIREEXT; OFF: each entry, scaled |
| Q-LEFDEF-99 | Non-default rules persist across writes and include LEF/DEF-read rules | **compat** | ON: whole global table; OFF: only referenced rules, write-local |
| Q-LEFDEF-103 | PINS formatting defects | **compat** | ON: Magic's bytes; OFF: RESET/SCAN/TIEOFF written, newline fixed |
| Q-LEFDEF-105 | Asymmetric directional-surround probe | **compat** | ON: asymmetric probe; OFF: symmetric |
| Q-LEFDEF-107 | Inverted y warning in via cut generation | **compat** | ON: inverted y test; OFF: same as x (message only) |
| Q-LEFDEF-110 | Stale .ext used without check | **compat** | no re-extraction; OFF adds a staleness warning |
| Q-LEFDEF-111 | Naive contact-region merging | **compat** | ON: chain bounding box; OFF: exact rectangles |
| Q-LEFDEF-112 | Repeated geometry for connected seeds | **compat** | ON: duplicates emitted; OFF: each tile once per net |

### OASIS format

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-OASIS-1 | Non-minimal integer encodings | **fix** | accept non-minimal encodings; write minimal ones |
| Q-OASIS-2 | Validation signature coverage and byte order | **fix** | gdstk coverage and byte order; judgement item on buying P39 |
| Q-OASIS-3 | RECTANGLE square flag and modal geometry-h | **fix** | S=1 sets modal geometry-h (KLayout) |
| Q-OASIS-4 | CTRAPEZOID table, trapezoid formulas and modal w/h after CTRAPEZOID | **fix** | keep the table; modal w/h = decoded bounding box |
| Q-OASIS-5 | Modal reset at name records | **fix** | reset modal variables at every name record |
| Q-OASIS-6 | PROPERTY attachment after XYABSOLUTE/XYRELATIVE | **fix** | XYABSOLUTE/XYRELATIVE end attachment; PROPERTY goes to the cell (KLayout) |
| Q-OASIS-7 | Exact definition of strict mode | **fix** | keep the strict-mode set (matches KLayout) |
| Q-OASIS-8 | Table offsets pointing at a CBLOCK | **fix** | table offsets may point at a CBLOCK |
| Q-OASIS-9 | Records spanning a CBLOCK boundary | **fix** | tolerate records spanning a CBLOCK end |
| Q-OASIS-10 | Zero spacing in repetitions | **replicate** | zero-separation arrays as gds-read; coincident uses discarded by the duplicate rule (painted for flattened cells) |
| Q-OASIS-11 | Path extension modal values for flush and half-width schemes | **fix** | store the numeric extension (KLayout) |
| Q-OASIS-12 | Non-quarter-turn placement angles | **replicate** | gds-read quarter-turn rounding with an error |

### OASIS read

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-OASIS-30 | Cell timestamps without dates in OASIS | **replicate** | timestamp 0, as gds-read for an unknown date |
| Q-OASIS-31 | Circle approximation | **fix** | inscribed 64-gon with platform-identical trig |
| Q-OASIS-32 | Label presentation lost through OASIS | **other** | drop presentation, warn once per write; no private property in M6 |
| Q-OASIS-33 | Negative path extensions ignored | **compat** | follows Q-MASK-114: ON ignore negative extensions; OFF shorten |
| Q-OASIS-34 | Name for a cell refnum with no CELLNAME | **fix** | `#<n>`, as KLayout |
| Q-OASIS-35 | GDS output of read-only OASIS cells | **fix** | transcode OASIS read-only cells to GDS (judgement item) |
| Q-OASIS-37 | User properties discarded on read | **replicate** | discard, as gds-read |
| Q-OASIS-38 | LAYERNAME not used for mapping | **replicate** | numbers only, as cifinput |
| Q-OASIS-39 | Layer or datatype numbers above 255 | **replicate** | wildcards only, as GDS; unmapped pairs reported once as gds-read's calmaLayerError (silent for read-only cells) |
| Q-OASIS-40 | Extension records | **fix** | keep; warn when payloads may refer to dropped XNAMEs |

### OASIS write

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-OASIS-60 | Character mapping applied to OASIS strings | **replicate** | gds-write mapping (per magic-compat, >127 as `X`, case folding with `lower` off), control characters as `_` |
| Q-OASIS-61 | Duplicate cell names after mapping | **fix** | follows Q-MASK-209: rename to `XXXXX<n>` |
| Q-OASIS-62 | 200-point polygon limit kept | **replicate** | keep the 200-point limit |
| Q-OASIS-63 | Transcoding vendor GDS paths that OASIS cannot express | **fix** | odd width is an error (inexpressible; as KLayout's default); even-width round ends as CIRCLEs (KLayout) |
| Q-OASIS-64 | Pinned DEFLATE implementation | **platform** | bundled, pinned zlib on all platforms |
| Q-OASIS-65 | Default validation scheme | **other** | default `crc32` |
| Q-OASIS-66 | Integer-width standard properties | **other** | declare 4/4, as KLayout |
| Q-OASIS-67 | Shape repetition detection changes element order | **other** | default `repetitions yes` |
| Q-OASIS-68 | Full-dump prefix randomness | **compat** | follows Q-MASK-211 |
| Q-OASIS-69 | Value type of S_GDS_PROPERTY | **other** | b-string; KLayout's narrowest type as fallback |

### OASIS command

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-OASIS-90 | Shared or independent settings for gds and oasis | **other** | share read-side settings, keep write-side ones separate (judgement item) |
| Q-OASIS-91 | No gzip-wrapped OASIS output | **other** | CBLOCK only; gzip-wrapped files still read |

## Counts

| Kind | Count | IDs |
|---|---|---|
| compat | 54 | L1, L2, L3, L6, L7, L10, L11, L12, L13, L14, L15, L20, L21, L31, L32, L33, L34, L36, L37, L38, L39, L51, L60, L62, L63, L64, L65, L66, L69, L70, L71, L72, L73, L74, L77, L78, L79, L81, L84, L91, L93, L94, L95, L96, L98, L99, L103, L105, L107, L110, L111, L112, O33, O68 |
| replicate | 17 | L23, L35, L45, L46, L47, L50, L52, L85, L97, O10, O12, O30, O37, O38, O39, O60, O62 |
| fix | 17 | L83, O1, O2, O3, O4, O5, O6, O7, O8, O9, O11, O31, O34, O35, O40, O61, O63 |
| platform | 4 | L22, L86, L88, O64 |
| other | 7 | O32, O65, O66, O67, O69, O90, O91 |
| total | 99 | |

L = Q-LEFDEF, O = Q-OASIS. The judgement items are counted under their recommended option (Q-OASIS-2 fix, Q-OASIS-35 fix, Q-OASIS-90 other). Q-LEFDEF-1 and Q-LEFDEF-73 are compat with a tier-1 part fixed in both modes; Q-OASIS-33, 61 and 68 are already settled by the approved Q-MASK-114, 209 and 211 resolutions.

## Notable changes to the entries' original recommendations

- **fix → compat** (the quirk changes output values but loses no data, so the standing rule applies): Q-LEFDEF-1 (DEF-read width still fixed in both modes), 2, 3, 10, 11, 13, 14, 20, 39, 60, 62, 63, 64, 65, 66, 69, 70, 71, 72, 74, 77, 78, 79, 81, 84, 91, 94, 98, 99, 107. Q-LEFDEF-33, 34 and 36 said "fix with an opt-in compatibility mode" or "always recompute"; they become plain compat.
- **replicate → compat** (Magic's intent is clear, so OFF does it): Q-LEFDEF-6, 7, 12, 15, 21, 31, 32, 37, 38, 51, 93, 95, 96, 103, 105, 110, 111, 112.
- **fix → compat plus a both-modes fix**: Q-LEFDEF-73 (rounding is compat; the undefined reference point and the half-parsed RECT are tier 1).
- **Unchanged kind, sharper wording**: Q-LEFDEF-22, 86, 88 (platform, now SHALL), Q-LEFDEF-46 and 47 (replicate plus a warning), Q-LEFDEF-83 (fix: reuse the .mag instance-id renaming; a recorded deviation, since Magic writes the duplicate id).
- **Q-OASIS-6**: the spec's "attach to the preceding element" becomes KLayout's "cell property". The P39 property-association clause was not consulted, so this rests on KLayout parity (judgement item 1).
- **Q-OASIS-63**: odd-width vendor paths make the write fail with an error instead of being widened (OASIS cannot express half-unit edges without changing the file unit); even-width round ends become CIRCLE caps (as KLayout) instead of flush.
- **Q-OASIS-90**: from "gather PDK usage" to a recommendation (judgement item 3).
- Every other OASIS entry keeps its original option; the "confirm against P39" caveats are collected in judgement item 1.

## Evidence corrections

39 entries had wrong line numbers or an incomplete or wrong description; each entry's `Evidence:` line now carries the correction. The ones that change the substance:

- **Q-LEFDEF-1**: scaling truncates toward zero, so the sentinel stays -1 and IS filled for d < n < 2d. More important: with a 1:1 grid and no LEF WIDTH, `def read` builds empty rectangles for every regular-net wire and paints nothing. That part is data loss and is fixed in both modes.
- **Q-LEFDEF-3**: the secondary obstruction type carries over only in OBS (PORT never sets it); the contact-area carry-over affects both.
- **Q-LEFDEF-7**: `RECT MASK n` and `RECT ITERATE` fail with "RECT requires 4 values"; LEF vias defined by a via rule get no geometry.
- **Q-LEFDEF-12**: geometry is off by 2*ORIGIN against LEF's rule, and Magic's own LEF write/read round trip shifts a cell whose boundary lower-left is not (0,0).
- **Q-LEFDEF-14**: SCAN, RESET and TIEOFF are DEF values, not LEF 5.8 ones, so accepting them in LEF is an extension.
- **Q-LEFDEF-21**: POLYGONs are painted into annotated cells without the connectivity check RECTs get.
- **Q-LEFDEF-31**: on a 1:1 grid `lef write -tech` writes no WIDTH, SPACING or DIRECTION at all; `WIDTH <default spacing>` appears only for d < n < 2d.
- **Q-LEFDEF-65**: the pending flag and current layer are reset per pin; only the rectangle list leaks, and rectangles left at the end of PINS are dropped.
- **Q-LEFDEF-79**: UNITS takes an integer prefix ("1000.5" reads as 1000 silently).
- **Q-LEFDEF-93**: the array path (T(R).ll) is the one that round-trips through `def read`; the non-array path is wrong for any orientation when R.ll is not (0,0).
- **Q-LEFDEF-98**: `extend` is stored in doubled internal units; only rules from LEF/DEF read show the defect.
- **Q-LEFDEF-99**: a write-created rule name that collides with a LEF-read rule reuses that rule.
- **Q-LEFDEF-103**: RESET, SCAN and TIEOFF are valid DEF PINS uses, and Magic's own reader accepts them.
- **Q-OASIS-6**: KLayout treats such a PROPERTY as a cell property; the spec did not match the reference reader.
- **Q-OASIS-10**: coincident single placements are discarded by gds-read's duplicate-use rule, not kept, unless the cell is flattened (`flatten`/`flatglob`), in which case they are painted.
- **Q-OASIS-35**: generating GDS from the representational layout drops every vendor layer `cifinput` does not map.
- **Q-OASIS-39**: GDS has the same 255 limit in Magic; OASIS behaves exactly as GDS.
- **Q-OASIS-63**: KLayout refuses odd widths (or rounds them down) and writes round ends as CIRCLE caps; the spec's choices both altered vendor geometry. Neither a PATH nor a POLYGON can carry an odd width exactly, since its edges lie on half units.
- **Q-OASIS-69**: KLayout's writer uses the narrowest string type, not b-string, for property values.
- Line numbers or refs only: Q-LEFDEF-11, 13, 15, 20, 34, 39, 50, 60, 62, 63, 96, 97, 105; Q-OASIS-12, 30, 32, 33, 37, 38, 62, 68.

Q-LEFDEF-63 appeared twice in the reviewer drafts with the same recommendation; the version with the corrected refs is used.

## Spec changes after approval

Not edited yet. After approval each spec gets the text below, and the entry's `Resolution:` line.

### lef-tech-section

- **Q-LEFDEF-1**: lef-tech-section 'Filling defaults after grid scaling': state that scaling truncates toward zero, so the sentinel stays -1 (and is filled) for d < n < 2d, becomes <= -2 for n >= 2d and 0 for n < d. Contact areas floor, so they are filled only for n < d. scalegrid and CIF-read rescaling do not refill. Split the requirement into ON (as now) and OFF (DRC defaults filled whenever unset). Add a scenario for 3:2. def-read width requirements (defRead.c:198-217, 366-392, 519-521): a non-positive layer width is replaced by the DRC default width in both modes (tier 1). lef-write -tech table (spec line ~530): fix the sentence 'With default initialization, the written WIDTH equals the DRC default spacing'. On a 1:1 grid, ON writes no WIDTH, SPACING or DIRECTION line, and that sentence holds only for d < n < 2d.
- **Q-LEFDEF-2**: lef-tech-section 'Filling defaults after grid scaling', ROUTE bullet: mark the width overwrite as magic-compat ON. Add the OFF rule (spacing <- DRC default spacing) and note that the overwrite can occur only for grid ratios d < n < 2d.

### lef-read

- **Q-LEFDEF-3**: lef-read 'LEF layer name resolution' (line ~94): say that the secondary type is only ever set in OBS (PORT never sets it). Mark the carry-over sentence as magic-compat ON. Add the OFF rule: both are reset at every LAYER. Correct the Magic refs to lefRead.c:697-750, 1220-1291, 1328-1331.
- **Q-LEFDEF-6**: lef-read LAYER statement table, SPACING row (line ~162): mark 'last wins' as magic-compat ON and add the OFF rule (first unqualified SPACING, otherwise the minimum first value). Note that the value is used only by lef write -tech.
- **Q-LEFDEF-7**: lef-read via-geometry requirement (line ~203): mark the unsupported list as magic-compat ON. Add OFF requirements for LEF generated vias (reusing the def-read generated-via geometry), RECT MASK, RECT ITERATE and VIA POLYGON. Correct the Magic refs to lefRead.c:2742-2777, 2895-2915, 867-936.
- **Q-LEFDEF-10**: lef-read NONDEFAULTRULE requirement (line ~293): mark the swallowing and VIA-body behaviour as magic-compat ON and add the OFF parse rule. Also state that USEVIA and USEVIARULE add a rule entry for the named via (both modes), and that END falls through to HARDSPACING handling.
- **Q-LEFDEF-11**: lef-read 'Macro reading' step 2 (line ~312): mark the current text as magic-compat ON. Add OFF: with -annotate, a macro whose (possibly renamed) cell does not exist is skipped with no cell created. Correct the Magic ref to lefRead.c:2114-2158.
- **Q-LEFDEF-12**: lef-read 'Macro coordinate offset' (line ~335) and its scenario: mark the current rule as magic-compat ON and correct the explanation (the geometry/box mismatch is 2*ORIGIN, and the box moves by +F). Add the OFF rule (offset = -F only; box (-ORIGIN-F, SIZE-ORIGIN-F)) with an OFF scenario (ORIGIN 1 1, SIZE 10 BY 10, RECT 0 0 2 2 gives (0,0)-(200,200) and box (-100,-100)-(900,900)). Cross-reference the def-read placement requirement.
- **Q-LEFDEF-13**: lef-read PIN statement table, DIRECTION row (line ~411): mark 'only the first token' as magic-compat ON. Add OFF: OUTPUT followed by TRISTATE maps to tristate. Correct the Magic ref to lefRead.c:1546-1563, 1677-1685.
- **Q-LEFDEF-14**: lef-read PIN statement table, USE row (line ~412): mark the current list as magic-compat ON and add the OFF mapping for SCAN, RESET and TIEOFF.
- **Q-LEFDEF-15**: lef-read geometry requirement (line ~444): mark the ignore rule as magic-compat ON. Add OFF requirements for WIDTH/PATH painting (end extension w/2, half-unit rounding as for RECT) and VIA placement, including port-label creation and annotation checks. Correct the Magic ref to lefRead.c:1219-1224, 1336-1338.
- **Q-LEFDEF-20**: lef-read via geometry statements (line ~199): mark the per-RECT error for IGNORE layers as magic-compat ON and add the OFF silent skip. Correct the Magic refs to lefRead.c:910, 1230, 2656, 2903-2905.
- **Q-LEFDEF-21**: lef-read 'Annotation of existing cells' (line ~513): mark the paint and non-port conversion as magic-compat ON. Add OFF: no painting, POLYGON coverage check, and non-port labels untouched when a port label exists. Correct the Magic refs to lefRead.c:1238-1335, 1704-1782.
- **Q-LEFDEF-22**: lef-read (line ~314): replace 'Reading SHALL disable undo...' with 'A LEF read SHALL be one undoable transaction' and drop the message. Cite layout-undo. Correct the Magic ref to lefRead.c:3018-3019, 3203. Make the matching change in def-read.
- **Q-LEFDEF-23**: None to the behaviour. In lef-read (line ~460), replace 'from the last plane painted' with 'the polygon's decomposition (identical on every plane)'.

### lef-write

- **Q-LEFDEF-31**: lef-write -tech layer table paragraph (line ~530): replace 'With default initialization, the written WIDTH equals the DRC default spacing and no SPACING line appears' with the ON rule (1:1 grid: no WIDTH, SPACING or DIRECTION; d < n < 2d: WIDTH = default spacing, no SPACING). Add the OFF rule (filled width and spacing written, DIRECTION written).
- **Q-LEFDEF-32**: lef-write -tech table, cut/contact row: mark `CUT AREA` as magic-compat ON, and state that the value is 4x the cut area (half-unit storage) and appears only for a positive stored area. Add the OFF `WIDTH` rule.
- **Q-LEFDEF-33**: lef-write SITE requirement (line ~501): mark 'cell being written' as magic-compat ON and add the OFF rule (site cell's own FIXED_BBOX).
- **Q-LEFDEF-34**: lef-write lines ~266 and ~446: mark 'U persists for the session ... not recomputed' as magic-compat ON. Add OFF: U is recomputed for every write. The header-only-for-edit-root rule stays for both modes. Correct the Magic ref to lefCmd.c:442-443.
- **Q-LEFDEF-35**: lef-write names requirement (line ~1061): keep the verbatim rule and add a SHALL for the warning on names containing space, `;`, `#` or newline. Note that `-`, `*` and `$` are legal LEF.
- **Q-LEFDEF-36**: lef-write PIN header (lines ~387 and ~706): mark the empty keywords as magic-compat ON. Add OFF: USE SIGNAL for reset, scan and tieoff, and no DIRECTION line for class codes 0x60/0x70.
- **Q-LEFDEF-37**: lef-write 'Hidden-detail pin geometry with positive setback' step 3 (line ~849): mark it as magic-compat ON and note that -pinonly therefore has no effect under -hide d > 0. Add OFF step 3: if p < d, remove material inside B shrunk by p. Add an OFF scenario.
- **Q-LEFDEF-38**: lef-write hide obstruction step 2 (line ~1023): mark 'whole tile, unclipped' as magic-compat ON and add OFF: tiles clipped to each strip.
- **Q-LEFDEF-39**: lef-write split-tile bullet (line ~913): mark it as magic-compat ON and add OFF: both halves written. Correct the comment ref to lefWrite.c:484-492 and the marking ref to 828-849.
- **Q-LEFDEF-45**: None. lef-write line ~304 already states it. Reference the resolution.
- **Q-LEFDEF-46**: lef-write port-index step 6 (line ~656): add that the system SHALL report a warning naming the dropped port.
- **Q-LEFDEF-47**: lef-write (line ~865): add that the system SHALL report a warning when the setback is dropped.
- **Q-LEFDEF-50**: None to the behaviour (lef-write line ~654). Update the Magic refs to lefWrite.c:1418-1460, 1772-1778, 1789-1808, and note that the idx decrement has no effect on the output.
- **Q-LEFDEF-51**: lef-write 'Hidden-detail pin geometry with zero setback' (line ~832): mark the paint-back of C as magic-compat ON. Add OFF: paint (C minus B shrunk by p) united with the label rectangle, and remember that area for the obstruction cut-out. Add an OFF scenario.
- **Q-LEFDEF-52**: None (lef-write line ~938). Reference the resolution.

### def-read

- **Q-LEFDEF-60**: def-read generated-via requirement (line ~318): mark the persistence as magic-compat ON and add OFF: reset per via definition. Correct the Magic ref to defRead.c:1916, 1958, 1993-2003.
- **Q-LEFDEF-62**: def-read VIAS properties (line ~253): mark the ignore rule as magic-compat ON and add OFF requirements for ORIGIN, OFFSET, PATTERN, POLYGON and MASK. Correct the Magic ref to defRead.c:1928-1937, 2004-2012.
- **Q-LEFDEF-63**: def-read generated-via primary type (line ~315) stays for both modes. Route layer-switch requirement (line ~707): mark the residue rule for a non-contact primary as magic-compat ON, and add OFF: after a generated via, switch to B when on T, else to T. Correct the Magic refs to lefRead.c:2578-2580 and defRead.c:447-459, 483-525.
- **Q-LEFDEF-64**: def-read line ~531: state that with magic-compat ON the values persist from pin to pin (Magic); with it OFF they reset to DEFAULT at each '- <pin>'. Correct the Magic refs to defRead.c:1463-1464, 1552-1577, 1600-1614.
- **Q-LEFDEF-65**: def-read line ~498: rewrite the bullet. Pending flag and current layer are reset at each pin (Magic). Only the rectangle list carries over: with magic-compat ON, rectangles left unpainted are painted with the next pin's placement and name, and those left at the end of PINS are discarded; with it OFF, rectangles are painted per port with that port's placement, and a port without placement is reported and skipped.
- **Q-LEFDEF-66**: def-read line ~475: keep the message for ON; add that with magic-compat OFF, POLYGON and VIA geometry is painted and labelled (polygons decomposed as in lef-read). Other unknown '+' keywords keep the message in both modes.
- **Q-LEFDEF-69**: def-read line ~590: qualify the 'consumed as route tokens' sentence as magic-compat ON; add the OFF rule (skip to the next '+' or ';', where ';' ends the net). Note that '+ RECT/POLYGON/VIA/MASK' special shapes take this path.
- **Q-LEFDEF-70**: def-read line ~734: mark the current text as magic-compat ON; add the OFF behaviour (consume all vertices and paint the polygon on the current paint type; reference point unchanged).
- **Q-LEFDEF-71**: def-read line ~735: mark as magic-compat ON; add that with it OFF the point after VIRTUAL only sets the reference point (no wire, no non-Manhattan message).
- **Q-LEFDEF-72**: def-read line ~715: mark the current text as magic-compat ON; add the OFF rule (MASK tokens ignored silently; via orientation applied to the via geometry).
- **Q-LEFDEF-73**: def-read line ~733: keep the trunc formula for magic-compat ON and add floor for OFF; add that a RECT with no prior reference point is reported and skipped, and that an unparsable RECT is reported and not painted (both modes).
- **Q-LEFDEF-74**: def-read line ~592: mark the 'next routing statement only' rule as magic-compat ON; add the OFF rule (rule applies to the whole net regardless of position; routes painted after the net statement is parsed). The unknown-rule error is unchanged.
- **Q-LEFDEF-77**: def-read line ~219: mark 'further points skipped' as magic-compat ON; add that with it OFF all points are read and FIXED_BBOX is their bounding box.
- **Q-LEFDEF-78**: def-read line ~195: mark the raw-name rule as magic-compat ON; add that with it OFF the fallback name is the basename without '.def'.
- **Q-LEFDEF-79**: def-read line ~166 (and the UNITS parsing text): describe the integer-prefix parse and the fallback to 100 for ON; add that with magic-compat OFF UNITS sets rather than multiplies, warns on repeat, and rejects non-integer or non-positive values with a fallback to 100.
- **Q-LEFDEF-81**: def-read line ~773: qualify 'current edit cell' and the unexpanded area as magic-compat ON; add the OFF rule (design cell; area expanded by 1).
- **Q-LEFDEF-83**: def-read line ~377: replace 'Magic's behavior is undefined' with the rename rule (reuse the cell-file-format instance-id renaming and its messages), citing the resolution.
- **Q-LEFDEF-84**: def-read line ~137: keep the generic unknown-keyword rule; state that with magic-compat ON FILLS/SLOTS/STYLES follow it (including 'END statement out of context.'), and that with it OFF they are skipped as whole sections with one message.
- **Q-LEFDEF-85**: def-read line ~441: no behaviour change; replace the Q-LEFDEF-85 pointer with the resolution (replicate in both modes).
- **Q-LEFDEF-86**: def-read line ~47: replace 'MAY instead run headless' with a SHALL (read works without a window/box; no 'Point to a window first'), citing X-4 and the resolution.
- **Q-LEFDEF-88**: def-read line ~47: state the platform behaviour (document edits blocked during the read; progress via the platform progress service; console message format kept), citing X-4 and the resolution.

### def-write

- **Q-LEFDEF-91**: def-write line ~346: mark the underscore filter and uncounted records as magic-compat ON; add that with it OFF only the vias generated by this write are written. Update the golden-case note accordingly.
- **Q-LEFDEF-93**: def-write line ~429: keep the formula for magic-compat ON; add the OFF formula (x,y = f32(T(R).ll)*s for both non-arrayed uses and array elements); correct the Q-LEFDEF-93 note to say the array path matches def read.
- **Q-LEFDEF-94**: def-write line ~422: state the signed formula for magic-compat ON and the absolute-value formula for OFF.
- **Q-LEFDEF-95**: def-write line ~606 (and the vertical case): keep the integer-halving rule for magic-compat ON; add x1 += f32(q*s/2), x2 -= f32(q*s/2) (and y for N) for OFF.
- **Q-LEFDEF-96**: def-write line ~664: keep 'py != x2' for magic-compat ON and add 'py != y1' for OFF. Correct the Magic ref to defWrite.c:1534.
- **Q-LEFDEF-97**: def-write line ~666 and the TAPERRULE / '( PIN )' text: no behaviour change; replace the Q-LEFDEF-97 pointer with the resolution (replicate in both modes) and fix the cited Magic lines.
- **Q-LEFDEF-98**: def-write line ~710: mark the repeat-first-entry and unscaled WIREEXT as magic-compat ON; add that with it OFF each entry is written and WIREEXT is f32(extend/2)*s. Correct the Magic ref to defWrite.c:3162-3170.
- **Q-LEFDEF-99**: def-write lines ~704 and ~786: keep the current text for magic-compat ON; add the OFF rule (write only referenced rules; discard write-created rules after the write; LEF/DEF-read rules untouched; no reuse of a LEF-read rule on a name collision).
- **Q-LEFDEF-103**: def-write line ~464 (and the DIRECTION and LAYER/PLACED lines): describe the empty keywords and the missing newline for magic-compat ON; add the OFF output (RESET/SCAN/TIEOFF keywords, undefined classes omitted, newline after the LAYER fragment). Correct the claim that these uses are invalid DEF.
- **Q-LEFDEF-105**: def-write line ~309: keep the probe area for magic-compat ON; add the symmetric area (xl - dL - sL, yl - sL, xh + dL + sL, yh + sL), and the same for H, for OFF. Correct the Magic ref to defWrite.c:1910-1931.
- **Q-LEFDEF-107**: def-write line ~358: keep 'bottom >= Y1' for magic-compat ON; add 'bottom < Y1' for OFF.
- **Q-LEFDEF-110**: def-write line ~112: keep 'not re-extracted' for both modes; qualify 'SHALL NOT be checked for staleness' as magic-compat ON, and add the OFF timestamp warning.
- **Q-LEFDEF-111**: def-write line ~293: keep the bounding-box rule for magic-compat ON; add the OFF decomposition (maximal horizontal strips merged vertically when x extents match; each rectangle a via).
- **Q-LEFDEF-112**: def-write line ~576 (seed paragraph): keep repeated emission for magic-compat ON; add that with it OFF tiles already emitted for the net are skipped by later seeds.

### oasis-format

- **Q-OASIS-1**: oasis-format 'integers' rule 2: replace 'See Q-OASIS-1' with a note that KLayout writes an overlong zero in the END padding length (dbOASISWriter.cc END record), so acceptance is required in practice.
- **Q-OASIS-2**: oasis-format 'Validation': add '(gdstk library.cpp oas_validate)' as the source and keep the not-confirmed note until OASIS-012 passes or the P39 text is checked.
- **Q-OASIS-3**: oasis-format RECTANGLE: cite both KLayout files (reader do_read_rectangle and writer square path) as the source. Note that KLayout does not read the H field when S=1 and the bit is set, so the reported-and-read behaviour is Hefesto's own.
- **Q-OASIS-4**: oasis-format CTRAPEZOID: replace 'transcribed from an independent implementation and must be verified' with 'matches KLayout (dbOASISReader.cc ctraps_table) and gdstk (library.cpp CTRAPEZOID) for all 26 types; OASIS-030 is the release check'.
- **Q-OASIS-5**: oasis-format modal-variable rule 2: cite KLayout dbOASISReader.cc do_read (reset after each name record) and keep the note that P39 is not confirmed.
- **Q-OASIS-6**: oasis-format PROPERTY attachment: replace 'A PROPERTY after XYABSOLUTE or XYRELATIVE attaches to the element before that record' with 'XYABSOLUTE and XYRELATIVE end element attachment; a following PROPERTY attaches to the current CELL (as KLayout)'. Add a scenario. oasis-read needs no change, since rule 3 already discards cell properties.
- **Q-OASIS-7**: oasis-format strict mode: cite the KLayout warnings as the source. Add a scenario: a strict-mode file written by oasis-write, read back, produces no strict-mode warning.
- **Q-OASIS-8**: oasis-format table offsets rule 4: cite KLayout dbOASISWriter.cc begin_table/begin_cblock.
- **Q-OASIS-9**: oasis-format CBLOCK rule 5: state that a span is accepted at any byte, including inside a string, and note that KLayout accepts it only outside strings.
- **Q-OASIS-10**: oasis-read 'Placement repetitions': add that displacements read as individual uses (rule 3) go through the gds-read duplicate-use rule, so coincident identical placements are discarded with the 'placed on top of itself' warning, except for flattened (CDFLATGDS) cells, whose placements are painted. Add a type 10 scenario with a zero g-delta. In oasis-format repetition rule 2, change 'which are kept' to 'which the decoder keeps; oasis-read decides what they become'.
- **Q-OASIS-11**: oasis-format PATH modal updates: cite dbOASISReader.cc do_read_path and keep the not-confirmed note.

### oasis-read

- **Q-OASIS-12**: oasis-read 'Placement transforms': correct the Magic ref to calma/CalmaRdio.c:81-193. Add a scenario: record 18 with angle 30 gives a placement at 0 plus the 'Non-Manhattan angle (30)' error, and a golden documents the KLayout difference.
- **Q-OASIS-30**: oasis-read 'Cell timestamps': cite calma/CalmaRdio.c:326-332 and CalmaRdcl.c:500-508 as the analogue. Change 'See Q-OASIS-30 for alternatives' to 'a private timestamp property may be added with P-8'.
- **Q-OASIS-31**: oasis-read 'CIRCLE elements': replace 'cos/sin are binary64' with a requirement that the vertex values be computed identically on all platforms (correctly-rounded or bundled trig), and note that the result is inscribed only up to rounding. Add a scenario fixing the exact vertices for r = 1000, N = 64.
- **Q-OASIS-32**: oasis-write TEXT translation (line ~249): replace '(see Q-OASIS-32)' with the resolution and add that the system SHALL warn once per write with the number of font labels written without presentation. oasis-read (line ~289): replace the pointer with the resolution.
- **Q-OASIS-33**: None to the behaviour: oasis-read line ~264 already follows Q-MASK-114. Replace 'see Q-OASIS-33' with the resolution.
- **Q-OASIS-34**: oasis-read line ~55: replace '(see Q-OASIS-34)' with the resolution and note that `#` is a legal cell-name character (cmd-hierarchy).
- **Q-OASIS-35**: oasis-read line ~444: replace 'How GDS output treats them is open (Q-OASIS-35)' with the chosen rule. gds-write: add 'Read-only OASIS vendor cells' (transcode, or refuse with an error under option b), with the element mapping and the error for inexpressible elements (layer or datatype above 32767, polygon above 8191 points), and a golden case at 32767/32768. Correct the Magic ref to CalmaWrite.c:972-1361.
- **Q-OASIS-37**: oasis-read line ~369: replace 'See Q-OASIS-37' with the resolution (replicate gds-read's property handling).
- **Q-OASIS-38**: oasis-read line ~169: replace '(see Q-OASIS-38)' with the resolution.
- **Q-OASIS-39**: oasis-read line ~168: replace '(see Q-OASIS-39)' with the resolution, note that GDS numbers above 255 behave the same, and state that unmapped pairs are reported once per pair as gds-read does (calmaLayerError), with no report in read-only cells.

### oasis-write

- **Q-OASIS-40**: oasis-write line ~507: replace '(see Q-OASIS-40)' with the resolution and add the warning.
- **Q-OASIS-60**: oasis-write 'String mapping' rule 1 (line ~177): replace 'see Q-OASIS-60' with the resolution, and state that the permissive table is the one selected by magic-compat (Q-MASK-200), that bytes above 127 become `X` with the 'Unprintable character' warning, that output is upper-cased when `oasis lower` is off, and that layout data is not mutated (Q-MASK-208).
- **Q-OASIS-61**: None to the behaviour: oasis-write line ~156 already states it. Replace '(Q-OASIS-61)' with the resolution.
- **Q-OASIS-62**: oasis-write line ~70: replace '(see Q-OASIS-62)' with the resolution. Correct the Magic ref to CalmaWrite.c:2190-2260, 2394-2709.
- **Q-OASIS-63**: oasis-write PATH row (line ~478): replace the odd-width rule with 'an odd width makes the write fail with an error naming the cell (OASIS cannot express it)', replace the type-1 rule with 'flush path plus a CIRCLE of radius w/2 at each end', keep the negative-width rule, and cite KLayout's writer for both. Add an odd-width error scenario.
- **Q-OASIS-64**: oasis-write line ~376: replace '(see Q-OASIS-64)' with the resolution, name the build-pinned zlib and its settings, and add a scenario: the same write on macOS and Linux gives identical bytes.
- **Q-OASIS-65**: oasis-write line ~444: replace 'See Q-OASIS-65 for the default' with the resolution.
- **Q-OASIS-66**: oasis-write line ~417: replace 'see Q-OASIS-66' with the resolution and cite KLayout's writer.
- **Q-OASIS-67**: oasis-write line ~355: replace '(see Q-OASIS-67)' with the resolution.
- **Q-OASIS-68**: None to the behaviour: oasis-write line ~460 already follows Q-MASK-211. Correct the Magic ref to CalmaWrite.c:754-760.
- **Q-OASIS-69**: oasis-write line ~310: replace 'See Q-OASIS-69 for the value type' with the resolution, and note KLayout's narrowest-type rule as the fallback.

### oasis-command

- **Q-OASIS-90**: oasis-command 'Settings independent of the gds command' (line ~138): split into options shared with `gds` (`drccheck`, `flatten`, `flatglob`, `maskhints`, `noduplicates`, `ordering`, `polygon subcells`, `path subcells`, `readonly`, `rescale`, `savepaths`, `unique` and `warning`) and independent options (`abstract`, `addendum`, `arrays`, `compress`, `contacts`, `labels`, `library`, `lower`, `merge` and `undefined` (write-side), `datestamp` and `nodatestamp` (used on both sides by `gds`; a `gds datestamp` set for reproducible GDS output must not change OASIS cell timestamps), and `magscale` (no effect on OASIS), plus the OASIS-only options). Update the 'Applies to' column of 'Options shared with the gds command' to say which are shared. Add a scenario: `gds readonly true` then `oasis read` marks the read cells read-only.
- **Q-OASIS-91**: oasis-command line ~97: replace '(see Q-OASIS-91)' with the resolution.

Behaviour unchanged, only the reference or Magic lines change: Q-LEFDEF-23, Q-LEFDEF-45, Q-LEFDEF-50, Q-LEFDEF-52, Q-OASIS-33, Q-OASIS-61, Q-OASIS-68.

Outside the specs: golden deviation entries in `tests/golden/deviations.toml` for the both-modes fixes that change Magic-comparable output (Q-LEFDEF-1 DEF-read width, Q-LEFDEF-73, Q-LEFDEF-83: Hefesto's `.mag` and `def write` carry the renamed instance id where Magic writes the duplicate), and the undo change of Q-LEFDEF-22 in layout-undo's list of undoable operations.
