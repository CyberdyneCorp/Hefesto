# M5 decision table: the 105 open questions blocking mask generation, GDS and CIF

M5 ("Mask generation, GDS and CIF", docs/roadmap.md) cannot start until every open question its specs cite has a `Resolution:` line. `python3 scripts/check_milestone_readiness.py --milestone M5` lists 105: 104 in `docs/decisions/open-questions/MASK.md` and Q-DRC-13 in `DRC.md`. Each entry was checked against Magic 8.3.694 (fc5daa9e, `/Users/leonardo/trabalho/magic`, read-only) and, where a PDK is named, against the sky130A, gf180mcuD and ihp-sg13g2 tech files. Each entry now carries `Status: open (recommendation drafted 2026-11-13)` and a rewritten `Recommendation:` line with the full reasoning; corrected evidence is marked `(corrected 2026-11-13: ...)`. **Nothing here is approved yet**, and no entry has a `Resolution:` line.

The standing rule for Magic bugs and quirks is compat: `magic-compat` ON copies Magic, OFF does the intended thing. Most rows follow it. Where they do not, the row says why: replicate when Magic defines no corrected behaviour or the quirk is an internal unit or order choice that byte identity needs, fix when the quirk loses data, is undefined behaviour, or has no Magic output to match.

Recommendation kinds (X-2, docs/decisions/cross-cutting.md):

- **replicate**: do what Magic does, in both modes.
- **compat**: replicate with `magic-compat` ON and use the corrected behavior with it OFF (tier 2).
- **fix**: corrected behavior in both modes (tier 1, or tier 3 when there is no Magic output to match).
- **platform**: a Hefesto platform decision (X-3/X-4).
- **other**: none of the above.

## Judgement items

None remain. Q-MASK-204 and Q-MASK-318 were judgement items in the first draft; review against Magic's source showed that the standing rules decide both (Q-MASK-204 by the compat rule and the byte-identical exit criterion, Q-MASK-318 by X-2 tier 1 because it loses data). What is left is cost and scope, in the next section.

## Effort and scope notes

- **Q-MASK-204 (compat): bplane emulation.** With magic-compat ON, Hefesto's cell-use index must reproduce Magic's enumeration order exactly: LIFO inBox, the re-placement of uses by DBReComputeBbox (database/DBcellbox.c:728-764), the bin rebuild at 10 or more uses on a non-surrounding search (bplane/bpBins.c:651), bin geometry and the bin walk. Each writer must enumerate where Magic does: GDS SREFs before CIFGenSubcells (calma/CalmaWrite.c:1486, :1510-1514), so the first `gds write` of a fresh session is unbinned; CIF numbering before it (cif/CIFwrite.c:333) and CIF `C` calls after it (:467), so CIF calls are binned for any cell with 10 or more uses that has an interaction (cif/CIFhier.c:885-902) or is larger than one step square (drc/DRCsubcell.c:451). Replicating only the unbinned order would fail those CIF goldens and any GDS golden of a second write; a canonical order in both modes would give up the byte-identical criterion. `.mag` writes uses alphabetically (database/DBio.c:4775-4791), so the OFF order is stable. The golden harness must pin Magic's session state: one write per fresh session, no DRC or extraction before it, unless a case tests history on purpose.
- **Q-MASK-318 (fix): CIF read symbol table.** Within one read, symbol n (in `C n` and in `DS n`) always maps to a cell created by that read, collisions become `n_k`, and no in-memory cell is cleared. The forward-call path (`C n` before `DS n`, cif/CIFrdcl.c:1355) needs the same rule as `DS` (:484-486). `DD n` is compat (ON: one symbol; OFF: every symbol >= n, as CIF 2.0).

## Decision table

Each row's full text is the entry's `Recommendation:` line. Where the draft changes the entry's original recommendation, that line ends with `(corrected 2026-11-13, was ...)`.

### Output styles and operators

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MASK-1 | grow-grid rounds by \|c\| mod g on both sides | **compat** | ON: move edges by \|c\| mod g; OFF: round outward to the g-grid (floor/ceil) |
| Q-MASK-2 | close threshold is an area but normalised as a length | **compat** | ON: linear threshold scaling; OFF: quadratic, exact 64-bit comparison; units unchanged |
| Q-MASK-3 | bridge/bridge-lim test the wrong bottom neighbour for the SE corner | **compat** | ON: bottom-left neighbour for the pass-1 SE corner; OFF: bottom neighbour at the right end, as pass 2 |
| Q-MASK-5 | bridge-lim searches database planes with mask-unit coordinates | **compat** | ON: mask-unit search and unscaled erase on database planes; OFF: convert units; warn when scaleFactor ≠ 1 |
| Q-MASK-6 | tagged uses internal-unit label coordinates against mask planes | **compat** | ON: Magic's unit mix on output and input (IHP reads depend on it); OFF: convert the label rect to the plane's units |
| Q-MASK-7 | not-square drops all non-Manhattan geometry | **compat** | ON: drop split tiles, bottom type for all neighbours; OFF: copy triangles, side-correct neighbour types |
| Q-MASK-8 | gridlimit is never rescaled and has two interpretations | **replicate** | store and apply gridlimit as written; warn when the expander ≠ 10 |
| Q-MASK-10 | style name prefix shadowing | **fix** | exact-name (and base+variant) style matching, with a warning; only styles Magic drops change |
| Q-MASK-11 | `labels ... text` has no effect | **replicate** | `labels ... text` in cifoutput is the same as `labels ...` |
| Q-MASK-13 | bloat-all prevents unit reduction | **replicate** | bloat-all counts as 1 in both GCDs (keeps expander 10 and GDS UNITS for every PDK) |
| Q-MASK-15 | grow-min geometric formula is incomplete | **replicate** | port the grow-min algorithm exactly (int product, double sqrt, truncation); lint non-rect_only use |
| Q-MASK-18 | interaction radius not recomputed when no reduction occurs | **compat** | ON: keep the stale radius after a non-reducing rescale; OFF: always recompute the halo |
| Q-MASK-20 | bloat-or on split tiles ignores corner extensions | **replicate** | two leg rectangles plus the shifted triangle (or Euclidean grow), as Magic |
| Q-MASK-21 | interacting tests against database types use truncating division | **compat** | ON: truncating division for database-type tests; OFF: outward rounding |
| Q-MASK-22 | net with no layer list adds nothing | **replicate** | `net` without layers adds nothing; lint warnings for empty and CIF/temp layer lists |
| Q-MASK-23 | bbox operator uses the database cell bounding box | **replicate** | bbox of the cell being generated (paint, subcells, label anchors; no text extents) |
| Q-MASK-26 | hierarchical truncation returns partial layer geometry | **replicate** | replicate the hierarchical truncation; the residual question is Q-MASK-402 |
| Q-MASK-27 | squares grid-limit fix diverges for negative coordinates | **compat** | ON: sign-blind grid-limit shrink; OFF: sign-aware correction, as slots |

