# Open Questions — EXTRACT

Open questions found while specifying the circuit extractor (capabilities `extract-tech-section`, `extract-nodes-and-labels`, `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-hierarchy`, `extract-substrate`, `extract-file-format`, `extract-commands`). Magic reference: `RTimothyEdwards/magic` @ `fc5daa9e` (8.3.694).

### Q-EXTRACT-1: `defaultsideoverlap` shield direction chosen by plane index, not plane order
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the "forward" direction (which planes lie between the two planes and act as shields) is decided by comparing the plane *indices* `plane1 < plane2`, while the in-between test uses plane *order* values. If the tech declares planes in an order different from `planeorder`, the shield set can be empty or wrong.
- Evidence: extract/ExtTech.c:1800-1820
- Verified 2026-11-13: Confirmed at extract/ExtTech.c:1798-1822 (ExtTechSimpleSideOverlapCap). `forward = (plane1 < plane2)` compares plane indices, while the shield loop compares exts_planeOrder values. When the index order and the plane order disagree, the forward test `pnum3 < pnum2 && pnum3 > pnum1` can never hold, so the shield set is empty. The other overlap parsers (:1243-1268, :1435-1460, :1674-1690) use plane order only. PDK check: sky130A declares cap1/cap2 between the metals but orders them last, and ihp-sg13g2 does the same with mimcap. No defaultsideoverlap rule in sky130A-extract.tech, gf180mcuD-extract.tech or ihp-sg13g2.tech involves those planes, so none of the three PDKs is affected.
- Options: replicate | fix (compare plane order values) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: choose the direction by plane index, as Magic does, so the shield set can be empty or wrong when declaration order differs from planeorder. OFF: choose the direction by plane order (pnum1 < pnum2), so the shields are always the planes strictly between P1 and P2 in plane order. This is a plain bug that changes capacitance values, so the standing compat rule applies. None of the three PDKs changes in either mode; only techs whose plane declaration order differs from planeorder on a sideoverlap pair differ. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-2: Diode substrate types parsed from the terminal argument
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: for `device diode|pdiode|ndiode`, the substrate type mask is taken from argument 4 (the same as the terminal types) and the substrate node name from argument 5 (which is the substrate-types field in the documented syntax).
- Evidence: extract/ExtTech.c:2743-2759 (corrected 2026-11-13: The lines are right: extract/ExtTech.c:2743-2757. argv[4] gives both the terminal types and the substrate types, and argv[5] the substrate node name; trailing parameter words are stripped first (argc--, :2560-2620). The premise is wrong, though. Magic's own manual (doc/latexfiles/maint2.tex:2931) documents `device diode model pos_types neg_types` with no substrate field. The 'documented syntax' the entry refers to is Hefesto's spec table, not Magic's. The PDKs depend on Magic's reading: sky130A-extract.tech:1027 `device ndiode ... photo pwell,space/w error` uses argument 5 ('error') as the substrate node name. The other diode lines (sky130 :1139-1147, gf180 :927-930, ihp :5095-5097) give only OTHER. The proposed fix would parse 'error' as a type list and break sky130.)
- Options: replicate | fix (substrate types = argument 5, name = argument 6)
- Recommendation: replicate. Both modes: for diode, pdiode and ndiode, argument 4 (OTHER) is both the terminal type mask and the substrate type mask (empty substrate mask when it is `None`), and argument 5, if present, is the substrate node name. This is Magic's intended syntax, not a bug: the diode's other terminal is the well or substrate. The 'fix' would break sky130A's photodiode line, and Magic defines no other syntax. No lint warning is needed. (corrected 2026-11-13, was replicate with a lint warning)

