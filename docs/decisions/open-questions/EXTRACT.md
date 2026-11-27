# Open Questions — EXTRACT

Open questions found while specifying the circuit extractor (capabilities `extract-tech-section`, `extract-nodes-and-labels`, `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-hierarchy`, `extract-substrate`, `extract-file-format`, `extract-commands`). Magic reference: `RTimothyEdwards/magic` @ `fc5daa9e` (8.3.694).

### Q-EXTRACT-1: `defaultsideoverlap` shield direction chosen by plane index, not plane order
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the "forward" direction (which planes lie between the two planes and act as shields) is decided by comparing the plane *indices* `plane1 < plane2`, while the in-between test uses plane *order* values. If the tech declares planes in an order different from `planeorder`, the shield set can be empty or wrong.
- Evidence: extract/ExtTech.c:1800-1820
- Verified 2026-11-13: Confirmed at extract/ExtTech.c:1798-1822 (ExtTechSimpleSideOverlapCap). `forward = (plane1 < plane2)` compares plane indices, while the shield loop compares exts_planeOrder values. When the index order and the plane order disagree, the forward test `pnum3 < pnum2 && pnum3 > pnum1` can never hold, so the shield set is empty. The other overlap parsers (:1243-1268, :1435-1460, :1674-1690) use plane order only. PDK check: sky130A declares cap1/cap2 between the metals but orders them last, and ihp-sg13g2 does the same with mimcap. No defaultsideoverlap rule in sky130A-extract.tech, gf180mcuD-extract.tech or ihp-sg13g2.tech involves those planes, so none of the three PDKs is affected.
- Options: replicate | fix (compare plane order values) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: choose the direction by plane index, as Magic does, so the shield set can be empty or wrong when declaration order differs from planeorder. OFF: choose the direction by plane order (pnum1 < pnum2), so the shields are always the planes strictly between P1 and P2 in plane order. This is a plain bug that changes capacitance values, so the standing compat rule applies. None of the three PDKs changes in either mode; only techs whose plane declaration order differs from planeorder on a sideoverlap pair differ. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON the defaultsideoverlap shield direction is chosen by plane index, as Magic does; with it OFF by plane order, so the shields are always the planes strictly between P1 and P2 in plane order. None of the three reference PDKs differs between the modes. Approved 2026-11-13.