### DRC mask-layer rules

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-DRC-13 | Mask error coordinate mapping truncates toward zero | **compat** | ON: truncation, widening, and dropping collapsed area/maxwidth errors; OFF: outward rounding; diagnostic counts drops in both |

### GDS read

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MASK-110 | Structure with trailing garbage leaves input planes uncleared | **compat** | ON: uncleared planes leak into the next structure; OFF: clear them and drop the malformed structure |
| Q-MASK-111 | "Rewound" flag is session-global and never reset | **compat** | ON: session-global rewound flag; OFF: reset per read (diagnostics only) |
| Q-MASK-113 | Round path ends (PATHTYPE 1) read as flush | **compat** | ON: PATHTYPE 1 read as flush; OFF: as type 2 with a warning |
| Q-MASK-114 | Path end extensions applied per axis | **compat** | ON: per-axis end extension, negatives ignored; OFF: along the segment, negatives shorten |
| Q-MASK-115 | Negative (absolute) path widths dropped | **compat** | ON: W ≤ 0 dropped; OFF: W < 0 used as an absolute (unmagnified) width with a warning |
| Q-MASK-116 | Duplicate-point removal skips the head pair | **compat** | ON: head duplicate point kept; OFF: removed |
| Q-MASK-118 | Path outline uses H = 1.5708 and savetheta = 0 for the first segment | **compat** | ON: H = 1.5708 and savetheta = 0 for the first joint; OFF: π/2 and the first segment's angle |
| Q-MASK-119 | savepaths stores input units in a dimension property | **compat** | ON: savepaths values in CIF input units; OFF: internal units |
| Q-MASK-120 | TEXT WIDTH honored only right after PRESENTATION | **compat** | ON: TEXT WIDTH only right after PRESENTATION; OFF: anywhere before STRANS |
| Q-MASK-121 | Label positions truncated toward zero | **compat** | ON: label positions truncated toward zero; OFF: floor division |
| Q-MASK-122 | Inconsistent font-size units between DRC default, 1 µm cap and MAG | **replicate** | port the three font-size formulas exactly (DRC default, MAG with +0.5, WIDTH) |
| Q-MASK-123 | Pin placeholder converts text-kind labels to ports | **compat** | ON: placeholder turns text labels into ports; OFF: port data only from port-kind layers |
| Q-MASK-124 | AREF row spacing rounding discarded | **compat** | ON: truncated AREF ysep; OFF: rounded, as for columns |
| Q-MASK-125 | Unknown instance property stops property parsing | **compat** | ON: unknown PROPATTR stops property parsing; OFF: skip the pair and continue |
| Q-MASK-126 | Look-ahead seek assumes minimal STRNAME padding | **fix** | seek back using the real STRNAME record length |
| Q-MASK-128 | Home-directory substitution in GDS_FILE uses the wrong string | **fix** | apply the `~` substitution to the GDS path itself (Magic stores a .mag path: data loss) |
| Q-MASK-129 | GDS_FILE stores the path as found on the search path | **compat** | ON: GDS_FILE as found on the search path; OFF: absolute canonical path |
| Q-MASK-130 | `gds rescale no` also forces warning level "limit" | **compat** | ON: `gds rescale no` also sets warning level `limit`; OFF: leaves it |
| Q-MASK-132 | Host dependencies (time zone, PDK variables, gzip) | **platform** | host local zone with a pinned-zone override for tests; PDK variables from Tcl globals (settings on iPadOS); bundled zlib |
| Q-MASK-134 | BGNSTR year interpreted as years since 1900 | **compat** | ON: year as tm_year with 32-bit truncation; OFF: full years accepted, no overflow |