### Q-EXTRACT-3: Device perimeter capacitance scaled as an area
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the per-perimeter capacitor coefficient (`gscap`) is multiplied by `dscale²` in micron conversion and by `(n/d)²` on grid rescale, like an area coefficient.
- Evidence: extract/ExtTech.c:3873-3874, extract/ExtTech.c:4046-4049 (corrected 2026-11-13: The behaviour is confirmed but the line numbers are off by about 10. Micron conversion is at extract/ExtTech.c:3884 (`exts_deviceSDCap *= sqfac`, sqfac = dscale²), and grid rescale at :4051-4052 (`*= sqn; /= sqd`). gscap is multiplied by the perimeter at extract/ExtBasic.c:1930 (the `c` parameter) and at :3264 (the value of a capacitor with no model), so it reaches the .ext output. PDK check: sky130A's only `device capacitor` line (sky130A-extract.tech:1163) gives a single number, which is the area cap. No PDK sets gscap, so none is affected.)
- Options: replicate | fix (scale by `dscale` and `n/d`) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: scale gscap like an area coefficient (dscale² and n²/d²), as Magic does. OFF: scale it as a per-length coefficient (dscale and n/d), like perimeter caps. This is a unit bug that changes the `c` parameter and the values of capacitors with no model, so the compat rule applies. The three PDKs are unaffected because none declares a perimeter cap on a device. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-4: Name selection when the first attached label is an attribute
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: non-port labels are inserted after the head of the label list; if the head is an attribute label, each later name label is inserted at position 2, so the most recently attached name wins instead of the first.
- Evidence: extract/ExtRegion.c:236-252 (corrected 2026-11-13: The behaviour is confirmed but the lines are wrong. Insertion is at extract/ExtRegion.c:315-331 (lreg_labels) and :489-503 (nreg_labels), not ExtRegion.c:236-252. Port labels are pushed at the head. Every other label goes after the run of port labels at the head, or becomes the head when the list is empty. So when the head is a non-port label, each later non-port label is inserted at position 2. extNodeName (extract/ExtBasic.c:993-1007) returns the first LABTYPE_NAME label. Result: with a name label at the head, the first name wins; with an attribute at the head, the most recently attached name wins.)
- Options: replicate | fix (always keep first name label) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: keep Magic's list order, so the most recently attached name label wins when the first attached label was an attribute. OFF: keep attachment order for non-port labels, so the first attached name label always wins; port precedence is unchanged. This is an ordering quirk that changes node names in .ext and netlists, so the compat rule applies. Golden cases need an attribute label attached before two name labels. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-5: Record order depends on corner-stitched tile enumeration
- Kind: ambiguity
- Magic behavior: the order of `node`, `device`, `use` and `port` records is the reverse of the discovery order of tiles/regions under Magic's tile-plane area enumeration (and subcell-plane enumeration for uses).
- Evidence: extract/ExtBasic.c:5057-5200, extract/ExtBasic.c:4976-5005, extract/ExtCell.c:617
- Options: replicate (Hefesto's tile planes use the same maximal-horizontal-strip decomposition and enumeration order) | fix (sort by reference point) and compare golden files order-insensitively
- Recommendation: replicate if `layout-geometry-model` is bit-compatible; otherwise compare records as sets per record class and document.
- Resolution: compat — record order follows the tile order bullet: Hefesto reproduces Magic's maximal-horizontal-strip decomposition and enumeration order, so `node`, `device`, `use` and `port` order is byte-identical (X-1 as amended by D-1, 2026-11-07).

### Q-EXTRACT-6: Temporary unique labels not reverted for some cells
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: after an extraction run with `do unique`, temporary `_uqN` labels are reverted only for cells whose isolated-substrate preparation returned a saved plane; cells without isolated substrate regions (or in styles without a default substrate type) keep the renamed labels. The guard `if (EXT_DOUNIQUE)` tests a constant and is always true.
- Evidence: extract/ExtMain.c:744-752, extract/ExtMain.c:607-612 (corrected 2026-11-13: The behaviour is confirmed, and it is wider than the entry says. The lines are wrong: the reverts are at extract/ExtMain.c:842-848 (ExtractOneCell) and :1108-1115 (extExtractStack), not :744-752/:607-612. `if (EXT_DOUNIQUE)` tests the constant 0x100, so it is always true. Reverts happen only for cells on the saveList, which holds only cells whose extPrepSubstrate/ExtCell returned a saved plane. In ExtractOneCell the top def is never passed to ExtRevertUniqueCell at all. The renames are permanent edits: extract/ExtCell.c:490-500 calls extUniqueCell(EXT_UNIQ_TEMP*), which does DBRemoveLabel+DBPutFontLabel (extract/ExtUnique.c:335-339), marking the cell CDMODIFIED, with undo disabled. A later save writes the `_uqN` labels to the .mag file.)
- Options: replicate | fix (revert every temporarily renamed cell)
- Recommendation: fix. Both modes: after every extraction run with `do unique`, revert the temporary labels in every cell that was renamed, the top cell of `extract`/`extract cell` included. Restore the cell's modified flag if the renaming was its only change. Leaving `_uqN` labels silently changes the saved layout, which is X-2 tier 1 data loss, and the revert does not change .ext content. Magic's ON output stays byte-identical.

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
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: writes `version 8.3` (Magic major.minor); readers warn when it differs from their own version.
- Evidence: extract/ExtCell.c:578, utils/buildinfo.c:19, extflat/EFread.c:604-612
- Verified 2026-11-13: Confirmed. The writer is at extract/ExtCell.c:576, not :578: `fprintf(f, "version %s\n", MagicVersion)`. MagicVersion is major.minor (utils/buildinfo.c:19, utils/magic_buildinfo.h:19). The reader is at extflat/EFread.c:604-611: it warns 'Cell was extracted using version %s of the extractor. It should be re-extracted.' whenever the value differs from its own.
- Options: write `8.3` | write a Hefesto version
- Recommendation: replicate. Both modes: write `version 8.3`, treating it as the .ext format version rather than Hefesto's version. When reading, warn as Magic does when the value is not 8.3. A Hefesto version string would make every Magic reader warn and would break byte-identical goldens, and Hefesto's own version belongs in the Hefesto headers, not in this field.

### Q-EXTRACT-11: Output path creation and current-directory fallback [platform]
- Kind: platform
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: creates the `extract path` directory with `mkdir 0755`, falls back to the current working directory when the `.mag` directory is not writable, and uses search paths to open the file.
- Evidence: extract/ExtCell.c:150-245
- Verified 2026-11-13: Confirmed at extract/ExtCell.c:152-250 (ExtFileOpen). With `extract path`, a missing directory is created with mkdir 0755 and the file falls back to <cellname> in the cwd when creation fails. With no path, the file goes next to cd_file with the extension replaced. Opening uses PaOpen with Path/CellLibPath. A failed write is retried as `<basename>.ext` in the cwd unless `<basename>.mag` exists there.
- Options: replicate on Linux/macOS | platform alternative on iPadOS (write into the project container; no cwd)
- Recommendation: platform. Desktop (macOS, Linux): replicate Magic exactly, including the mkdir 0755, the cwd fallback and the .mag-conflict test. iPadOS and any other sandboxed host (X-4): 'current directory' is the project folder, which is the session working directory granted to the sandbox. Relative `extract path` values resolve against it, and directories are created through the file-coordination service, with no POSIX mode. A path outside the granted roots is refused with the same 'Cannot open output file' outcome. Messages stay Magic's text.

### Q-EXTRACT-12: Path-length values depend on tile decomposition
- Kind: ambiguity
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `distance` min/max are computed by flooding tiles with costs between boundary midpoints, so values depend on how the plane is cut into tiles.
- Evidence: extract/ExtLength.c:770-1040
- Verified 2026-11-13: Confirmed at extract/ExtLength.c:770-1040. extPathFlood is a depth-first flood over the net yanked into __PATHYANK__. It marks tiles, walks the neighbours in a fixed order (top, then the other sides), and keeps epa_min/epa_max. extPathTileDist (:1015-1040) is integer Manhattan arithmetic plus the tile width or height when entry and exit are on the same side. The results therefore depend only on the tile planes and the walk order. Q-EXTRACT-5 is already resolved to reproduce Magic's maximal-horizontal-strip decomposition and enumeration order (X-1/D-1).
- Options: replicate tile-exact | specify behavioral tolerance
- Recommendation: replicate. Both modes: port the flood exactly, with the same yank, neighbour walk order, marking and integer distances. `distance` min and max are then byte-identical. The tolerance fallback is no longer needed, because Q-EXTRACT-5 already commits Hefesto to Magic's tile decomposition and enumeration order. A tolerance would only hide port errors. (corrected 2026-11-13, was behavioral, golden comparison within a tolerance)

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
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: for mosfet-like classes the property's width and length are written as `W L` (normal device records write `L W`), and coordinates/dimensions are multiplied by units-per-lambda (truncated), unlike internal-unit device records.
- Evidence: extract/ExtBasic.c:430-520, extract/ExtBasic.c:560-575
- Verified 2026-11-13: Confirmed, with nuance (extract/ExtBasic.c:353-556, not only :430-575). The property tokens are read in order: llx, lly, urx and ury each go through `int *= float exts_unitsPerLambda`, which truncates. For mosfet, asymmetric and bjt, the next two tokens are read into w and then l, and are written back as ` %d %d` w, l. So the output keeps the property's own token order, and the code merely labels the first token 'width'. Normal device records write length then width (ExtBasic.c:2891). fet, devcap and devcaprev areas use upl², and perimeters and lengths use upl.
- Options: replicate | fix
- Recommendation: replicate. Both modes: pass the property's dimension tokens through in their written order, scaled by units-per-lambda with truncation to int, exactly as Magic does. There is no corrected order to choose: Magic copies the values, and the property's author sets their meaning. Swapping them would break every consumer of existing properties. Truncation only matters with a fractional units-per-lambda, where no exact answer exists either.

### Q-EXTRACT-17: Shield fractions accumulate across multiple sideoverlap rules
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: in the fringe-halo model, the shielded substrate fraction is summed over every matching rule, so two rules targeting the same type can over-subtract substrate fringe; shield fractions use the boundary's stored inside type (a contact type is not reduced to its residue) for the multiplier.
- Evidence: extract/ExtCouple.c:1560-1600, extract/ExtCouple.c:680-700
- Verified 2026-11-13: Confirmed. In extSideOverlapHalo (extract/ExtCouple.c:1420-1640), the rule loop (about :1530-1580) adds `subfrac += sov.so_subfrac` once per matching rule, and only when rtp != rbp. extSubtractSideOverlap (:672-740) computes the multiplier from `ta = TiGetType(bp->b_inside)` (:688) without reducing a contact to its residue; extSideOverlapHalo itself does reduce it. fringehalo is on by default (ExtMain.c:58), so this is the default path.
- Options: replicate | fix (count shielding once) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: sum the shield fractions over every matching rule, accumulate only when the nodes differ, and use the unreduced inside type, as Magic does. OFF: compute the shielded fraction once per overlapped tile, whatever the rule count or node identity, using the contact's residue type for the multiplier. Over-subtracting substrate fringe is a bug that changes capacitance values, so the compat rule applies. Duplicate rules for one (ta, tb) pair are rare in the PDKs, so ON and OFF mostly agree there except for contact edges. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-18: Legacy sideoverlap substrate subtraction uses whole-segment length
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `min(accounted, length)` uses the overlap of the target tile with the whole boundary segment, not with the shadow sub-segment being processed.
- Evidence: extract/ExtCouple.c:1689-1698, extract/ExtCouple.c:1790-1800
- Verified 2026-11-13: Confirmed. The lines are off by about 10: extract/ExtCouple.c:1677-1686 computes `length` as the overlap of tp with the whole bp->b_segment, and :1780-1782 does `subcap = perimCap * MIN(areaAccountedFor, length)`. areaAccountedFor is accumulated over every matching rule from the clip to esws->area. MIN with the whole-segment length therefore caps it more loosely than the clipped length would, and only matters when several rules match. This path is used only with fringehalo off.
- Options: replicate | fix | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: cap the accounted length by tp's overlap with the whole boundary segment, as Magic does. OFF: cap it by the length of the clipped shadow sub-segment being processed, with the accounted length counted once per tile, not per rule. This quirk changes values, so the compat rule applies. It only affects the legacy model (fringehalo off) with several matching rules. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-19: Truncations in step and halo parsing
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `step` is truncated to an integer before micron conversion; `sidehalo` is kept in milli-units, converted with rounding, then integer-divided by 1000 (truncation), so `sidehalo 2.5` in lambda gives 2.
- Evidence: extract/ExtTech.c:3362-3375, extract/ExtTech.c:3444-3452, extract/ExtTech.c:3986-3997
- Verified 2026-11-13: Confirmed. extract/ExtTech.c:3362-3373 reads sidehalo as %lg, multiplies it by 1000 and casts to int. :3444-3451 sets step to (int)atof. In micron styles, :3973-3976 does halo/dscale+0.5 and step/dscale+0.5; :3990 then does halo /= 1000 (integer). So `sidehalo 2.5` in a lambda style gives 2 and `step 7.5` gives 7. PDK check: sky130A, gf180mcuD and ihp-sg13g2 all use `step 7` and `sidehalo 8` (microns). These are exact multiples after conversion (dscale 0.01 µm/λ for sky130), so all three PDKs come out the same in both modes.
- Options: replicate | fix (round) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: truncate as Magic does. OFF: parse step as a double and round both values to the nearest internal unit after unit conversion, with no integer truncation of fractional lambda values. The ×1000 is meant to keep fractional values, so truncating them is unintended, and it changes halo and step, hence coupling caps and hierarchical interaction areas. The step ≤ 0 check applies to the rounded value. Integer micron values in the PDKs give the same result in both modes. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-20: Single-precision denominator in sidewall coupling
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `sep + offset` is computed in single precision because the offset is stored as `float`; results can differ in the last digits from a double computation.
- Evidence: extract/extractInt.h:349-351, extract/ExtCouple.c:3127 (corrected 2026-11-13: The behaviour is confirmed but one line is wrong: ec_offset is declared `float` at extract/extractInt.h:476, not :349-351, which is struct extTree. extract/ExtCouple.c:3127 computes `swcap = (e->ec_cap * overlap) / (sep + e->ec_offset)`: int + float is evaluated in float, then double/float. The offset is also stored and scaled as float (ExtTech.c:3959, :4116-4117).)
- Options: replicate | use double
- Recommendation: replicate. Both modes: store the sidewall offset as a 32-bit float and form `sep + offset` in single precision, as Magic does. This is an internal number-representation choice, not a behaviour error. The difference is in the last digits, below any physical meaning, but it shows up in %lg output, so byte identity needs it. A second double path would only double the test matrix.

### Q-EXTRACT-21: Fractional parameter limits in lambda units
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: parameter range limits are stored ×1000 and integer-divided by 1000 after conversion, so in lambda styles limits below 1 λ collapse to 0 and `l>=0.15` behaves like `l>=0`.
- Evidence: extract/ExtTech.c:2585-2600, extract/ExtTech.c:4000-4015
- Verified 2026-11-13: Confirmed. Parsing at extract/ExtTech.c:2596-2611 stores ival = (int)(0.5 + V·1000), with ±1 for strict comparisons. Micron styles divide the int limits by dscale or dsq at :3897-3906 (truncating on assignment). :4003-4010 then integer-divides the limits and the offset by 1000. In lambda styles, `l>=0.15` therefore becomes minimum 0, and a strict `>` loses its +1. The three PDKs use micron units with limits that convert exactly, so none is affected.
- Options: replicate | fix (keep fractional limits in internal units) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: truncate as Magic does. OFF: keep the limits in milli-internal-units and compare 1000·L (or W, A) against them, so fractional limits and strict comparisons keep their meaning. Collapsing a limit to 0 changes model selection, which is an output value, so the compat rule applies. Offsets follow the same rule. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-22: Shorted-ports warning suppressed by lumped resistance
- Kind: bug
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the warning `Ports "a" and "b" are electrically shorted.` is suppressed when the *lumped* resistance option is on (default), although the comment says it should be suppressed only for full R-C extraction.
- Evidence: extract/ExtBasic.c:815-838
- Verified 2026-11-13: Confirmed. extract/ExtBasic.c:826-829 tests `!(ExtOptions & EXT_DORESISTANCE)`, which is lumped resistance (0x008, part of EXT_DOALL and on by default, ExtMain.c:58). The comment at :820-823 says the warning should be skipped only when extresist runs, which is EXT_DOEXTRESIST (0x200). In the default configuration the warning therefore never prints. Only console text is affected; the .ext is not.
- Options: replicate | fix (test the extresist option) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: suppress the warning whenever lumped resistance is on, as Magic does. OFF: suppress it only when full R-C extraction (extresist) runs in the same pass, as the comment intends. The .ext output is the same in both modes. The golden harness compares Magic console text (--diagnostics=magic), so the ON default must stay silent to match. (corrected 2026-11-13, was fix)

### Q-EXTRACT-23: Which node keeps its name under unique renaming
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: when a duplicate is found, the node being visited (later in node-list order) keeps the name and all others are renamed, unless the earlier node holds a port label with that text; the result depends on node-list order (Q-EXTRACT-5).
- Evidence: extract/ExtUnique.c:150-190
- Verified 2026-11-13: Confirmed at extract/ExtUnique.c:155-188. On a duplicate, if the current label is not a port and the earlier node holds a port label with the same text, extMakeUnique runs keeping the earlier node, and the hash is not marked processed. Otherwise the current node (later in node-list order) keeps the name and the text is marked processed. Renaming is at :265-345.
- Options: replicate | fix (keep first by position)
- Recommendation: replicate. Both modes: the node visited later keeps the name, except that a port on the earlier node wins over a non-port, exactly as Magic does. This is a design choice with no defined 'correct' winner, not a bug. Node-list order is already reproduced byte-identically under Q-EXTRACT-5, so the result is deterministic and matches Magic's names.

### Q-EXTRACT-24: Inconsistent concave-corner tests
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: the substrate corner correction calls a corner concave when the neighbor tiles' *types* connect to the node type; the coupling corner correction requires the neighbor tiles to belong to the same *node*.
- Evidence: extract/ExtBasic.c:5430-5436, extract/ExtCouple.c:2370-2400
- Verified 2026-11-13: Confirmed. The substrate corner correction at extract/ExtBasic.c:5434-5438 calls a corner convex when both tiles are space, and concave when both tile types are in the connect mask; it does not check the free quadrant. The coupling correction at extract/ExtCouple.c:2337-2362 requires node identity (ExtGetRegion == rbp) and also requires the free quadrant (b_outside) to be space. The two differ when the tiles connect by type but are different nodes (diagonal point contact) or the free quadrant is not space.
- Options: replicate | fix (use node identity in both) | compat (replicate with `magic-compat` ON, corrected behavior with it OFF)
- Recommendation: compat. ON: keep both tests as Magic has them. OFF: use the coupling test in both places, requiring node identity and a free quadrant that is space. The divergence is a bug that changes corner fringe values, so the compat rule applies. It only shows up at diagonal point contacts or at corners whose free quadrant holds other material. (corrected 2026-11-13, was replicate)

### Q-EXTRACT-25: Halo/step overrides lost on style reload
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: `extract halo` and `extract stepsize` modify the loaded style in place; reloading or changing the style (or a grid rescale that reloads) restores tech values.
- Evidence: commands/CmdE.c:1240-1300, extract/ExtTech.c:1084-1109
- Verified 2026-11-13: Partly confirmed. commands/CmdE.c:1241-1300 writes ExtCurStyle->exts_sideCoupleHalo and exts_stepSize in place. ExtLoadStyle (extract/ExtTech.c:1084-1109) reinitializes and reloads the style, restoring the tech values; it is reached through `extract style` (:796) and ExtCompareStyle (:299). Correction: a grid rescale does not reload the style. ExtTechScale scales the overrides along with everything else (ExtTech.c:4038-4039, called from CmdRS.c:330 and DBio.c:801).
- Options: replicate | persist overrides as session options
- Recommendation: replicate. Both modes: `extract halo` and `extract stepsize` change the loaded style. Selecting or reloading a style, or a tech load, restores the tech values. A grid rescale scales the override like any other style value. This is Magic's documented style model, not a defect, and it has no output to correct.

### Q-EXTRACT-26: Cumulative-area sticky-label merge may create phantom nodes
- Kind: quirk
- Status: open (recommendation drafted 2026-11-13)
- Magic behavior: with `labelcheck`, sticky labels created by the hard-way search can produce merges to names that describe a single array element instead of a range (noted in source).
- Evidence: extract/ExtHier.c:330-345 (corrected 2026-11-13: The lines are wrong. The author's note is at extract/ExtHier.c:521-526, inside extHierConnectFunc1 (code :528-575), not :330-345, which is array-name construction and node unification. With labelcheck on, every LABEL_STICKY label in the cumulative def that touches the tile is unified with the tile's node name. For arrays, that name describes one element, not a range. The source author is 'not certain' the code should be restricted to user sticky labels.)
- Options: replicate | restrict to user sticky labels
- Recommendation: replicate. Both modes: unify every sticky label in the cumulative def, as Magic does. Magic defines no corrected behaviour: its author was unsure of the restriction, and Hefesto has no reliable way to tell hard-search labels from user sticky labels without inventing semantics for array ranges. The phantom names are extra aliases in `merge` records that the netlisters resolve. Add golden cases with arrays and labelcheck.
