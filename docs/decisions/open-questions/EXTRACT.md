# Open Questions — EXTRACT

Open questions found while specifying the circuit extractor (capabilities `extract-tech-section`, `extract-nodes-and-labels`, `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-hierarchy`, `extract-substrate`, `extract-file-format`, `extract-commands`). Magic reference: `RTimothyEdwards/magic` @ `fc5daa9e` (8.3.694).

### Q-EXTRACT-1: `defaultsideoverlap` shield direction chosen by plane index, not plane order
- Kind: quirk
- Magic behavior: the "forward" direction (which planes lie between the two planes and act as shields) is decided by comparing the plane *indices* `plane1 < plane2`, while the in-between test uses plane *order* values. If the tech declares planes in an order different from `planeorder`, the shield set can be empty or wrong.
- Evidence: extract/ExtTech.c:1800-1820
- Options: replicate | fix (compare plane order values)
- Recommendation: replicate (affects existing PDK results only when plane declaration order differs from planeorder; document).

### Q-EXTRACT-2: Diode substrate types parsed from the terminal argument
- Kind: bug
- Magic behavior: for `device diode|pdiode|ndiode`, the substrate type mask is taken from argument 4 (the same as the terminal types) and the substrate node name from argument 5 (which is the substrate-types field in the documented syntax).
- Evidence: extract/ExtTech.c:2743-2759
- Options: replicate | fix (substrate types = argument 5, name = argument 6)
- Recommendation: replicate for compatibility of existing PDKs; add a lint warning.

### Q-EXTRACT-3: Device perimeter capacitance scaled as an area
- Kind: bug
- Magic behavior: the per-perimeter capacitor coefficient (`gscap`) is multiplied by `dscale²` in micron conversion and by `(n/d)²` on grid rescale, like an area coefficient.
- Evidence: extract/ExtTech.c:3873-3874, extract/ExtTech.c:4046-4049
- Options: replicate | fix (scale by `dscale` and `n/d`)
- Recommendation: replicate (values appear in `.ext` for model-less capacitors and the `c` parameter); flag in release notes.

### Q-EXTRACT-4: Name selection when the first attached label is an attribute
- Kind: quirk
- Magic behavior: non-port labels are inserted after the head of the label list; if the head is an attribute label, each later name label is inserted at position 2, so the most recently attached name wins instead of the first.
- Evidence: extract/ExtRegion.c:236-252
- Options: replicate | fix (always keep first name label)
- Recommendation: replicate (node names must match Magic).