### Q-EXTRACT-2: Diode substrate types parsed from the terminal argument
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: for `device diode|pdiode|ndiode`, the substrate type mask is taken from argument 4 (the same as the terminal types) and the substrate node name from argument 5 (which is the substrate-types field in the documented syntax).
- Evidence: extract/ExtTech.c:2743-2759 (corrected 2026-11-13: The lines are right: extract/ExtTech.c:2743-2757. argv[4] gives both the terminal types and the substrate types, and argv[5] the substrate node name; trailing parameter words are stripped first (argc--, :2560-2620). The premise is wrong, though. Magic's own manual (doc/latexfiles/maint2.tex:2931) documents `device diode model pos_types neg_types` with no substrate field. The 'documented syntax' the entry refers to is Hefesto's spec table, not Magic's. The PDKs depend on Magic's reading: sky130A-extract.tech:1027 `device ndiode ... photo pwell,space/w error` uses argument 5 ('error') as the substrate node name. The other diode lines (sky130 :1139-1147, gf180 :927-930, ihp :5095-5097) give only OTHER. The proposed fix would parse 'error' as a type list and break sky130.)
- Options: replicate | fix (substrate types = argument 5, name = argument 6)
- Recommendation: replicate. Both modes: for diode, pdiode and ndiode, argument 4 (OTHER) is both the terminal type mask and the substrate type mask (empty substrate mask when it is `None`), and argument 5, if present, is the substrate node name. This is Magic's intended syntax, not a bug: the diode's other terminal is the well or substrate. The 'fix' would break sky130A's photodiode line, and Magic defines no other syntax. No lint warning is needed. (corrected 2026-11-13, was replicate with a lint warning)
- Resolution: replicate — in both modes, for diode, pdiode and ndiode, argument 4 (OTHER) is both the terminal type mask and the substrate type mask (empty when `None`), and argument 5, if present, is the substrate node name, as Magic does (sky130A's photodiode line relies on it). No lint warning. Approved 2026-11-13.

### Q-EXTRACT-3: Device perimeter capacitance scaled as an area
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the per-perimeter capacitor coefficient (`gscap`) is multiplied by `dscale²` in micron conversion and by `(n/d)²` on grid rescale, like an area coefficient.
- Evidence: extract/ExtTech.c:3873-3874, extract/ExtTech.c:4046-4049 (corrected 2026-11-13: The behaviour is confirmed but the line numbers are off by about 10. Micron conversion is at extract/ExtTech.c:3884 (`exts_deviceSDCap *= sqfac`, sqfac = dscale²), and grid rescale at :4051-4052 (`*= sqn; /= sqd`). gscap is multiplied by the perimeter at extract/ExtBasic.c:1930 (the `c` parameter) and at :3264 (the value of a capacitor with no model), so it reaches the .ext output. PDK check: sky130A's only `device capacitor` line (sky130A-extract.tech:1163) gives a single number, which is the area cap. No PDK sets gscap, so none is affected.)
- Options: replicate | fix (scale by `dscale` and `n/d`) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: scale gscap like an area coefficient (dscale² and n²/d²), as Magic does. OFF: scale it as a per-length coefficient (dscale and n/d), like perimeter caps. This is a unit bug that changes the `c` parameter and the values of capacitors with no model, so the compat rule applies. The three PDKs are unaffected because none declares a perimeter cap on a device. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON gscap is scaled like an area coefficient (dscale², n²/d²), as Magic does; with it OFF like a per-length coefficient (dscale, n/d), as perimeter caps are. No reference PDK sets gscap. Approved 2026-11-13.

### Q-EXTRACT-4: Name selection when the first attached label is an attribute
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: non-port labels are inserted after the head of the label list; if the head is an attribute label, each later name label is inserted at position 2, so the most recently attached name wins instead of the first.
- Evidence: extract/ExtRegion.c:236-252 (corrected 2026-11-13: The behaviour is confirmed but the lines are wrong. Insertion is at extract/ExtRegion.c:315-331 (lreg_labels) and :489-503 (nreg_labels), not ExtRegion.c:236-252. Port labels are pushed at the head. Every other label goes after the run of port labels at the head, or becomes the head when the list is empty. So when the head is a non-port label, each later non-port label is inserted at position 2. extNodeName (extract/ExtBasic.c:993-1007) returns the first LABTYPE_NAME label. Result: with a name label at the head, the first name wins; with an attribute at the head, the most recently attached name wins.)
- Options: replicate | fix (always keep first name label) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: keep Magic's list order, so the most recently attached name label wins when the first attached label was an attribute. OFF: keep attachment order for non-port labels, so the first attached name label always wins; port precedence is unchanged. This is an ordering quirk that changes node names in .ext and netlists, so the compat rule applies. Golden cases need an attribute label attached before two name labels. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON node names follow Magic's list order, so the most recently attached name label wins when the first attached label is an attribute; with it OFF the first attached name label always wins. Port precedence is the same in both modes. Golden cases attach an attribute label before two name labels. Approved 2026-11-13.

### Q-EXTRACT-5: Record order depends on corner-stitched tile enumeration
- Kind: ambiguity
- Magic behavior: the order of `node`, `device`, `use` and `port` records is the reverse of the discovery order of tiles/regions under Magic's tile-plane area enumeration (and subcell-plane enumeration for uses).
- Evidence: extract/ExtBasic.c:5057-5200, extract/ExtBasic.c:4976-5005, extract/ExtCell.c:617
- Options: replicate (Hefesto's tile planes use the same maximal-horizontal-strip decomposition and enumeration order) | fix (sort by reference point) and compare golden files order-insensitively
- Recommendation: replicate if `layout-geometry-model` is bit-compatible; otherwise compare records as sets per record class and document.
- Resolution: compat — record order follows the tile order bullet: Hefesto reproduces Magic's maximal-horizontal-strip decomposition and enumeration order, so `node`, `device`, `use` and `port` order is byte-identical (X-1 as amended by D-1, 2026-11-07).

### Q-EXTRACT-6: Temporary unique labels not reverted for some cells
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: after an extraction run with `do unique`, temporary `_uqN` labels are reverted only for cells whose isolated-substrate preparation returned a saved plane; cells without isolated substrate regions (or in styles without a default substrate type) keep the renamed labels. The guard `if (EXT_DOUNIQUE)` tests a constant and is always true.
- Evidence: extract/ExtMain.c:744-752, extract/ExtMain.c:607-612 (corrected 2026-11-13: The behaviour is confirmed, and it is wider than the entry says. The lines are wrong: the reverts are at extract/ExtMain.c:842-848 (ExtractOneCell) and :1108-1115 (extExtractStack), not :744-752/:607-612. `if (EXT_DOUNIQUE)` tests the constant 0x100, so it is always true. Reverts happen only for cells on the saveList, which holds only cells whose extPrepSubstrate/ExtCell returned a saved plane. In ExtractOneCell the top def is never passed to ExtRevertUniqueCell at all. The renames are permanent edits: extract/ExtCell.c:490-500 calls extUniqueCell(EXT_UNIQ_TEMP*), which does DBRemoveLabel+DBPutFontLabel (extract/ExtUnique.c:335-339), marking the cell CDMODIFIED, with undo disabled. A later save writes the `_uqN` labels to the .mag file.)
- Options: replicate | fix (revert every temporarily renamed cell)
- Recommendation: fix. Both modes: after every extraction run with `do unique`, revert the temporary labels in every cell that was renamed, the top cell of `extract`/`extract cell` included. Restore the cell's modified flag if the renaming was its only change. Leaving `_uqN` labels silently changes the saved layout, which is X-2 tier 1 data loss, and the revert does not change .ext content. Magic's ON output stays byte-identical.
- Resolution: fix — in both modes, after every extraction run with `do unique` the temporary `_uqN` labels are reverted in every renamed cell, the top cell of `extract`/`extract cell` included, and the cell's modified flag is restored when the renaming was its only change (X-2 tier 1). The .ext output is unchanged. Approved 2026-11-13.

### Q-EXTRACT-7: Hash- and address-ordered output
- Kind: ambiguity
- Magic behavior: `cap` records are emitted in hash-table iteration order, and within a record the two names are ordered by the memory addresses of the node structures; `merge` groups are emitted in string-hash iteration order.
- Evidence: extract/ExtCouple.c:289-322, extract/ExtCouple.c:557-562, extract/ExtHier.c:1111-1180
- Options: replicate (impossible to reproduce addresses) | define a deterministic order (e.g., by first appearance) and compare unordered
- Recommendation: Hefesto writes a deterministic order; golden comparison treats `cap` pairs as unordered and sums duplicates, and compares merge name sets.
- Resolution: decided by X-1 as amended (2026-11-06, D-1). `merge` groups are name-keyed and reproduced in Magic's hash order; `cap` records and their terminal order are address-keyed, written in the deterministic order and canonicalized (capability `extract-file-format`).

### Q-EXTRACT-8: Zero-size capacitor writes dimensions twice
- Kind: bug
- Magic behavior: a capacitor device with a model and no terminal writes ` 0 0` and then the regular ` L W` fields with uninitialized L and W.
- Evidence: extract/ExtBasic.c:3135-3142, extract/ExtBasic.c:3184-3196
- Options: replicate (undefined values) | fix (write ` 0 0` once)
- Recommendation: fix.
- Resolution: fix — the zero-size capacitor writes ` 0 0` once and no uninitialised L and W (X-2 tier 1, 2026-11-07).

### Q-EXTRACT-9: `$`-variable substitution in substrate names [platform]
- Kind: platform
- Magic behavior: substrate names beginning with a single `$` are replaced by the value of the Tcl global variable of that name (or the name without `$` when undefined); device substrate names are substituted only in backward-compatible substrate mode.
- Evidence: extract/ExtBasic.c:900-925, extract/ExtBasic.c:2617-2630
- Options: replicate with Hefesto's scripting variables | platform alternative (project-level variable table on iPadOS)
- Recommendation: provide a variable table in the session; strip `$` when undefined, as Magic does.
- Resolution: platform — the host variables are the global variables of the embedded Tcl interpreter, available on every platform including iPadOS; an undefined name is used without its `$`, as in Magic (X-3, 2026-11-07).

### Q-EXTRACT-10: Version string written in `.ext`
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: writes `version 8.3` (Magic major.minor); readers warn when it differs from their own version.
- Evidence: extract/ExtCell.c:578, utils/buildinfo.c:19, extflat/EFread.c:604-612
- Verified 2026-11-13: Confirmed. The writer is at extract/ExtCell.c:576, not :578: `fprintf(f, "version %s\n", MagicVersion)`. MagicVersion is major.minor (utils/buildinfo.c:19, utils/magic_buildinfo.h:19). The reader is at extflat/EFread.c:604-611: it warns 'Cell was extracted using version %s of the extractor. It should be re-extracted.' whenever the value differs from its own.
- Options: write `8.3` | write a Hefesto version
- Recommendation: replicate. Both modes: write `version 8.3`, treating it as the .ext format version rather than Hefesto's version. When reading, warn as Magic does when the value is not 8.3. A Hefesto version string would make every Magic reader warn and would break byte-identical goldens, and Hefesto's own version belongs in the Hefesto headers, not in this field.
- Resolution: replicate — in both modes Hefesto writes `version 8.3` as the .ext format version, and the reader warns, as Magic does, when the value is not 8.3. Hefesto's own version goes in its headers, not in this field. Approved 2026-11-13.

### Q-EXTRACT-11: Output path creation and current-directory fallback [platform]
- Kind: platform
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: creates the `extract path` directory with `mkdir 0755`, falls back to the current working directory when the `.mag` directory is not writable, and uses search paths to open the file.
- Evidence: extract/ExtCell.c:150-245
- Verified 2026-11-13: Confirmed at extract/ExtCell.c:152-250 (ExtFileOpen). With `extract path`, a missing directory is created with mkdir 0755 and the file falls back to <cellname> in the cwd when creation fails. With no path, the file goes next to cd_file with the extension replaced. Opening uses PaOpen with Path/CellLibPath. A failed write is retried as `<basename>.ext` in the cwd unless `<basename>.mag` exists there.
- Options: replicate on Linux/macOS | platform alternative on iPadOS (write into the project container; no cwd)
- Recommendation: platform. Desktop (macOS, Linux): replicate Magic exactly, including the mkdir 0755, the cwd fallback and the .mag-conflict test. iPadOS and any other sandboxed host (X-4): 'current directory' is the project folder, which is the session working directory granted to the sandbox. Relative `extract path` values resolve against it, and directories are created through the file-coordination service, with no POSIX mode. A path outside the granted roots is refused with the same 'Cannot open output file' outcome. Messages stay Magic's text.
- Resolution: platform — on desktop hosts (macOS, Linux) output paths behave exactly as in Magic (mkdir 0755, cwd fallback, .mag-conflict test); on sandboxed hosts (X-4) the current directory is the project folder, directories are created through file coordination, and a path outside the granted roots fails with Magic's 'Cannot open output file' outcome. Approved 2026-11-13.

### Q-EXTRACT-12: Path-length values depend on tile decomposition
- Kind: ambiguity
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `distance` min/max are computed by flooding tiles with costs between boundary midpoints, so values depend on how the plane is cut into tiles.
- Evidence: extract/ExtLength.c:770-1040
- Verified 2026-11-13: Confirmed at extract/ExtLength.c:770-1040. extPathFlood is a depth-first flood over the net yanked into __PATHYANK__. It marks tiles, walks the neighbours in a fixed order (top, then the other sides), and keeps epa_min/epa_max. extPathTileDist (:1015-1040) is integer Manhattan arithmetic plus the tile width or height when entry and exit are on the same side. Marks are not cleared on return: every tile, the destination tile included, stays MARKED until all start tiles under lab1 are done (ExtLength.c:647-674), so only the first path to reach a tile counts and min/max come from first-arrival paths only. The contact branch revisits a tile on another plane only if `TiGetClient == CLIENTDEFAULT` (:864); the non-contact case searches the other connected planes over the tile area plus a 1-unit halo (:878-905). The results therefore depend only on the tile planes and the walk order. Q-EXTRACT-5 is already resolved to reproduce Magic's maximal-horizontal-strip decomposition and enumeration order (X-1/D-1).
- Options: replicate tile-exact | specify behavioral tolerance
- Recommendation: replicate. Both modes: port the flood exactly, with the same yank, neighbour walk order, marking and integer distances. `distance` min and max are then byte-identical. The tolerance fallback is no longer needed, because Q-EXTRACT-5 already commits Hefesto to Magic's tile decomposition and enumeration order. A tolerance would only hide port errors. (corrected 2026-11-13, was behavioral, golden comparison within a tolerance)
- Resolution: replicate — in both modes the path-length flood is ported exactly (yank, neighbour walk order, marking, integer distances), so `distance` min and max are byte-identical (`Compatibility: exact`); no tolerance, since Q-EXTRACT-5 fixes the tile decomposition. User choice (a) of the closest calls. Approved 2026-11-13.

### Q-EXTRACT-13: Diode parameter matching uses uninitialized dimensions
- Kind: bug
- Magic behavior: for diode classes, model selection by parameter ranges is called before length and width are computed (uninitialized values).
- Evidence: extract/ExtBasic.c:2930-2938
- Options: replicate (undefined) | fix (compute L,W first, or treat as 0)
- Recommendation: fix by computing L and W (as for capacitors) before range matching; ranges on diode L/W are rare.
- Resolution: fix — L and W are computed before parameter-range matching for diode classes, so no uninitialised values are read (X-2 tier 1, 2026-11-07).

### Q-EXTRACT-14: Documentation mismatches for `extract do all` and defaults
- Kind: doc-mismatch
- Magic behavior: `all` = adjust, capacitance, coupling, lumped, length and unique; startup defaults also enable fringehalo (format-dependent), labelcheck and aliases. The HTML doc says aliases and labelcheck are off by default; the Hefesto overview (`docs/reference/magic/circuit-extraction.md`) claims `all` includes labelcheck and aliases.
- Evidence: extract/extract.h:70-96, extract/ExtMain.c:56, doc/html/extract.html
- Options: follow code | follow doc
- Recommendation: follow code (specified in `extract-commands`); correct the overview document.
- Status: the overview part is done. `docs/reference/magic/circuit-extraction.md` now lists `all` correctly. The HTML-doc mismatch remains informative only.
- Resolution: n/a — spec follows Magic's code; documentation is wrong (`extract-commands` follows the code; the overview is corrected) (spec conventions §2.7, 2026-11-07).

### Q-EXTRACT-15: `r` parameter with zero width or zero area
- Kind: bug
- Magic behavior: the `r` device parameter divides by device width or area without checking for zero (integer division by zero or float-to-int overflow).
- Evidence: extract/ExtBasic.c:1975-2010
- Options: replicate (crash/undefined) | fix (write 0)
- Recommendation: fix (write `r=0`).
- Resolution: fix — `r` is written as 0 when the width or area is zero, instead of dividing by zero (X-2 tier 1, 2026-11-07).

### Q-EXTRACT-16: `device` cell property dimensions
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: for mosfet-like classes the property's width and length are written as `W L` (normal device records write `L W`), and coordinates/dimensions are multiplied by units-per-lambda (truncated), unlike internal-unit device records.
- Evidence: extract/ExtBasic.c:430-520, extract/ExtBasic.c:560-575
- Verified 2026-11-13: Confirmed, with nuance (extract/ExtBasic.c:353-556, not only :430-575). The property tokens are read in order: llx, lly, urx and ury each go through `int *= float exts_unitsPerLambda`, which truncates. For mosfet, asymmetric and bjt, the next two tokens are read into w and then l, and are written back as ` %d %d` w, l. So the output keeps the property's own token order, and the code merely labels the first token 'width'. Normal device records write length then width (ExtBasic.c:2891). fet areas use upl² and perimeters upl. With a model (not `None`), `device resistor` also reads two tokens into w and l scaled by upl (ExtBasic.c:476-490), and `device capacitor`/`capacitor reversed` read an area scaled by upl² and a perimeter scaled by upl (:496-512); all are passed through in token order. After scaling, a degenerate box is widened: `if (urx <= llx) urx++` (:429) and `if (ury <= lly) ury++` (:440).
- Options: replicate | fix
- Recommendation: replicate. Both modes: pass the property's dimension tokens through in their written order, scaled by units-per-lambda with truncation to int, exactly as Magic does. There is no corrected order to choose: Magic copies the values, and the property's author sets their meaning. Swapping them would break every consumer of existing properties. Truncation only matters with a fractional units-per-lambda, where no exact answer exists either.
- Resolution: replicate — in both modes the device cell property's dimension tokens are passed through in written order, scaled by units-per-lambda with truncation, as Magic does; there is no reordering. Approved 2026-11-13.

### Q-EXTRACT-17: Shield fractions accumulate across multiple sideoverlap rules
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: in the fringe-halo model, the shielded substrate fraction is summed over every matching rule, so two rules targeting the same type can over-subtract substrate fringe; shield fractions use the boundary's stored inside type (a contact type is not reduced to its residue) for the multiplier.
- Evidence: extract/ExtCouple.c:1560-1600, extract/ExtCouple.c:680-700
- Verified 2026-11-13: Confirmed. In extSideOverlapHalo (extract/ExtCouple.c:1420-1640), the rule loop (about :1530-1580) adds `subfrac += sov.so_subfrac` once per matching rule, and only when rtp != rbp. extSubtractSideOverlap (:672-740) computes the multiplier from `ta = TiGetType(bp->b_inside)` (:688) without reducing a contact to its residue; extSideOverlapHalo itself does reduce it. fringehalo is on by default (ExtMain.c:58), so this is the default path.
- Options: replicate | fix (count shielding once) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: sum the shield fractions over every matching rule, accumulate only when the nodes differ, and use the unreduced inside type, as Magic does. OFF: compute the shielded fraction once per overlapped tile, whatever the rule count or node identity, using the contact's residue type for the multiplier. Over-subtracting substrate fringe is a bug that changes capacitance values, so the compat rule applies. Duplicate rules for one (ta, tb) pair are rare in the PDKs, so ON and OFF mostly agree there except for contact edges. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON shield fractions are summed over every matching sideoverlap rule, only between different nodes, with the unreduced inside type, as Magic does; with it OFF the shielded fraction is computed once per overlapped tile using the contact's residue type. Approved 2026-11-13.

### Q-EXTRACT-18: Legacy sideoverlap substrate subtraction uses whole-segment length
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `min(accounted, length)` uses the overlap of the target tile with the whole boundary segment, not with the shadow sub-segment being processed.
- Evidence: extract/ExtCouple.c:1689-1698, extract/ExtCouple.c:1790-1800
- Verified 2026-11-13: Confirmed. The lines are off by about 10: extract/ExtCouple.c:1677-1686 computes `length` as the overlap of tp with the whole bp->b_segment, and :1780-1782 does `subcap = perimCap * MIN(areaAccountedFor, length)`. areaAccountedFor is accumulated over every matching rule from the clip to esws->area. MIN with the whole-segment length therefore caps it more loosely than the clipped length would, and only matters when several rules match. This path is used only with fringehalo off.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: cap the accounted length by tp's overlap with the whole boundary segment, as Magic does. OFF: cap it by the length of the clipped shadow sub-segment being processed, with the accounted length counted once per tile, not per rule. This quirk changes values, so the compat rule applies. It only affects the legacy model (fringehalo off) with several matching rules. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON the legacy sideoverlap substrate subtraction caps the accounted length by the overlap with the whole boundary segment, as Magic does; with it OFF by the clipped sub-segment length, counted once per tile. Only the legacy model (fringehalo off) is affected. Approved 2026-11-13.

### Q-EXTRACT-19: Truncations in step and halo parsing
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `step` is truncated to an integer before micron conversion; `sidehalo` is kept in milli-units, converted with rounding, then integer-divided by 1000 (truncation), so `sidehalo 2.5` in lambda gives 2.
- Evidence: extract/ExtTech.c:3362-3375, extract/ExtTech.c:3444-3452, extract/ExtTech.c:3986-3997
- Verified 2026-11-13: Confirmed. extract/ExtTech.c:3362-3373 reads sidehalo as %lg, multiplies it by 1000 and casts to int. :3444-3451 sets step to (int)atof. In micron styles, :3973-3976 does halo/dscale+0.5 and step/dscale+0.5; :3990 then does halo /= 1000 (integer). So `sidehalo 2.5` in a lambda style gives 2 and `step 7.5` gives 7. PDK check: sky130A, gf180mcuD and ihp-sg13g2 all use `step 7` and `sidehalo 8` (microns). These are exact multiples after conversion (dscale 0.01 µm/λ for sky130), so all three PDKs come out the same in both modes.
- Options: replicate | fix (round) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: truncate as Magic does. OFF: parse step as a double and round both values to the nearest internal unit after unit conversion, with no integer truncation of fractional lambda values. The ×1000 is meant to keep fractional values, so truncating them is unintended, and it changes halo and step, hence coupling caps and hierarchical interaction areas. The step ≤ 0 check applies to the rounded value. Integer micron values in the PDKs give the same result in both modes. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON step and halo are truncated as Magic does; with it OFF step is parsed as a double and both values are rounded to the nearest internal unit after unit conversion, and the step ≤ 0 check uses the rounded value. The reference PDKs give the same values in both modes. Approved 2026-11-13.

### Q-EXTRACT-20: Single-precision denominator in sidewall coupling
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `sep + offset` is computed in single precision because the offset is stored as `float`; results can differ in the last digits from a double computation.
- Evidence: extract/extractInt.h:349-351, extract/ExtCouple.c:3127 (corrected 2026-11-13: The behaviour is confirmed but one line is wrong: ec_offset is declared `float` at extract/extractInt.h:476, not :349-351, which is struct extTree. extract/ExtCouple.c:3127 computes `swcap = (e->ec_cap * overlap) / (sep + e->ec_offset)`: int + float is evaluated in float, then double/float. The offset is also stored and scaled as float (ExtTech.c:3959, :4116-4117).)
- Options: replicate | use double
- Recommendation: replicate. Both modes: store the sidewall offset as a 32-bit float and form `sep + offset` in single precision, as Magic does. This is an internal number-representation choice, not a behaviour error. The difference is in the last digits, below any physical meaning, but it shows up in %lg output, so byte identity needs it. A second double path would only double the test matrix.
- Resolution: replicate — in both modes the sidewall offset is a 32-bit float and `sep + offset` is evaluated in single precision, as Magic does, so %lg output is byte-identical. Approved 2026-11-13.

### Q-EXTRACT-21: Fractional parameter limits in lambda units
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: parameter range limits are stored ×1000 and integer-divided by 1000 after conversion, so in lambda styles limits below 1 λ collapse to 0 and `l>=0.15` behaves like `l>=0`.
- Evidence: extract/ExtTech.c:2585-2600, extract/ExtTech.c:4000-4015
- Verified 2026-11-13: Confirmed. Parsing at extract/ExtTech.c:2596-2611 stores ival = (int)(0.5 + V·1000), with ±1 for strict comparisons. Micron styles divide the int limits by dscale or dsq at :3897-3906 (truncating on assignment). :4003-4010 then integer-divides the limits and the offset by 1000. In lambda styles, `l>=0.15` therefore becomes minimum 0, and a strict `>` loses its +1. The three PDKs use micron units with limits that convert exactly, so none is affected.
- Options: replicate | fix (keep fractional limits in internal units) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: truncate as Magic does. OFF: keep the limits in milli-internal-units and compare 1000·L (or W, A) against them, so fractional limits and strict comparisons keep their meaning. Collapsing a limit to 0 changes model selection, which is an output value, so the compat rule applies. Offsets follow the same rule. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON fractional parameter limits and offsets are truncated by the /1000 conversion, as Magic does; with it OFF they are kept in milli-internal-units and compared against 1000·L (W, A). The reference PDKs are unaffected. Approved 2026-11-13.

### Q-EXTRACT-22: Shorted-ports warning suppressed by lumped resistance
- Kind: bug
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the warning `Ports "a" and "b" are electrically shorted.` is suppressed when the *lumped* resistance option is on (default), although the comment says it should be suppressed only for full R-C extraction.
- Evidence: extract/ExtBasic.c:815-838
- Verified 2026-11-13: Confirmed. extract/ExtBasic.c:826-829 tests `!(ExtOptions & EXT_DORESISTANCE)`, which is lumped resistance (0x008, part of EXT_DOALL and on by default, ExtMain.c:58). The comment at :820-823 says the warning should be skipped only when extresist runs, which is EXT_DOEXTRESIST (0x200). In the default configuration the warning therefore never prints. Only console text is affected; the .ext is not.
- Options: replicate | fix (test the extresist option) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: suppress the warning whenever lumped resistance is on, as Magic does. OFF: suppress it only when full R-C extraction (extresist) runs in the same pass, as the comment intends. The .ext output is the same in both modes. The golden harness compares Magic console text (--diagnostics=magic), so the ON default must stay silent to match. (corrected 2026-11-13, was fix)
- Resolution: compat — with `magic-compat` ON the shorted-ports warning is suppressed whenever lumped resistance is on, as Magic does; with it OFF only when extresist runs in the same pass. The .ext output is the same in both modes. Approved 2026-11-13.

### Q-EXTRACT-23: Which node keeps its name under unique renaming
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: when a duplicate is found, the node being visited (later in node-list order) keeps the name and all others are renamed, unless the earlier node holds a port label with that text; the result depends on node-list order (Q-EXTRACT-5).
- Evidence: extract/ExtUnique.c:150-190
- Verified 2026-11-13: Confirmed at extract/ExtUnique.c:155-188. On a duplicate, if the current label is not a port and the earlier node holds a port label with the same text, extMakeUnique runs keeping the earlier node, and the hash is not marked processed. Otherwise the current node (later in node-list order) keeps the name and the text is marked processed. Renaming is at :265-345.
- Options: replicate | fix (keep first by position)
- Recommendation: replicate. Both modes: the node visited later keeps the name, except that a port on the earlier node wins over a non-port, exactly as Magic does. This is a design choice with no defined 'correct' winner, not a bug. Node-list order is already reproduced byte-identically under Q-EXTRACT-5, so the result is deterministic and matches Magic's names.
- Resolution: replicate — in both modes the node visited later keeps the name under unique renaming, except that a port on the earlier node wins over a non-port, as Magic does; node-list order is fixed by Q-EXTRACT-5. Approved 2026-11-13.

### Q-EXTRACT-24: Inconsistent concave-corner tests
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: the substrate corner correction calls a corner concave when the neighbor tiles' *types* connect to the node type; the coupling corner correction requires the neighbor tiles to belong to the same *node*.
- Evidence: extract/ExtBasic.c:5430-5436, extract/ExtCouple.c:2370-2400
- Verified 2026-11-13: Confirmed. The substrate corner correction at extract/ExtBasic.c:5434-5438 calls a corner convex when both tiles are space, and concave when both tile types are in the connect mask; it does not check the free quadrant. The coupling correction at extract/ExtCouple.c:2337-2362 requires node identity (ExtGetRegion == rbp) and also requires the free quadrant (b_outside) to be space. The two differ when the tiles connect by type but are different nodes (diagonal point contact) or the free quadrant is not space.
- Options: replicate | fix (use node identity in both) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: keep both tests as Magic has them. OFF: use the coupling test in both places, requiring node identity and a free quadrant that is space. The divergence is a bug that changes corner fringe values, so the compat rule applies. It only shows up at diagonal point contacts or at corners whose free quadrant holds other material. (corrected 2026-11-13, was replicate)
- Resolution: compat — with `magic-compat` ON substrate corner fringe treats a corner as concave by Magic's type test; with it OFF it uses the coupling test (same node and a free quadrant that is space) in both places. Approved 2026-11-13.

### Q-EXTRACT-25: Halo/step overrides lost on style reload
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: `extract halo` and `extract stepsize` modify the loaded style in place; reloading or changing the style (or a grid rescale that reloads) restores tech values.
- Evidence: commands/CmdE.c:1240-1300, extract/ExtTech.c:1084-1109
- Verified 2026-11-13: Partly confirmed. commands/CmdE.c:1241-1300 writes ExtCurStyle->exts_sideCoupleHalo and exts_stepSize in place. ExtLoadStyle (extract/ExtTech.c:1084-1109) reinitializes and reloads the style, restoring the tech values; it is reached through `extract style` (:796) and ExtCompareStyle (:299). Correction: a grid rescale does not reload the style. ExtTechScale scales the overrides along with everything else (ExtTech.c:4038-4039, called from CmdRS.c:330 and DBio.c:801).
- Options: replicate | persist overrides as session options
- Recommendation: replicate. Both modes: `extract halo` and `extract stepsize` change the loaded style. Selecting or reloading a style, or a tech load, restores the tech values. A grid rescale scales the override like any other style value. This is Magic's documented style model, not a defect, and it has no output to correct.
- Resolution: replicate — in both modes `extract halo` and `extract stepsize` change the loaded style: selecting or reloading a style, or a tech load, restores the tech values, and a grid rescale scales the overrides. Approved 2026-11-13.

### Q-EXTRACT-26: Cumulative-area sticky-label merge may create phantom nodes
- Kind: quirk
- Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)
- Magic behavior: with `labelcheck`, sticky labels created by the hard-way search can produce merges to names that describe a single array element instead of a range (noted in source).
- Evidence: extract/ExtHier.c:330-345 (corrected 2026-11-13: The lines are wrong. The author's note is at extract/ExtHier.c:521-526, inside extHierConnectFunc1 (code :528-575), not :330-345, which is array-name construction and node unification. With labelcheck on, every LABEL_STICKY label in the cumulative def that touches the tile is unified with the tile's node name. For arrays, that name describes one element, not a range. The source author is 'not certain' the code should be restricted to user sticky labels.)
- Options: replicate | restrict to user sticky labels
- Recommendation: replicate. Both modes: unify every sticky label in the cumulative def, as Magic does. Magic defines no corrected behaviour: its author was unsure of the restriction, and Hefesto has no reliable way to tell hard-search labels from user sticky labels without inventing semantics for array ranges. The phantom names are extra aliases in `merge` records that the netlisters resolve. Add golden cases with arrays and labelcheck.
- Resolution: replicate — in both modes every sticky label in the cumulative def is unified, as Magic does; the resulting extra aliases are resolved by the netlisters. Golden cases cover arrays and labelcheck. Approved 2026-11-13.

### Q-EXTRACT-27: An interrupted extraction or extresist run leaves truncated output files that later runs treat as current
- Kind: bug
- Status: approved (recommendation drafted 2026-11-20 in the M7 plan, approved 2026-11-21)
- Magic behavior: ExtCell opens `<cell>.ext` for writing before it extracts anything (extract/ExtCell.c:99) and writes the header first, `timestamp <cd_timestamp>` on the first line (extCellFile, :515). When the run is interrupted, each later phase is skipped by its `SigInterruptPending` test (:515-531) and the file is closed as it is (:116). The next `extract` reads only the first line to decide whether a cell is current (extTimestampMisMatch, extract/ExtMain.c:968-990), so the truncated file passes, and the cell is marked CDNOEXTRACT and skipped (:295-296). `ext2spice`, `ext2sim`, `extresist`, `antennacheck` and `def write` then read a partial netlist without any message. extresist has the same flaw: it stops at the next node on interrupt (resis/ResRex.c:1395) and leaves a truncated `.res.ext` (and `.res.lump`, `.fh`). That file is newer than the `.ext`, so the `rc` staleness rule of `simulation-netlist-preparation` (run extresist when `.res.ext` is missing or older) trusts it, and `ext2spice extresist on` reads it silently.
- Evidence: extract/ExtCell.c:99-116, extract/ExtCell.c:153-251 (ExtFileOpen), extract/ExtCell.c:515-531, extract/ExtMain.c:295-296, extract/ExtMain.c:968-990, resis/ResRex.c:1395; extract-commands "Incremental extraction"; simulation-netlist-preparation (mode `rc`); openspec/changes/m7-extraction design D12, D16, D18
- Options: fix (write each file under a temporary name in the same directory and rename it when the cell is complete; a cancelled run keeps the previous file) | compat (ON: Magic's truncated file; OFF: the fix) | replicate
- Recommendation: fix. Both modes, for `.ext`, `.res.ext`, `.res.lump` and `.fh`: the target path is first resolved exactly as ExtFileOpen does (`ExtLocalPath`, then the directory of `cd_file`, then the retry with the path stripped); the file is written under a temporary name in the directory of that resolved path and renamed over the target only after the last record; on cancel or error the temporary file is removed and the previous file, if any, is left unchanged. When the target is a symbolic link, Magic writes through it; Hefesto resolves the link and renames over its target, so the link survives. If no temporary file can be created in that directory (a writable file in a read-only directory), Hefesto writes in place as Magic does. This is silent data loss under X-2 tier 1: the truncated file has a valid timestamp, so nothing tells the user, and every netlist built from it is wrong. Uninterrupted runs give the same bytes, the same file names and the same console output as Magic, so no golden case changes. On sandboxed hosts the temporary file and the rename go through file coordination, as Q-EXTRACT-11 does for directories.
- Resolution: fix — in both modes each `.ext`, `.res.ext`, `.res.lump` and `.fh` file is written completely or not at all: the target path is resolved as ExtFileOpen does, the file is written under a temporary name in the directory of the resolved path and renamed over the target (a symbolic link's target when the target is a link) after the last record; on cancel or error the temporary file is removed and the previous file is left unchanged; if no temporary file can be created there, the file is written in place as Magic does. Uninterrupted runs are byte-identical to Magic, so no golden case changes. Specs: `extract-commands` "Incremental extraction" and "Output path and file location", `extresist-output` "Overlay file grammar and order". Approved 2026-11-21.

### Q-EXTRACT-28: `extract list` or `extract listall` with nothing after it crashes
- Kind: bug
- Status: approved 2026-11-22 (proposed in M7 round R1, 2026-11-21; approved with the recommended option)
- Magic behavior: CmdExtract removes the `list`/`listall` word and then looks up `argv[1]`, which is the NULL terminator: Lookup dereferences NULL and Magic crashes.
- Evidence: commands/CmdE.c:1145-1162; extract-commands
- As implemented (round R1, scaffold): both modes print `Wrong number of arguments in "extract" command.  Try "extract help" for help.`
- Options: fix (as implemented) | another message
- Recommendation: fix.
- Resolution: fix — in both modes `extract list` or `extract listall` with nothing after it prints `Wrong number of arguments in "extract" command.  Try "extract help" for help.` and does nothing else, instead of Magic's NULL dereference (X-2 tier 1). Spec: `extract-commands` "Command syntax and option matching". Test: `tests/unit/core/m7_scaffold_test.cpp` "extract list and listall with no option print the argument-count message". Approved 2026-11-22.

### Q-EXTRACT-29: TiSplitX on a split (diagonal) tile moves its triangle
- Kind: bug
- Status: approved 2026-11-22 (raised in M7 round R2 from the round R1 MODEL review, 2026-11-22; approved with the recommended option)
- Magic behavior: TiSplitX cuts a tile at x without looking at its body (tiles/tile.c:250-300): the new right-hand tile gets body 0 and the original keeps its `TT_DIAGONAL`, `TT_SIDE` and `TT_DIRECTION` bits, so the diagonal now spans the narrower left tile and the triangle's area and slope change. ResSplitX then gives the new tile `TiGetType(tile)`, the left type only (resis/ResFract.c:436-446). extresist's fracture plane is the only caller that can meet a split tile (net geometry with non-Manhattan pieces).
- Evidence: tiles/tile.c:250-300, resis/ResFract.c:424-490; m7-extraction design D6, D16; tasks.md 3.4 (as landed: `db::StitchEditor::split_x` and `join_y` refuse split tiles in both modes and change nothing)
- Options: compat (ON: copy Magic's cut of a split tile; OFF: refuse, as `StitchEditor` does today) | fix (refuse in both modes) | replicate
- Recommendation: compat. With `magic-compat` ON the stitch editor copies Magic's cut; with it OFF it refuses a split tile and changes nothing. Today `StitchEditor::split_x` refuses split tiles in both modes; the ON branch lands with RESNET (R4, task 13.1), together with Q-NETLIST-53, which decides what ResFracture does before it cuts.
- Resolution: compat — with `magic-compat` ON `StitchEditor::split_x` on a split tile copies Magic's TiSplitX (the right-hand tile is a plain tile, the left keeps the diagonal bits); with it OFF it refuses and changes nothing. As landed in R1 the editor refuses in both modes; the ON branch lands with RESNET (R4). Approved 2026-11-22.

### Q-EXTRACT-30: Out-of-range float-to-int conversions in the extract section
- Kind: bug
- Status: approved 2026-11-23 (user decision M7-15; decided by the standing rule in round R2, 2026-11-22: undefined behaviour is fixed in both modes; confirmed with the recommended option)
- Magic behavior: the extract-section reader converts doubles to `int` with no range check: `(int)(0.5 + dval * 1000)` for a device limit (`l<=1e10`) and offset (`l=l+1e10`), then `ival + 1` / `ival - 1` in int; `step` as `(int)atof`; `sidehalo` as `(int)(d * 1000.0)`; ExtTechScale's `scalen * scalen` in int and its `int *= float` conversions. A value past the int range, or a NaN (`nan` is accepted by `sscanf("%lf")`), is undefined behaviour in C.
- Evidence: extract/ExtTech.c:2505-2520 and :2600-2620 (device parameter offsets and limits), :3365-3372 (sidehalo), :3444-3451 (step), :3973-3976 (micron conversion of step and halo), :4022-4130 (ExtTechScale); Hefesto `src/core/src/extract/tech/device_rules.cpp`, `scale_rules.cpp`, `conversion.cpp`
- Options: fix (saturate to the int range, NaN as 0, in both modes) | replicate (not possible: undefined)
- Recommendation: fix.
- Resolution: fix — in both modes every such conversion saturates to the int range and a NaN becomes 0 (`detail::saturate_int32`), and ExtTechScale squares its factors in float. Real technology files never reach these values, so no golden case changes. Spec: `extract-tech-section` "Step size and side halo parsing". Tests: `tests/unit/extract/tech/ext_style_rules_test.cpp` "a huge or NaN step and halo saturate instead of overflowing", "huge or NaN device limits and offsets saturate", `ext_style_scale_test.cpp` "rescale by a factor past the int range clamps instead of overflowing"; fuzz seed `tests/fuzz/corpus/fuzz_tech_file/extract-hostile-numbers.tech`. Approved 2026-11-23.

### Q-EXTRACT-31: The second side of a two-sided split tile is never a region seed
- Kind: bug
- Status: approved 2026-11-25 (proposed in M7 round R3 by EXTNODE, 2026-11-24; implemented per the standing rule: magic-compat ON copies Magic, OFF does the intended thing; approved with the recommended option)
- Magic behavior: DBSrPaintClient checks a split tile's single `ti_client` before it calls extNodeAreaFunc or extRegionAreaFunc for each side. Once the left side belongs to a region, the right side is skipped as a seed. When nothing else connects to the right side (for example poly on the left and ndiff on the right of one diagonal), its material is in no node and is missing from the `.ext`.
- Evidence: database/DBtiles.c:719-767 (DBSrPaintClient); extract/ExtBasic.c:5440-5448; extract/ExtRegion.c:142-209; Hefesto `src/core/src/extract/node/regions.cpp`; capture-only probe EX-P02 (`tests/data/extract/node/split_probe/EXTRACT/cases/EX-P02`, golden-capture run 45473246056, Magic 8.3.694 at fc5daa9e): a split tile with poly (sw) and ndiff (ne) and nothing else gives only `node "active_0_0#" 0 0 0 0 p`, so the ndiff side is missing from the `.ext`, as read from the code
- As implemented (round R3): with magic-compat ON the right side is skipped as Magic does; with it OFF a region starts at any side still unvisited, so every active-type tile side belongs to exactly one node (`extract-nodes-and-labels` "Node connectivity"). Tests: `tests/unit/extract/node/regions_test.cpp` "Second side of a split tile as a seed: magic-compat ON skips it" and "Second side of a split tile as a seed: magic-compat OFF gives it its node".
- Options: compat (as implemented) | replicate | fix
- Recommendation: compat.
- Resolution: compat — with `magic-compat` ON the second side of a two-sided split tile is skipped as a region seed once its first side belongs to a region, as Magic's DBSrPaintClient does (EX-P02: the ndiff side is in no node); with it OFF a region starts at any unvisited side, so every active-type tile side belongs to exactly one node. Spec: `extract-nodes-and-labels` "Node connectivity" (a scenario per mode). Tests: `tests/unit/extract/node/regions_test.cpp` (one per mode). Approved 2026-11-25.

### Q-EXTRACT-32: Per-class area and perimeter are stored only with lumped resistance on
- Kind: quirk
- Status: approved 2026-11-25 (proposed in M7 round R3 by EXTCAP, 2026-11-24; implemented per the standing rule; approved with the recommended option)
- Magic behavior: extSetResist is the only writer of `nreg_pa` and runs only with `EXT_DORESISTANCE`, so with `extract no resistance` every node record shows `0 0` per resistance class. The spec text (`extract-resistance-lumped` "Per-class area and perimeter of a node") says the values are stored even when lumped resistance is off.
- Evidence: extract/ExtBasic.c:5205, :5477 (the extSetResist calls); Hefesto `src/core/src/extract/cap/`, `src/core/src/extract/node/regions.cpp` (`NodePassOptions::resistance`)
- As implemented (round R3): with magic-compat ON the values are 0 unless lumped resistance is on; with it OFF they are always stored. Tests: `tests/unit/extract/cap/` "class values with magic-compat ON are 0 when lumped resistance is off" and "class values with magic-compat OFF are kept when lumped resistance is off".
- Options: compat (as implemented; the requirement gets one sentence and a scenario per mode) | fix (always stored)
- Recommendation: compat. Spec edit on approval: replace "These values SHALL be stored even when lumped resistance is disabled." with "With magic-compat ON, as in Magic, the values are stored only when lumped resistance is on, and are 0 otherwise. With it OFF they are always stored."
- Resolution: compat — with `magic-compat` ON per-class area and perimeter are stored only with lumped resistance on, and are 0 otherwise, as Magic; with it OFF they are always stored. Spec: `extract-resistance-lumped` "Per-class area and perimeter of a node" (the sentence of the Recommendation, a scenario per mode). Tests: `tests/unit/extract/cap/` (one per mode). Approved 2026-11-25.

### Q-EXTRACT-33: Corner coupling and fringe-halo shield pieces follow Q-EXTRACT-17's quirk
- Kind: quirk
- Status: approved 2026-11-25 (proposed in M7 round R3 by EXTCAP, 2026-11-24; implemented under Q-EXTRACT-17's compat resolution; approved with the recommended option)
- Magic behavior: extCornerOverlap and extSubtractCornerOverlap sum the shielded substrate fraction per rule, only between different nodes, and use the unreduced inside type, the quirk Q-EXTRACT-17 decided for sideoverlap. In the fringe-halo shield pieces the coupling fraction also uses the unreduced inside type (ExtCouple.c:740), where the spec text says m(a,b) of the residue.
- Evidence: extract/ExtCouple.c:740 and the corner functions (extCornerOverlap, extSubtractCornerOverlap); Q-EXTRACT-17; Hefesto `src/core/src/extract/cap/corners.cpp`, `sideoverlap.cpp`, `compat_branches.cpp`
- As implemented (round R3): Q-EXTRACT-17's branch covers both: with magic-compat ON summed per rule, between different nodes only, with the unreduced inside type; with it OFF once per tile with the residue.
- Options: compat (Q-EXTRACT-17 extended to corners and to the coupling fraction, as implemented) | a separate decision for corners
- Recommendation: compat. Spec edits on approval: `extract-capacitance` "Corner fringe coupling" gains the sentence "The shielded fractions follow Q-EXTRACT-17's resolution"; "Fringe-halo sideoverlap" says that with magic-compat ON the coupling fraction uses the unreduced inside type.
- Resolution: compat — Q-EXTRACT-17's resolution extends to the corner functions and to the coupling fraction of fringe-halo shield pieces: with `magic-compat` ON summed per rule, between different nodes only, with the unreduced inside type; with it OFF once per tile with the residue. Spec: `extract-capacitance` "Corner fringe coupling" (the sentence of the Recommendation, a scenario per mode) and "Fringe-halo sideoverlap" (the coupling fraction per mode). Approved 2026-11-25.

### Q-EXTRACT-34: A device parameter range with only a lower limit is never checked
- Kind: quirk
- Status: approved 2026-11-25 (proposed in M7 round R3 by EXTDEV, 2026-11-24; implemented per the standing rule; approved with the recommended option)
- Magic behavior: a `device` line limit such as `l>=5` sets `pl_minimum` and leaves `pl_maximum` at -1; extDevFindParamMatch skips any range with minimum > maximum, so a lower-only limit never selects a model. EX-037 shows it: both gates come out as `nlong`.
- Evidence: extract/ExtTech.c:2575-2611; extract/ExtBasic.c:2088; EX-037
- As implemented (round R3): ON skips the range as Magic does; OFF applies the lower limit (EX-037's short gate becomes `nshort`). Tests: `tests/unit/extract/device/` "EX-037 with magic-compat ON: a lower limit alone is never checked, both gates are nlong" and "EX-037 with magic-compat OFF: the lower limit applies, the short gate is nshort".
- Options: compat (as implemented) | replicate | fix
- Recommendation: compat. Spec edit on approval: `extract-devices` "Model selection by parameter ranges" gets one scenario per mode.
- Resolution: compat — with `magic-compat` ON a range with only a lower limit is skipped, as Magic (EX-037: both gates `nlong`); with it OFF the lower limit applies (the short gate is `nshort`). Spec: `extract-devices` "Model selection by parameter ranges" (a scenario per mode). Tests: `tests/unit/extract/device/` (one per mode). Approved 2026-11-25.

### Q-EXTRACT-35: Parameter range matching falls back to the last record tried
- Kind: quirk
- Status: approved 2026-11-25 (proposed in M7 round R3 by EXTDEV, 2026-11-24; implemented per the standing rule; approved with the recommended option)
- Magic behavior: when no compatible record's ranges pass, extDevFindParamMatch returns the last record it tried (`newdevptr = devptr`), not the starting one, and prints `Device parameters do not match any extraction model.`
- Evidence: extract/ExtBasic.c:2160-2196
- As implemented (round R3): ON uses the last record tried; OFF uses the starting record, as the spec text says ("the original record SHALL be used"). Test: `tests/unit/extract/device/` "Range matching returns the last record tried (ON) or the first (OFF) when none passes".
- Options: compat (as implemented) | replicate | fix
- Recommendation: compat. Spec edit on approval: `extract-devices` "Model selection by parameter ranges" states the ON behaviour, with a scenario per mode.
- Resolution: compat — when no compatible record's ranges pass, with `magic-compat` ON the last record tried is used, as Magic; with it OFF the starting record. Both print `Device parameters do not match any extraction model.` Spec: `extract-devices` "Model selection by parameter ranges" (a scenario per mode). Test: `tests/unit/extract/device/` "Range matching returns the last record tried (ON) or the first (OFF) when none passes". Approved 2026-11-25.

### Q-EXTRACT-36: Crashes, hangs and undefined behaviour in the node, capacitance and device passes
- Kind: bug
- Status: approved 2026-11-25 (fixed in both modes in M7 round R3 by the standing rule: crashes, hangs and undefined behaviour are fixed in both modes; approved with the recommended option)
- Magic behavior: (a) the split-tile diagonal `w*w+h*h` is an int product that overflows for sides past 46340 units (extNodeAreaFunc; ExtBasic.c:5574, ExtPerim.c:115); (b) ExtLabelRegions dereferences CLIENTDEFAULT for a label whose probe tile is a two-sided split tile with an unvisited left side; (c) overlap and sideoverlap areas (`o_area`, `areaTotal`, `so_clip`) overflow int past 2^31 square units; (d) `resnew` converts +inf to int when the perimeter P rounds to S in single precision (a division by zero); (e) an edge whose inside tile has no node writes through a CLIENTDEFAULT region; (f) extDevFindMatch loops forever when the current record is not in the tile type's list (two device types in one region; ExtBasic.c:3457-3475); (g) a terminal index past MAXSD writes out of bounds (ExtBasic.c:4040-4060); (h) the resistor branch reads a stale or uninitialised W when no boundary estimate exists (ExtBasic.c:2988-3060); (i) `r` with a zero device perimeter is an integer division by zero (ExtBasic.c:1985-1995).
- Evidence: as listed; Hefesto `src/core/src/extract/{node,cap,device}/`
- As implemented (round R3), both modes: (a) a 64-bit product; (b) the label is dropped; (c) 64-bit areas; (d) a saturating conversion (Q-EXTRACT-30's rule); (e) the edge subtracts nothing; (f) one full turn of the record list ends the search; (g) the tile is not a terminal; (h) W is 0; (i) r = 0 (Q-EXTRACT-15's intent).
- Tests (R3 fix pass; each fails without its fix, (e) and (g) by crashing): (a) `tests/unit/extract/cap/substrate_caps_test.cpp` "a split tile past 46340 units forms its diagonal in 64 bits" (capacitance) and `tests/unit/extract/node/regions_test.cpp` "Q-EXTRACT-36 (a): a split tile past 46340 units adds its diagonal to the class perimeter in 64 bits" (node pass); (b) `tests/unit/extract/node/labels_test.cpp` "Q-EXTRACT-36 (b): a label on the unvisited side of a two-sided split tile is dropped"; (c) `tests/unit/extract/cap/lumped_test.cpp` "a total past the int range keeps the previous value"; (d) `lumped_test.cpp` "a perimeter whose S rounds to P saturates instead of dividing by zero"; (e) `tests/unit/extract/cap/sideoverlap_test.cpp` "Q-EXTRACT-36 (e): an edge whose inside tile has no node subtracts no substrate capacitance"; (g) `tests/unit/extract/device/devices_test.cpp` "Q-EXTRACT-36 (g): a terminal past MAXSD is not a terminal". No test for (f), (h) and (i), because no input reaches them in Hefesto: (f) Magic keeps one record per tile type, so a record of a two-type line belongs to one type's list and the walk from it never comes back; Hefesto keeps one record per `device` line, listed under each of its types, and the boundary walk of a region classifies one tile type, so `find_match` is never called with a record outside the type's list (the bound stays as a guard); (h) Magic's `width` is a local that keeps an earlier device's value when extComputeEffectiveLW fails; Hefesto's L and W are fresh per device and start at 0, so there is no stale value to regress against; (i) every device region has a boundary on its own plane that counts toward its perimeter, so the perimeter is never 0 (the guard stays).
- Options: fix (as implemented) | replicate (not possible for undefined behaviour)
- Recommendation: fix. Real layouts never reach (a), (c) or (d); (b) and (e)-(i) change no captured case.
- Resolution: fix — in both modes, (a) to (i) as implemented in round R3. Tests exist for (a), (b), (c), (d), (e) and (g); round R5 (m7-round5-extract) adds (f): `tests/unit/extract/device/abut_probe_test.cpp` "Q-EXTRACT-36 (f): a record outside the tile type's list ends the match search after one turn" calls `device_detail::find_match` directly with EX-P03's pmos record and hangs without the bound. (h) and (i) have no test: (h) Hefesto's L and W are fields of a per-device record that start at 0, so there is no carried state to catch; (i) `r_value` is internal to `device_params.cpp`, and through a layout every device region has a Manhattan boundary on its own plane, so the perimeter is never 0 (the guard stays). No spec change: real layouts never reach these inputs. Approved 2026-11-25.

### Q-EXTRACT-37: The shorted-ports warning truncates the cell's port labels
- Kind: compat-neutral
- Status: withdrawn in the M7 R3 fix pass, 2026-11-24 (no decision needed; raised in round R3 by EXTCAP)
- Magic behavior: to compare the two names, extOutputNodes writes `'\0'` at `_uq` inside the label texts (ExtBasic.c:838-839), prints the warning, and then writes the `_` back (ExtBasic.c:846-847), at fc5daa9e. The cut lasts for the comparison only: no label changes, and the node's later `equiv` lines print the full names.
- Evidence: extract/ExtBasic.c:832-847
- As implemented (round R3): neither EXTNODE's record writer nor EXTCAP's warning builder changes a label; the names are compared on copies, in both modes. The output is the same as Magic's.
- Resolution: none needed. The R3 row said Magic leaves the labels cut; the review of PR #100 read the same lines as cutting the later `equiv` names. Both missed the restore at ExtBasic.c:846-847, so there is no difference to decide.

### Q-EXTRACT-38: `extract do|no unique <word>` with a word other than `notopports`
- Kind: compat
- Status: approved 2026-11-25 (proposed in M7 round R4 by EXTCMD, 2026-11-25; implemented per the standing rule; approved with the recommended option)
- Magic behavior: when the word after `unique` does not start with `notop`, CmdExtract prints `Usage:  extract do unique [notopports]` and then applies the table index of `unique` (DOUNIQUE = 11 = 0xb) as an ExtOptions bit mask, so it sets (`do`) or clears (`no`) adjust, capacitance and lumped resistance.
- Evidence: commands/CmdE.c:1451-1461
- As implemented (round R4): magic-compat ON prints the usage and replicates the side effect; OFF prints the usage and changes nothing. Tests: `tests/unit/extract/command/options_test.cpp` (one per mode).
- Options: compat (as implemented) | replicate | fix
- Recommendation: compat. Spec edit on approval: `extract-commands` "Extraction options" gains one sentence and a scenario per mode.
- Resolution: compat — with `magic-compat` ON a word other than `notop…` after `unique` prints the usage line and applies DOUNIQUE as a bit mask, as Magic; with it OFF it prints the usage line and changes nothing. Spec: `extract-commands` "Extraction options" (a sentence and a scenario per mode). Tests: `tests/unit/extract/command/options_test.cpp` (one per mode). Approved 2026-11-25.

### Q-EXTRACT-39: The line of a non-primitive `device` cell property has no newline
- Kind: compat
- Status: approved 2026-11-25 (proposed in M7 round R4 by EXTCMD, 2026-11-25; implemented per the standing rule; approved with the recommended option)
- Magic behavior: extBasic writes the verbatim tokens of a `device` property that is not `primitive` and no newline, so the `.ext` of such a cell ends without one (EX-042 `dev2.ext`), and in a hierarchical cell the next record is joined onto the same line.
- Evidence: extract/ExtBasic.c:519-528; EX-042
- As implemented (round R4): magic-compat ON writes no newline (EX-042 replays byte for byte; `CellResult::unterminated`); OFF ends the line. Tests: `tests/unit/extract/hier/extract_cell_flat_test.cpp` (one per mode).
- Options: compat (as implemented) | replicate
- Recommendation: compat. Spec edit on approval: `extract-devices` "Device cell property" (or `extract-commands` "Cell properties affecting extraction") gains the rule and a scenario per mode.
- Resolution: compat — with `magic-compat` ON the line of a non-primitive `device` cell property ends without a newline, as Magic (EX-042); with it OFF it ends with one. Spec: `extract-devices` "Device cell property" (the rule and a scenario per mode). Tests: `tests/unit/extract/hier/extract_cell_flat_test.cpp` (one per mode). Approved 2026-11-25.

### Q-EXTRACT-40: Unifying two hierarchical nodes drops one node's pass-2 adjustments
- Kind: bug
- Status: approved 2026-11-25 (proposed in M7 round R4 by EXTHIER, 2026-11-25; implemented per the standing rule; approved with the recommended option)
- Magic behavior: extHierConnectFunc2/3 and extHierSubstrate unify two Node records by moving the names and freeing one node; the freed node's `node_cap` and `node_pa` are discarded ("We don't need to update node_cap since it hasn't been computed yet"). That holds within one interaction only: when a later step square unifies two nodes that already received adjustments (extSubtreeAdjustInit), the merge record loses one node's correction.
- Evidence: extract/ExtHier.c:300-345, 740-845; extract/ExtSubtree.c:582-612
- As implemented (round R4): magic-compat ON keeps the survivor's values only; OFF adds the freed node's capacitance and per-class area and perimeter to the survivor (`HierNodeTable::unify` `sum_values`, renamed from `keep_values` in the R4 fix pass). Tests: `tests/unit/extract/hier/subcell_test.cpp` "Unifying two nodes with adjustments" (one per mode, on the table) and "Unifying two adjusted nodes in a later step square" (one per mode through `extract_subtree`: `merge "c1/b" "p2" -820` ON, `-1220` OFF; layout in `tests/data/extract/hier/unify/`). The behaviour is stated in `extract-hierarchy` (as-landed note).
- Options: compat (as implemented) | replicate | fix
- Recommendation: compat. The OFF sum is the intended adjustment (the merged node minus its pieces, summed over the interactions); ON keeps the golden output.
- Resolution: compat — with `magic-compat` ON unifying two hierarchical nodes keeps only the survivor's adjustments, as Magic; with it OFF the freed node's capacitance and per-class area and perimeter are added to the survivor. The as-landed note of `extract-hierarchy` "Merge records and pass-2 adjustments" is now the rule, with a scenario per mode (`merge "c1/b" "p2" -820` ON, `-1220` OFF). Tests: `tests/unit/extract/hier/subcell_test.cpp` (one per mode, table and `extract_subtree`). Approved 2026-11-25.

### Q-EXTRACT-41: Two device types touching in one device region make one device
- Kind: quirk
- Status: approved 2026-11-25 (proposed in M7 round R4 by EXTHIER from probe EX-P03, 2026-11-25; implemented per the standing rule; approved with the recommended option)
- Magic behavior: a device line that lists two gate types (`device mosfet cmos nfet,pfet ...`) joins them in exts_deviceConn, so an nfet touching a pfet is one device region. extOutputDevices walks only the tiles of the representative tile's gate node and selects the record from that tile's type: one device is written for the pfet (`device mosfet pmos 0 0 1 1 2 4 "VDD!" "g2" 2 0 "s2" 4 12,14 "d2" 4 12,14`) and the nfet half is lost. extDevFindMatch's endless walk (Q-EXTRACT-36 (f)) did not occur: with `thkox` over the nfet only, Magic printed `No matching device for pfet with no ID layer`, wrote no device, and `extract all` returned.
- Evidence: extract/ExtTech.c:2929; extract/ExtBasic.c:236-283; probe EX-P03 (`tests/data/extract/device/abut_probe`), golden-capture run 45553746645, `abut.ext` and `abutrev.ext`
- As implemented (round R4): ON matches the capture; OFF splits the device connectivity per type (`device_detail::device_connectivity`), so each type is its own device (`nmos` g1/s1/d1 and `pmos` g2/s2/d2). Tests: `tests/unit/extract/device/abut_probe_test.cpp` (ON pinned to the capture, OFF two devices, the ID-layer case in both modes). Scenario: `extract-devices` "Two device types touching in one device region".
- Options: compat (as implemented) | replicate
- Recommendation: compat. Losing a transistor is never intended. OFF also splits deliberate multi-type gate lines (e.g. sky130 `nfet,scnfet`) into one device per type, which is right only when the types are distinct devices: confirm, or restrict OFF to lines whose types have separate records.
- Resolution: compat, as implemented — with `magic-compat` ON a region of two touching gate types is one device, selected from the representative tile's type and node (EX-P03); with it OFF the device connectivity is split per type, so each type is its own device. OFF also splits deliberate multi-type gate lines (for example sky130's `nfet,scnfet`) into one device per type wherever two of the types touch; the user approved that as is, with no restriction to lines whose types have separate records. Spec: `extract-devices` "Device regions" (the rule) and its scenario "Two device types touching in one device region" (one THEN per mode). Tests: `tests/unit/extract/device/abut_probe_test.cpp`. Approved 2026-11-25.
- Evidence added in round R5 (m7-round5-extract): on a deliberate one-record multi-type gate line (`device mosfet nmos nfet,pfet ndiff None GND!`) with one transistor whose gate is nfet over its lower half and pfet over its upper half, ON writes one device of W 4 from the representative tile (the nfet half is lost, as in EX-P03) and OFF writes two `nmos` devices of W 4 each, with gates `g` and `@0_4` (tests "Q-EXTRACT-41: a multi-type gate line with one record is one device (magic-compat ON)" and "... splits per type (magic-compat OFF)"). This is the case the approval covered (OFF splits multi-type lines with no restriction); reported to the user again with the evidence.

### Q-EXTRACT-42: A receiver label touching two yanked tiles gets two `distance` records
- Kind: compat
- Status: proposed (raised in M7 round R5 by EXTCMD, 2026-11-26; implemented per the standing rule)
- Magic behavior: extLengthYank lists a receiver label once for each yanked tile whose area, grown by one unit at the top and right, it touches, so a label on the shared corner of two tiles is listed twice and extLength writes two identical `distance` records for it.
- Evidence: extract/ExtLength.c:448-450
- As implemented (round R5): magic-compat ON lists the label once per touched tile, as Magic; OFF lists each receiver label once. Test: `tests/unit/extract/length/length_test.cpp` "a receiver touching two yanked tiles is listed twice with magic-compat ON, once OFF".
- Options: compat (as implemented) | replicate
- Recommendation: compat. A duplicated record carries no information; ON keeps Magic's output.
- Spec: `extract-commands` "Path length extraction" states the behaviour (sentence and a scenario per mode, applied in round R5); the row ID is cited there on approval.

### Q-EXTRACT-43: `extract length` prints a stale or uninitialised `distance` for a driver label on space
- Kind: bug
- Status: proposed (fixed in both modes in M7 round R5 by EXTCMD under the standing rule: undefined behaviour)
- Magic behavior: extPathPairDistance returns without setting min and max for a driver label on space, and extLength still prints `distance` with the previous pair's values, or uninitialised ones for the first pair. The same pass sums distances in `int` (overflow on long nets) and floods by recursion (the stack can overflow on a large net).
- Evidence: extract/ExtLength.c:302-304, 639-640, 640-1040
- As implemented (round R5), both modes: a driver label on space gets no record; distances are summed in 64 bits; the flood keeps its frames on the heap. Test: `tests/unit/extract/length/length_test.cpp` "a driver label on space writes no record".
- Options: fix (as implemented) | replicate (not possible for uninitialised values)
- Recommendation: fix.
- Spec: `extract-commands` "Path length extraction" states the behaviour (sentence and scenario "Driver label on space", applied in round R5); the row ID is cited there on approval.

### Q-EXTRACT-44: `extract path` with a trailing slash names `<dir>//<cell>.ext`
- Kind: compat
- Status: proposed (raised in the M7 round R5 fix pass from a review finding, 2026-11-27; implemented per the standing rule)
- Magic behavior: ExtFileOpen joins ExtLocalPath and the cell name with `sprintf("%s/%s")`, so `extract path ext/` writes `ext//<cell>.ext`, and the `Extracting <cell> into ext//<cell>.ext:` message shows the doubled slash (capture EX-066). The file is the same; only the printed name differs.
- Evidence: extract/ExtCell.c:193, :200; golden case EX-066
- As implemented (R5 fix pass): magic-compat ON joins as Magic (`AtomicOutput` and so `extract` and `extresist`); OFF joins with one separator. Tests: `tests/unit/extract/file/atomic_output_test.cpp` "An extract path with a trailing slash is joined as Magic's sprintf joins it with magic-compat ON" / "... with one separator with magic-compat OFF".
- Options: compat (as implemented) | replicate (both modes print `//`)
- Recommendation: compat. ON keeps EX-066 byte-exact; OFF prints the path a user expects.
- Spec: `extract-commands` "Output path and file location" (sentence and a scenario per mode).