### GDS write

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MASK-200 | Permissive GDS character table swaps '%' and '&' | **compat** | ON: permissive table swaps `%`/`&` (IHP output byte-identical); OFF: identity mapping |
| Q-MASK-201 | UNITS values stick after an angstrom write | **fix** | UNITS derived from the current style on every write (Magic's sticky value writes 10× too small) |
| Q-MASK-202 | Modification date is current time, documentation says zero | **compat** | ON: modification date is the current time; OFF: t = 0, as gds.html documents |
| Q-MASK-203 | Dates depend on the host time zone | **replicate** | host local time zone; goldens and CI run with TZ=UTC |
| Q-MASK-204 | Order of SREFs and structures depends on the cell-use index's binning state | **compat** | ON: exact bplane enumeration order, sampled where Magic samples it; OFF: canonical reverse-insertion order |
| Q-MASK-205 | Order of port labels with equal port index | **replicate** | stable sort of the reversed label list (glibc order on the Linux reference image), independent of the host libc |
| Q-MASK-206 | Port labels dropped when the type has no `labels ... port` declaration | **compat** | ON: port label without a port layer omitted; OFF: written on the port-text layer; warn in both |
| Q-MASK-207 | string-limit truncation of odd-length strings keeps 31 characters | **compat** | ON: odd strings keep 31 characters under string-limit; OFF: the last 32 |
| Q-MASK-209 | Sanitized structure names are not checked for uniqueness | **fix** | uniquify colliding sanitized STRNAMEs as `XXXXX<calmanum>` (itself checked, `_k` if taken) with a warning |
| Q-MASK-210 | Default extension test looks for '.' anywhere in the path | **compat** | ON: no extension if the path has any `.`; OFF: test the basename only |
| Q-MASK-211 | Vendor full-dump prefix uses random() | **compat** | ON: own glibc TYPE_3 random() shared with `random`/`random seed`; OFF: prefix from a file-name hash |
| Q-MASK-212 | Full-dump unit check depends on the `gds readonly` setting at write time | **fix** | always check the vendor library's units; abort the dump once per library |
| Q-MASK-214 | Forward-reference renaming in full dumps | **compat** | ON: Magic's dangling forward-reference names, with a warning; OFF: appended structures take the recorded name |
| Q-MASK-215 | All structures of a full dump get the triggering cell's creation date | **compat** | ON: `datestamp` or the triggering cell's date, as Magic; OFF: the source file's creation date unless `datestamp` is set |
| Q-MASK-216 | Copied SNAME records are not updated for renamed cells | **compat** | ON: copy SNAMEs verbatim and warn; OFF: rewrite SNAMEs to current names |
| Q-MASK-217 | Merged polygons may contain slits and may be split arbitrarily | **replicate** | port Magic's merge and collinear passes exactly |
| Q-MASK-219 | Write with no output style leaves an empty file | **fix** | check the output style before the file is opened (no truncation of an existing file) |
| Q-MASK-221 | Text magnification scale is read-only | **compat** | ON: MAG ignores magscale; OFF: MAG divided by magscale |
| Q-MASK-222 | 32-bit overflow in coordinate scaling | **fix** | 64-bit coordinate arithmetic; a value outside I4 fails the write |
| Q-MASK-223 | gzip byte stream depends on the zlib build | **platform** | any conforming gzip writer; compare decompressed bytes |
| Q-MASK-225 | Incompatible expander writes mis-scaled output | **compat** | ON: write mis-scaled with Magic's errors; OFF: refuse before opening the file |
| Q-MASK-226 | Abstract-view failure leaves a partial but terminated file | **compat** | ON: partial, terminated library; OFF: validate abstract views before opening the file |
| Q-MASK-228 | bridge-lim passes 1 and 2 modify the plane they are enumerating | **compat** | ON: passes 1 and 2 enumerate the plane they paint; OFF: enumerate a snapshot |
| Q-MASK-229 | bridge-lim erases whole limiting tiles, not only their part inside the bridge | **compat** | ON: erase whole limiting tiles, unscaled; OFF: erase only the clipped part, in mask units |
| Q-MASK-230 | bloat-all leaves contact-image trigger tiles marked | **compat** | ON: stale trigger marks within the generation pass; OFF: reset every searched plane |
| Q-MASK-231 | bloat-all seeding depends on trigger order when d = 0 | **compat** | ON: seed only the first unprocessed tile; OFF: seed every overlapping region |
| Q-MASK-232 | bloat-all converts mask-layer triggers to internal units with truncation toward zero | **compat** | ON: truncating trigger conversion; OFF: outward rounding |

### Mask input styles and CIF I/O

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MASK-300 | cifinput "nanometers"/"angstroms" overwrites the decimal multiplier | **compat** | ON: unit keyword sets M; OFF: multiplies M, as on output; warn for decimal + unit |
| Q-MASK-301 | labels kind "sticky" identical to "text" | **replicate** | `sticky` is the same as `text` |
| Q-MASK-302 | style statement prefix-matches existing style names | **compat** | ON: prefix match merges styles; OFF: exact match; warn in both |
| Q-MASK-303 | istyle selection ambiguous even with an exact match | **fix** | an exact istyle name wins over prefix matches |
| Q-MASK-306 | input grow/shrink algorithm depends on the output style | **compat** | ON: input grow follows the output style's grow-euclidean; OFF: always square |
| Q-MASK-307 | boundary/mask-hints operators on regular input layers use output semantics | **compat** | ON: output-semantics OR in layer rules; OFF: no-op there; warn in both |
| Q-MASK-309 | copyup ignored by the CIF reader | **compat** | ON: CIF calls ignore copyup; OFF: apply the child's copyup planes on `C` (backward calls) |
| Q-MASK-310 | tagged on input mixes internal and read units | **compat** | ON: unscaled label search on read planes (IHP depends on it); OFF: scale by S |
| Q-MASK-311 | labels statement under a templayer stores a CIF index as a Magic type | **compat** | ON: CIF index used as a Magic type (sky130 FILLOBSPOLY); OFF: ignore with a warning; invalid index guarded in both |
| Q-MASK-314 | 95 rescale does not update the symbol scale | **fix** | after a 95 rescale, double the symbol numerator (Magic corrupts later geometry) |
| Q-MASK-315 | odd box lengths are truncated asymmetrically | **compat** | ON: truncate odd box lengths; OFF: subdivide read units so the box is exact |
| Q-MASK-316 | round flash read as a square | **replicate** | round flash painted as its bounding square |
| Q-MASK-317 | "9 name" outside a definition renames the edit cell | **compat** | ON: top-level `9` renames the edit cell; OFF: ignored with a warning |
| Q-MASK-318 | DD/DS reuse and redefinition of renamed symbols | **fix** | symbol n always maps to a cell created by this read (`n_k` on collision); no existing cell is cleared; DD compat (OFF: all symbols >= n) |
| Q-MASK-320 | .cif always appended to file names | **compat** | ON: always append `.cif`; OFF: only when missing; pickers pass the root name in both |
| Q-MASK-321 | flat CIF named __CIF2__ and without labels | **compat** | ON: `__CIF2__` without labels; OFF: root name and flattened labels |
| Q-MASK-322 | label text not escaped in CIF output | **compat** | ON: verbatim label text with a warning; OFF: whitespace and `;` → `_` |
| Q-MASK-323 | CIF symbol numbering depends on instance enumeration order | **compat (follows Q-MASK-204)** | numbering and `C` call order use the current mode's use enumeration order; ON: numbering unbinned (CIFwrite.c:333), calls binned (:467), as Magic |
| Q-MASK-324 | CIF header contains host/time fields | **platform** | same preamble lines; @@tool is Hefesto; six host/build lines masked in goldens |
| Q-MASK-325 | cif coverage counts triangles as rectangles | **compat** | ON: split tiles count as full rectangles; OFF: half area, exact |
| Q-MASK-326 | calma number list of exactly 255 entries rejected | **compat** | ON: 255+ list entries rejected; OFF: no entry limit |
| Q-MASK-327 | default 94 label sticky flag uses a mismatched layer search | **compat** | ON: Magic's mismatched sticky search; OFF: the current `L` layer's kind |
| Q-MASK-330 | not-square drops non-manhattan geometry | **compat** | same as Q-MASK-7 for input styles |
| Q-MASK-331 | warning redirect not honored for CIF input | **compat** | ON: CIF read ignores the redirect file; OFF: CIF messages go to it |
| Q-MASK-333 | CIF wires use square end extensions | **replicate** | square half-width wire ends |
| Q-MASK-334 | grow-grid does not snap upper edges to the grid (duplicate of Q-MASK-1) | **compat** | same as Q-MASK-1 for input styles |
| Q-MASK-335 | malformed CIF call transformation is used unscaled | **compat** | ON: place the call with the partial unscaled transform; OFF: skip the call |

### Hierarchical generation

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MASK-400 | Phantom interaction area around the origin | **compat** | ON: the (0,0) seed adds a phantom area; OFF: skip it; shared with hierarchical DRC |
| Q-MASK-401 | Vendor-cell exclusion during flattening never applies | **replicate** | vendor paint always copied; `see-no-vendor` parsed and stored, no effect |
| Q-MASK-402 | Hierarchy-stopping operators leave a partial layer result | **compat** | ON: the partial plane joins the residual; OFF: a stopped layer has no residual; warn in both |
| Q-MASK-403 | Layers that depend on bbox/boundary layers stay hierarchical | **compat** | ON: dependents of bbox/boundary layers stay hierarchical; OFF: removed transitively |
| Q-MASK-404 | Halo radius ignores several operators | **compat** | ON: Magic's radius; OFF: also count bloat-max/min and bridge widths |
| Q-MASK-405 | Non-distributive operators do not force hierarchical processing | **compat** | ON: Magic's force set; OFF: grow-min, not-square, orthogonal, maxrect also force |
| Q-MASK-406 | Array element filter with reversed index ranges | **compat** | ON: signed index filter; OFF: absolute distance from the corner |
| Q-MASK-409 | Grow-slivers details | **compat** | ON: Magic's sliver geometry; OFF: rescaled min-width, full growth, both axes; no stdout prints |
| Q-MASK-410 | CIFhier property ignored by CIF writer | **compat** | ON: CIF writer ignores CIFhier; OFF: honours it like GDS |
| Q-MASK-411 | Flat CIF output loses labels and mask hints, names cell __CIF2__ | **compat** | ON: flat CIF loses labels and hints; OFF: copies them, root name |
| Q-MASK-413 | `cif unfracture` needs a dummy argument (= D-20) | **compat** | ON: third word only queries, with a warning; OFF: third word sets the switch |
| Q-MASK-414 | Progress reporting and event blocking during interaction processing | **platform** | progress callback, cancellation without partial output, snapshot isolation |

## Counts

| Kind | Count | IDs |
|---|---|---|
| compat | 74 | Q-DRC-13, 1, 2, 3, 5, 6, 7, 18, 21, 27, 110, 111, 113, 114, 115, 116, 118, 119, 120, 121, 123, 124, 125, 129, 130, 134, 200, 202, 204, 206, 207, 210, 211, 214, 215, 216, 221, 225, 226, 228, 229, 230, 231, 232, 300, 302, 306, 307, 309, 310, 311, 315, 317, 320, 321, 322, 323, 325, 326, 327, 330, 331, 334, 335, 400, 402, 403, 404, 405, 406, 409, 410, 411, 413 |
| replicate | 16 | 8, 11, 13, 15, 20, 22, 23, 26, 122, 203, 205, 217, 301, 316, 333, 401 |
| fix | 11 | 10, 126, 128, 201, 209, 212, 219, 222, 303, 314, 318 |
| platform | 4 | 132, 223, 324, 414 |

Bare numbers are Q-MASK IDs. Q-MASK-323 is counted as compat (it follows Q-MASK-204). Q-MASK-318 is counted as fix; its DD sub-rule is compat. Total 105.

## Audit decision D-20

**This draft reverses D-20's recommendation.** D-20 recommends "always accept both forms" (`Resolution: fix`); Q-MASK-413 recommends compat instead. The standing rule decides it: X-2 tier 2 puts parse-argument quirks that change results behind `magic-compat`, and accepting the third word changes GDS output, so the "superset, no output change" argument behind D-20 does not apply.

D-20 (`docs/decisions/audit/DECISIONS.md`) is the same question as Q-MASK-413: should `cif unfracture yes` set the switch, when Magic reads the value only from the fourth word? D-20's draft was "always accept both forms (fix), output-neutral". That premise does not hold: in Magic `cif unfracture yes` leaves the switch unchanged, so setting it would change GDS polygon partitioning. The draft here is D-20's second option, **the third-word form only with `magic-compat` OFF** (compat), with a warning under ON when the ignored word is yes/no. Approving Q-MASK-413 resolves D-20; DECISIONS.md is updated with the other approved changes.

## Notable changes to the entries' original recommendations

- **Q-MASK-302**: fix → compat.
- **Q-MASK-325**: fix → compat.
- **Q-MASK-326**: fix → compat.
- **Q-MASK-327**: fix → compat.
- **Q-MASK-331**: fix → compat.
- **Q-MASK-335**: fix → compat.
- **Q-MASK-330**: replicate → compat.
- **Q-MASK-311**: fix (reject) → compat.
- **Q-MASK-318**: replicate → fix (X-2 tier 1, data loss); DD compat.
- **Q-MASK-323**: "capture Magic's order and decide" → compat (follows Q-MASK-204).
- **Q-MASK-215**: replicate → compat.
- **Q-MASK-204**: replicate → compat (no longer a judgement item).
- **Q-MASK-413**: D-20 draft: fix → compat.
- Most other changed entries said "replicate" or "fix"; under the standing rule they become compat where the quirk changes output and Magic's intent is clear (each entry is marked `(corrected 2026-11-13, was ...)`).

## Evidence corrections

36 entries had wrong line numbers or an incomplete description; each entry's `Evidence:` line now carries the corrected lines. The ones that change the substance:

- **Q-DRC-13**: area and maxwidth errors that collapse to zero width are not reported at all, not just placed differently.
- **Q-MASK-6**: `tagged` also runs on cifinput, with the same unit mismatch; IHP's cifin uses it five times, so the IHP read depends on it (same as Q-MASK-310).
- **Q-MASK-7**: lines moved to CIFgen.c:4727-4800; the code comment says triangles are never square, so dropping them is not the intent.
- **Q-MASK-10**: the prefix test exists for `style base variants`; a plain exact compare would break variant re-reads.
- **Q-MASK-13**: every line moved; bloat-all is what keeps the PDK expander at 10 and so the GDS UNITS.
- **Q-MASK-23**: the box is that of the cell being written (CIFErrorDef), including label anchors and excluding text extents.
- **Q-MASK-110**: the stale planes are painted into the next structure; the error path also skips label cleanup and unique ids.
- **Q-MASK-200**: the swapped entries are at CalmaWrite.c:234; the table also maps STRNAMEs, and IHP sets `calma-permissive-labels`.
- **Q-MASK-211**: random() is not unseeded: `random seed` calls srandom, so batch sessions are deterministic.
- **Q-MASK-212**: a failed check is not recorded, so every later abstract cell from the same file reports it again.
- **Q-MASK-219**: an existing file of the same name is truncated, which makes this data loss.
- **Q-MASK-222**: the cited lines were a debug print; the multiplications are at nine places, including the label-midpoint sum.
- **Q-MASK-226**: every cell not yet written is skipped, not only the ancestors.
- **Q-MASK-307**: the cited CIFrdcl.c range was the wrong function; the spec's ref (spec.md:362) is wrong too.
- **Q-MASK-309**: all three PDK cifinput styles use copyup, so CIF reads with them lose the cross-hierarchy checks.
- **Q-MASK-310**: a live IHP case (`tagged isolbox`, `tagged well/sub!` with S > 1).
- **Q-MASK-315**: boxes that straddle 0 also lose width (B 3 2 0 0 gives -1..1).
- **Q-MASK-320**: Magic's own GUI writes `chip.cif.cif`; its docs say read accepts `file` or `file.cif`.
- **Q-MASK-324**: @@source and @@compiled are host or build dependent as well.
- Line numbers only: Q-MASK-1, Q-MASK-2, Q-MASK-18, Q-MASK-121, Q-MASK-202, Q-MASK-207, Q-MASK-215, Q-MASK-221, Q-MASK-223, Q-MASK-232, Q-MASK-306, Q-MASK-314, Q-MASK-316, Q-MASK-322, Q-MASK-323, Q-MASK-333, Q-MASK-335.

Entries whose evidence was right but incomplete carry a `Verified 2026-11-13:` line instead. Notable additions: Q-MASK-311 and Q-MASK-310 are live sky130 and IHP cases; Q-MASK-204 gives the exact bin rebuild trigger (10 uses, non-surrounding search) and where each writer samples the order; Q-MASK-318 adds that a pre-existing user cell named `n` is also cleared, and that a forward `C n` misbinds; Q-MASK-209 corrects the string-limit collision claim (names over 32 become `XXXXX<calmanum>`, they are not truncated); Q-MASK-205 records the Linux reference image.

## Spec changes after approval

Not edited yet. After approval each spec gets the text below, and the entry's `Resolution:` line.

### drc-cif-rules

- **Q-DRC-13**: drc-cif-rules (spec.md:74): state that collapsed area and maxwidth errors are dropped with magic-compat ON. Add the OFF outward-rounding mapping and the diagnostic for dropped errors. Cite DRCcif.c:1364-1367, 1468-1471, 1497-1500 and 1554-1557.

### mask-output-operators

- **Q-MASK-1**: mask-output-operators grow-grid requirement (spec.md:169): add the magic-compat OFF formula (floor for lower edges, ceil for upper edges) and keep the current formula as the ON behaviour. Fix the evidence line to CIFgen.c:526-533.
- **Q-MASK-2**: mask-output-operators close (spec.md:498): state the ON (linear) and OFF (quadratic, exact) normalisation. mask-output-styles unit-normalisation pseudocode: note that close values take part in the GCD as lengths in both modes. Fix the evidence lines to CIFtech.c:2500-2502, 2648-2649 and 1912-1915.
- **Q-MASK-3**: mask-output-operators bridge pseudocode (spec.md:524) and bridge-lim (spec.md:637): mark `LB` as the ON behaviour and specify the OFF behaviour (bottom neighbour under the tile's right end, as BRight in pass 2).
- **Q-MASK-5**: mask-output-operators bridge-lim (spec.md:612): add the OFF unit conversion for limitFree, limitOverlap and eraseLimits, and the loader warning.
- **Q-MASK-6**: mask-output-operators tagged (spec.md:796): add the OFF conversion. mask-input-styles tagged: state the same Magic mismatch for input (internal-unit labels on read-unit planes) and the OFF conversion.
- **Q-MASK-7**: mask-output-operators not-square (spec.md:743): describe ON as now, and add OFF (copy triangles; side-correct neighbour types).
- **Q-MASK-15**: mask-output-operators grow-min (spec.md:223 area): spell out the exact h expression, its evaluation order and the 32-bit product, and that it is replicated in both modes.
- **Q-MASK-21**: mask-output-operators interacting (spec.md:762): add the OFF outward rounding.
- **Q-MASK-22**: mask-output-operators net (spec.md:780): add the lint warnings, and state that CIF and temp layers in the list contribute their whole plane, not only the net.
- **Q-MASK-23**: mask-output-operators bbox (spec.md:812): name the bbox source (the cell being generated, CIFErrorDef), and state that it includes label anchor rects and excludes text extents, plus the `top` parent rule. Check that layout-geometry-model's bbox definition includes label anchors.
- **Q-MASK-27**: mask-output-operators squares (spec.md:416 area): add the OFF sign-aware grid-limit adjustment.
- **Q-MASK-228**: mask-output-operators:637: keep the ON description, and add that with magic-compat OFF passes 1 and 2 enumerate a snapshot of the pass-0 plane.
- **Q-MASK-229**: mask-output-operators:612 and the scenario at 704: mark the full-tile, unscaled erase as magic-compat ON, and add the OFF rule (erase only the clipped part, in mask units).
- **Q-MASK-230**: mask-output-operators:352: qualify 'Trigger tiles ... are not reset (Q-MASK-230)' as magic-compat ON, scoped to the current generation pass, and add that with OFF every searched plane is reset.
- **Q-MASK-231**: mask-output-operators:352: mark the order-dependent seeding sentence as magic-compat ON, and add the OFF rule (every overlapping region is seeded).
- **Q-MASK-232**: mask-output-operators:352: mark 'truncating division (Q-MASK-232)' as magic-compat ON, and add the outward rounding for OFF.

### mask-output-styles

- **Q-MASK-8**: mask-output-styles gridlimit (spec.md:194): add the expander ≠ 10 warning. No behaviour change.
- **Q-MASK-10**: mask-output-styles (spec.md:72): replace the prefix rule with exact-name and base+variant matching, plus the warning.
- **Q-MASK-13**: mask-output-styles (spec.md:454): state that this is replicated in both modes, and cite the corrected lines.
- **Q-MASK-18**: mask-output-styles normalisation pseudocode (spec.md:447): add that OFF recomputes the radius on the g<=1 path.
- **Q-MASK-401**: mask-output-styles:341 says `see-no-vendor` 'affects vendor (read-only) cell handling in hierarchical generation'. Change it to: accepted and stored, with no effect on generation (Q-MASK-401). mask-hierarchical-generation:312/671 already describe the copy correctly.

### gds-read

- **Q-MASK-110**: gds-read 'structure end' step 6 (spec.md:188): state that with magic-compat ON the uncleared planes are painted into the next processed structure, and that with magic-compat OFF the planes are cleared and the structure's geometry is discarded.
- **Q-MASK-111**: gds-read spec.md:238: replace 'rewound during this session' with 'rewound during this session (magic-compat ON) or during this read (magic-compat OFF)'.
- **Q-MASK-113**: gds-read spec.md:378: split by mode. Type 1 is treated as type 0 with magic-compat ON and as type 2 (with a warning) with magic-compat OFF.
- **Q-MASK-114**: gds-read spec.md:382 item 5: mark the per-axis rule and the 'negative ignored' rule as magic-compat ON, and add the OFF rule. oasis-read spec.md:264: change 'negative extensions are ignored' to 'follow gds-read (mode-dependent)'.
- **Q-MASK-115**: gds-read spec.md:383 item 6: limit 'W <= 0 dropped' to magic-compat ON, and add the OFF rule for negative widths (|W|, not magnified, warning).
- **Q-MASK-116**: gds-read spec.md:421: mark the 'head never removed' rule as magic-compat ON, and add that with OFF consecutive duplicates are removed for 0 <= i <= n-2.
- **Q-MASK-118**: gds-read spec.md:462 and the definition of H: mark H = 1.5708 and 'savetheta = 0 for the first segment' as magic-compat ON, and add the OFF values (pi/2; savetheta = theta of segment 1).
- **Q-MASK-119**: gds-read spec.md:487 'Path centerline properties': mark 'values stay in input units' as magic-compat ON, and add that with OFF the values are in internal units.
- **Q-MASK-120**: gds-read spec.md:521 item 2: mark the skip rule as magic-compat ON, and add the OFF rule (WIDTH honored wherever it appears between PRESENTATION or TEXTTYPE and STRANS).
- **Q-MASK-121**: gds-read spec.md:523 item 4, and the pin-rectangle conversion rule: 'truncating toward zero' with magic-compat ON, floor division with OFF.
- **Q-MASK-122**: gds-read spec.md:526: add the MAG formula's +0.5 rounding and the WIDTH formula (CIFScaleCoord, no 8/10) next to the DRC default formula, if they are not already stated.
- **Q-MASK-123**: gds-read spec.md:582 item 3: mark 'even when the kind is text' as magic-compat ON, and add the OFF rule (port data only for placeholders created on port-kind layers).
- **Q-MASK-124**: gds-read spec.md:661 item 2: mark the truncated ysep as magic-compat ON, and add the OFF rounding, symmetric with cols.
- **Q-MASK-125**: gds-read spec.md:688: mark 'ends property processing' as magic-compat ON, and add that with OFF unknown pairs are skipped and parsing continues.
- **Q-MASK-126**: gds-read spec.md:749: replace 'L_BGNSTR + 4 + len'' with 'to the start of the BGNSTR record (using the actual STRNAME record length)', and note that this equals Magic's arithmetic for conforming files.
- **Q-MASK-128**: gds-read spec.md:877: add the rule 'if the path is absolute and starts with $HOME followed by "/", the prefix is replaced by ~', applied to the GDS path after the PDK-variable check. Make the gds-write library-name comparison use the same substitution.
- **Q-MASK-129**: gds-read spec.md:877: 'the path of the file as opened' with magic-compat ON; with OFF, the absolute canonical path, then substituted.
- **Q-MASK-130**: gds-read spec.md:1017: mark 'Setting no SHALL also set the warning level to limit' as magic-compat ON, and state that it has no effect on the warning level with OFF.
- **Q-MASK-132**: gds-read spec.md:1100: add the time-zone override setting and require golden tests to run with a pinned zone. State that the PDK variables come from Tcl globals, seeded from settings on iPadOS (X-4).
- **Q-MASK-134**: gds-read spec.md:204: mark 'year taken as years since 1900', and the 32-bit truncation, as magic-compat ON, and add the OFF rule (year >= 1900 is a full year, no overflow).

### gds-write

- **Q-MASK-200**: gds-write/spec.md:214 (String records, item 6): keep the swap as the magic-compat ON rule. Add that with magic-compat OFF every printable character maps to itself. The name-legality rule (spec:356) uses the same table in both modes.
- **Q-MASK-201**: gds-write/spec.md:277: replace the conditional paragraph with a normative one. UNITS are derived from the current style's angstrom flag on every write, and the Magic session-sticky behaviour is listed as a fixed deviation (X-2 tier 1). oasis-write/spec.md:119 already states this and stays.
- **Q-MASK-202**: gds-write/spec.md:246: keep 'modification date is always the current time' as the magic-compat ON rule. Add that with OFF it is t = 0, and that BGNLIB, the BGNSTR of the full-dump path and the BGNSTR of vendor cells all follow the same rule.
- **Q-MASK-204**: gds-write/spec.md:331 (Structure ordering): replace 'The general order depends on the index's binning state' with a normative description of the bplane order (inBox LIFO, rebuild at 10 or more elements on a non-surrounding search, bin walk) and the DBReComputeBbox re-placement for ON, with the points where each writer enumerates, and the canonical order for OFF. oasis-write/spec.md:460 cross-reference is unchanged.
- **Q-MASK-205**: gds-write/spec.md:606: replace 'The order of equal port indices is unspecified' with 'labels with equal port index are written in reverse label-list order (stable sort of the reversed list)'.
- **Q-MASK-206**: gds-write/spec.md:606: keep the current text as the ON rule. Add the OFF fallback (rectangle on the port-text layer) and the warning in both modes.
- **Q-MASK-207**: gds-write/spec.md:206 (String records, item 2): mark the current rule as magic-compat ON and add the OFF rule (keep the last 32 characters of the unpadded string).
- **Q-MASK-209**: gds-write/spec.md:356 (item 2): replace 'No uniqueness check is made after mapping' with the uniquification rule (`XXXXX<calmanum>`, itself checked) and its warning. Align the wording in oasis-write/spec.md:156, which currently says gds-write does not check.
- **Q-MASK-210**: gds-write/spec.md:24 (item 3): keep the current rule as magic-compat ON; add the OFF rule (test only the basename).
- **Q-MASK-211**: gds-write/spec.md:769 (item 2): replace 'drawn from the C library random() sequence' with the following: ON: the glibc TYPE_3 random() algorithm with seed 1, shared with the `random` command and reseeded by `random seed`. OFF: a hash of the resolved file name. oasis-write/spec.md:460 is updated to match.
- **Q-MASK-212**: gds-write/spec.md:768 (item 1): drop 'when the `gds readonly` setting is currently yes'. Units are always checked, the dump is aborted once per library per write with an error, and Magic's readonly-only check is listed as a fixed deviation.
- **Q-MASK-214**: gds-write/spec.md:775 (item 4) and item 5 / Structure ordering (spec:329): state the ON naming (current text) and the OFF rule (appended structures take the recorded forward-reference name). Add the dangling-reference warning.
- **Q-MASK-215**: gds-write/spec.md:245: keep 'for every structure produced by a full dump it is the timestamp of the vendor cell that triggered the dump' as the magic-compat ON rule. Add that with OFF the creation date is copied from the source structure's BGNSTR unless `datestamp` is set.
- **Q-MASK-216**: gds-write:737: keep 'not rewritten' for compat ON, add a warning, and add OFF-mode rewriting. oasis-write:487 rule 2 ('matching gds-write') must follow the same split.
- **Q-MASK-219**: gds-write:299 step 1: replace 'the already-created output file is left empty' with 'the output file is not created or modified'. The style check must come before the file is opened.
- **Q-MASK-221**: gds-write:615: keep the ON sentence, and add that with magic-compat OFF MAG is divided by the `magscale` setting.
- **Q-MASK-222**: gds-write:385: replace 'computed in 32-bit signed integer arithmetic' with 'computed exactly (64-bit); a value outside the signed 32-bit range fails the write with an error'.
- **Q-MASK-225**: gds-write:383: keep the ON text and add that with OFF the write is refused before the file is opened and the command fails.
- **Q-MASK-226**: gds-write:810: correct 'the cell and all of its ancestors are not written' to 'every cell not yet written is skipped, except undefined-reference cells written at the end', and add the OFF pre-validation (no file created or modified).

### mask-input-styles

- **Q-MASK-300**: mask-input-styles "scalefactor" step 3 (spec.md:160): keep SET for magic-compat ON; add "with magic-compat OFF, M is multiplied by 10/100" and the warning for decimal+unit.
- **Q-MASK-302**: mask-input-styles spec.md:100: describe prefix match as the magic-compat ON behaviour, exact match with OFF, and the tech-load warning in both modes.
- **Q-MASK-303**: mask-input-styles spec.md:138: replace "even when one of the matches is exact (see Q-MASK-303)" with "an exact match is selected; ambiguity is reported only among prefix matches when no exact match exists".
- **Q-MASK-306**: mask-input-styles spec.md:303: make the output-style dependence the magic-compat ON behaviour and add "with magic-compat OFF the square algorithm is always used". Update the Magic ref to CIFgen.c:5078-5079.
- **Q-MASK-307**: mask-input-styles "Input operator boundary" (spec.md:358) and the mask-hints requirement: make the layer-rule OR the magic-compat ON behaviour, add OFF = no-op plus the tech-load warning, state that copyup templayers ignore these operators, and fix the Magic ref to CIFrdcl.c:690-703.
- **Q-MASK-309**: mask-input-styles spec.md:343: replace "The CIF reader SHALL NOT perform the copy into parents" with the ON/OFF split, including the forward-call warning. cif-format-io call requirement: add the OFF copy.
- **Q-MASK-310**: mask-input-styles spec.md:392: make the unscaled search the magic-compat ON behaviour, add "with OFF the label rectangle is scaled by S", and note the IHP dependency. Update the refs to CIFgen.c:5542-5564 and 4058-4105.
- **Q-MASK-311**: mask-input-styles spec.md:478: make "the recorded type is the templayer's CIF-layer index" the magic-compat ON behaviour, add the OFF behaviour (statement ignored with a warning), and add the invalid-index guard in both modes.
- **Q-MASK-326**: mask-input-styles 'GDS layer and datatype mapping': make the '255 or more list entries' clause compat-ON only. OFF has no entry-count limit.
- **Q-MASK-330**: mask-input-styles 'Input operator not-square' and mask-output-operators 'not-square': make 'split tiles are dropped' compat-ON. OFF keeps split tiles unchanged.
- **Q-MASK-334**: mask-input-styles grow-grid (spec.md:317): add the OFF outward-rounding formula, and state that it is the shared function with mask-output-operators.

### cif-format-io

- **Q-MASK-314**: cif-format-io spec.md:232: replace "(without changing the symbol scale, see Q-MASK-314)" with "and the symbol scale numerator is multiplied by 2". Add a golden deviation entry.
- **Q-MASK-315**: cif-format-io spec.md:160: make truncation the magic-compat ON behaviour, state the width-loss case for boxes straddling 0, and add the OFF subdivision.
- **Q-MASK-317**: cif-format-io spec.md:102: make the edit-cell rename the magic-compat ON behaviour and add "with OFF a top-level 9 is ignored with a warning".
- **Q-MASK-318**: cif-format-io spec.md:102: replace "a later DS n ... maps again to a cell found by the name n" and the implied overwrite of renamed cells with "within one read, symbol n (in C and DS) always maps to a cell created by that read; a cell defined earlier in the read is preserved (renamed n_k if it still has the default name); a cell from before the read keeps its name and the new cell is n_k; no existing cell is reused or cleared". Add the DD rule: ON forgets one symbol, OFF every symbol >= n (CIF 2.0).
- **Q-MASK-320**: cif-format-io 'cif read, write and flat options': replace 'Magic always appends .cif (see Q-MASK-320)' with the compat rule (ON always append; OFF append only if missing). Also state that picker-driven read/write passes the root name in both modes.
- **Q-MASK-321**: cif-format-io 'CIF output preconditions and flat output' and mask-hierarchical-generation 'Flat CIF output': describe the ON form (`__CIF2__`, no labels) as compat-ON and add the OFF form (root name, flattened labels as with `flatten`).
- **Q-MASK-322**: cif-format-io 'CIF output labels': replace 'Label text is written verbatim (see Q-MASK-322)' with the compat rule and the OFF substitution set (space, tab, newline, ';' → '_') plus the warning text.
- **Q-MASK-323**: cif-format-io 'CIF output symbol numbering and order': replace 'must be made deterministic (see Q-MASK-323)' with a reference to gds-write's 'use enumeration order' definition in the current mode, used for both numbering and `C` call order, and the ON sampling points (numbering before CIFGenSubcells, calls after).
- **Q-MASK-324**: cif-format-io preamble requirement: state @@tool = 'Hefesto <version>' and @@compiled = Hefesto build time, give the iPadOS source of USER/HOST, and list the six lines masked in golden comparisons.
- **Q-MASK-325**: cif-format-io 'cif see, paint, coverage and statistics options' and tapeout-fill-and-density (density formula, line ~184): replace 'triangles counted as full rectangles (see Q-MASK-325)' with the compat rule (ON full rectangle, OFF half area, exact 64-bit).
- **Q-MASK-327**: cif-format-io 'Label user extensions': spell out the ON search (first CIF-layer index < number of layer/templayer rules whose label type equals the current default type) and the OFF rule (the current `L` layer's kind), for 94 and for 95 without a layer.
- **Q-MASK-331**: cif-format-io 'Read diagnostics': make '(redirection applies only to GDS input)' compat-ON, and add the OFF rule that CIF read messages go to the redirect file.
- **Q-MASK-333**: cif-format-io 'Wire command': drop '(see Q-MASK-333)' and state that square half-width end extensions apply in both modes, noting the difference from the CIF standard's round ends.
- **Q-MASK-335**: cif-format-io 'Calls and transformations': mark 'the call is still made with the transform parsed so far, unscaled' as compat-ON, and add the OFF behaviour (skip the call, clear the pending id).

### mask-hierarchical-generation

- **Q-MASK-400**: mask-hierarchical-generation:264 pseudocode: state the ON/OFF split (OFF: skip include when no other use found). drc-engine:137-160 'Interaction area' (Compatibility: exact) currently computes O as the bbox of other uses with no (0,0,0,0) seed, i.e. it silently has the OFF behaviour; it must add the ON seed and cite Q-MASK-400.
- **Q-MASK-402**: mask-hierarchical-generation:386 and limitation item 3 (687): add the OFF behaviour, and note that bbox/boundary never stop a hierarchical layer because the removal pass excludes them.
- **Q-MASK-403**: mask-hierarchical-generation:149 (removal pass): add the OFF rule (transitive removal of dependents).
- **Q-MASK-404**: mask-hierarchical-generation:113 (radius) and limitation item 4 (688): add the OFF counting rule and the note that some reaches have no bound.
- **Q-MASK-405**: mask-hierarchical-generation:155 and limitation item 2 (686): add the OFF force set.
- **Q-MASK-406**: mask-hierarchical-generation:483: add the OFF filter.
- **Q-MASK-409**: mask-hierarchical-generation:557: replace 'Magic also prints ... informative' with 'Hefesto does not print these lines', and add the OFF geometry rules.
- **Q-MASK-410**: mask-hierarchical-generation:580: add 'with magic-compat OFF the CIF writer honours CIFhier like the GDS writer'.
- **Q-MASK-411**: mask-hierarchical-generation:637 and cif-format-io (flat write): add the OFF behaviour (labels and hints copied, symbol named after the root cell).
- **Q-MASK-413**: mask-hierarchical-generation:653 and cif-format-io:505: add the OFF third-word form and the ON warning. Update D-20 in docs/decisions/audit/DECISIONS.md to the compat option.
- **Q-MASK-414**: mask-hierarchical-generation:727 [platform]: state progress callback, cancellation without partial output, and snapshot isolation.

No spec change beyond the `Resolution:` reference (and evidence line numbers): Q-MASK-11, Q-MASK-20, Q-MASK-26, Q-MASK-203, Q-MASK-217, Q-MASK-223, Q-MASK-301, Q-MASK-316.

Outside the specs: D-20 in `docs/decisions/audit/DECISIONS.md` (Q-MASK-413), and golden deviation entries in `tests/golden/deviations.toml` for Q-MASK-314 and any Q-MASK-10 listing case.