### Q-EXTRACT-5: Record order depends on corner-stitched tile enumeration
- Kind: ambiguity
- Magic behavior: the order of `node`, `device`, `use` and `port` records is the reverse of the discovery order of tiles/regions under Magic's tile-plane area enumeration (and subcell-plane enumeration for uses).
- Evidence: extract/ExtBasic.c:5057-5200, extract/ExtBasic.c:4976-5005, extract/ExtCell.c:617
- Options: replicate (Hefesto's tile planes use the same maximal-horizontal-strip decomposition and enumeration order) | fix (sort by reference point) and compare golden files order-insensitively
- Recommendation: replicate if `layout-geometry-model` is bit-compatible; otherwise compare records as sets per record class and document.
- Resolution: compat — record order follows the tile order bullet: Hefesto reproduces Magic's maximal-horizontal-strip decomposition and enumeration order, so `node`, `device`, `use` and `port` order is byte-identical (X-1 as amended by D-1, 2026-11-07).

### Q-EXTRACT-6: Temporary unique labels not reverted for some cells
- Kind: bug
- Magic behavior: after an extraction run with `do unique`, temporary `_uqN` labels are reverted only for cells whose isolated-substrate preparation returned a saved plane; cells without isolated substrate regions (or in styles without a default substrate type) keep the renamed labels. The guard `if (EXT_DOUNIQUE)` tests a constant and is always true.
- Evidence: extract/ExtMain.c:744-752, extract/ExtMain.c:607-612
- Options: replicate | fix (revert every temporarily renamed cell)
- Recommendation: fix — reverting is the documented intent and does not affect `.ext` content.

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
- Magic behavior: writes `version 8.3` (Magic major.minor); readers warn when it differs from their own version.
- Evidence: extract/ExtCell.c:578, utils/buildinfo.c:19, extflat/EFread.c:604-612
- Options: write `8.3` | write a Hefesto version
- Recommendation: write `8.3` while compatibility with Magic readers is required.

### Q-EXTRACT-11: Output path creation and current-directory fallback [platform]
- Kind: platform
- Magic behavior: creates the `extract path` directory with `mkdir 0755`, falls back to the current working directory when the `.mag` directory is not writable, and uses search paths to open the file.
- Evidence: extract/ExtCell.c:150-245
- Options: replicate on Linux/macOS | platform alternative on iPadOS (write into the project container; no cwd)
- Recommendation: on iPadOS map "current directory" to the project's extraction folder.

### Q-EXTRACT-12: Path-length values depend on tile decomposition
- Kind: ambiguity
- Magic behavior: `distance` min/max are computed by flooding tiles with costs between boundary midpoints, so values depend on how the plane is cut into tiles.
- Evidence: extract/ExtLength.c:770-1040
- Options: replicate tile-exact | specify behavioral tolerance
- Recommendation: behavioral; golden comparison within a tolerance until tile-plane equivalence is proven.

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
- Magic behavior: for mosfet-like classes the property's width and length are written as `W L` (normal device records write `L W`), and coordinates/dimensions are multiplied by units-per-lambda (truncated), unlike internal-unit device records.
- Evidence: extract/ExtBasic.c:430-520, extract/ExtBasic.c:560-575
- Options: replicate | fix
- Recommendation: replicate (consumers of this property expect Magic's output).

### Q-EXTRACT-17: Shield fractions accumulate across multiple sideoverlap rules
- Kind: quirk
- Magic behavior: in the fringe-halo model, the shielded substrate fraction is summed over every matching rule, so two rules targeting the same type can over-subtract substrate fringe; shield fractions use the boundary's stored inside type (a contact type is not reduced to its residue) for the multiplier.
- Evidence: extract/ExtCouple.c:1560-1600, extract/ExtCouple.c:680-700
- Options: replicate | fix (count shielding once)
- Recommendation: replicate (values must match).

### Q-EXTRACT-18: Legacy sideoverlap substrate subtraction uses whole-segment length
- Kind: quirk
- Magic behavior: `min(accounted, length)` uses the overlap of the target tile with the whole boundary segment, not with the shadow sub-segment being processed.
- Evidence: extract/ExtCouple.c:1689-1698, extract/ExtCouple.c:1790-1800
- Options: replicate | fix
- Recommendation: replicate.

### Q-EXTRACT-19: Truncations in step and halo parsing
- Kind: quirk
- Magic behavior: `step` is truncated to an integer before micron conversion; `sidehalo` is kept in milli-units, converted with rounding, then integer-divided by 1000 (truncation), so `sidehalo 2.5` in lambda gives 2.
- Evidence: extract/ExtTech.c:3362-3375, extract/ExtTech.c:3444-3452, extract/ExtTech.c:3986-3997
- Options: replicate | fix (round)
- Recommendation: replicate.

### Q-EXTRACT-20: Single-precision denominator in sidewall coupling
- Kind: quirk
- Magic behavior: `sep + offset` is computed in single precision because the offset is stored as `float`; results can differ in the last digits from a double computation.
- Evidence: extract/extractInt.h:349-351, extract/ExtCouple.c:3127
- Options: replicate | use double
- Recommendation: replicate (needed for identical `%lg` output in edge cases).

### Q-EXTRACT-21: Fractional parameter limits in lambda units
- Kind: quirk
- Magic behavior: parameter range limits are stored ×1000 and integer-divided by 1000 after conversion, so in lambda styles limits below 1 λ collapse to 0 and `l>=0.15` behaves like `l>=0`.
- Evidence: extract/ExtTech.c:2585-2600, extract/ExtTech.c:4000-4015
- Options: replicate | fix (keep fractional limits in internal units)
- Recommendation: replicate.

### Q-EXTRACT-22: Shorted-ports warning suppressed by lumped resistance
- Kind: bug
- Magic behavior: the warning `Ports "a" and "b" are electrically shorted.` is suppressed when the *lumped* resistance option is on (default), although the comment says it should be suppressed only for full R-C extraction.
- Evidence: extract/ExtBasic.c:815-838
- Options: replicate | fix (test the extresist option)
- Recommendation: fix (informative message only; no `.ext` change).

### Q-EXTRACT-23: Which node keeps its name under unique renaming
- Kind: quirk
- Magic behavior: when a duplicate is found, the node being visited (later in node-list order) keeps the name and all others are renamed, unless the earlier node holds a port label with that text; the result depends on node-list order (Q-EXTRACT-5).
- Evidence: extract/ExtUnique.c:150-190
- Options: replicate | fix (keep first by position)
- Recommendation: replicate together with Q-EXTRACT-5.

### Q-EXTRACT-24: Inconsistent concave-corner tests
- Kind: quirk
- Magic behavior: the substrate corner correction calls a corner concave when the neighbor tiles' *types* connect to the node type; the coupling corner correction requires the neighbor tiles to belong to the same *node*.
- Evidence: extract/ExtBasic.c:5430-5436, extract/ExtCouple.c:2370-2400
- Options: replicate | fix (use node identity in both)
- Recommendation: replicate.

### Q-EXTRACT-25: Halo/step overrides lost on style reload
- Kind: quirk
- Magic behavior: `extract halo` and `extract stepsize` modify the loaded style in place; reloading or changing the style (or a grid rescale that reloads) restores tech values.
- Evidence: commands/CmdE.c:1240-1300, extract/ExtTech.c:1084-1109
- Options: replicate | persist overrides as session options
- Recommendation: replicate, documenting the behavior.

### Q-EXTRACT-26: Cumulative-area sticky-label merge may create phantom nodes
- Kind: quirk
- Magic behavior: with `labelcheck`, sticky labels created by the hard-way search can produce merges to names that describe a single array element instead of a range (noted in source).
- Evidence: extract/ExtHier.c:330-345
- Options: replicate | restrict to user sticky labels
- Recommendation: replicate; add golden cases with arrays and labelcheck.
