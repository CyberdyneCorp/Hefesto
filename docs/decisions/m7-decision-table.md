# M7 decision table: the 56 open questions blocking extraction and netlisting

M7 ("Extraction and netlisting", docs/roadmap.md) cannot start until every open question its specs cite has a `Resolution:` line. `python3 scripts/check_milestone_readiness.py --milestone M7` lists 56: 19 in `docs/decisions/open-questions/EXTRACT.md`, 4 in `MISC.md` (antenna check) and 33 in `NETLIST.md`. Each entry was checked against Magic 8.3.694 (fc5daa9e, `/Users/leonardo/trabalho/magic`, read-only) and, where a PDK is named, against the sky130A, gf180mcuD and ihp-sg13g2 tech files. Each entry carries a rewritten `Recommendation:` line with the full reasoning; corrected evidence is marked `(corrected 2026-11-13: ...)`, and evidence that was right gets a `Verified 2026-11-13:` line. **All 56 were approved on 2026-11-13** as drafted, with no option changed; the three closest calls below take their recommended option (a). Each entry now carries `Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)` and a `Resolution:` line, and `python3 scripts/check_milestone_readiness.py --milestone M7` reports `M7: ready`.

The standing rule for Magic bugs and quirks is compat: `magic-compat` ON copies Magic, OFF does the intended thing. Most rows follow it. Where they do not, the row says why: replicate when Magic defines no corrected behaviour, the "bug" turns out to be Magic's intended semantics, or the quirk is an internal unit or order choice that byte identity needs; fix when the quirk loses data, is undefined behaviour, or has no Magic output to match. Several compat rows also carry a tier-1 guard that applies in both modes (Q-MISC-4, Q-NETLIST-8, Q-NETLIST-29, Q-NETLIST-30); the guard only changes inputs on which Magic has undefined behaviour.

Recommendation kinds (X-2, docs/decisions/cross-cutting.md):

- **replicate**: do what Magic does, in both modes.
- **compat**: replicate with `magic-compat` ON and use the corrected behavior with it OFF (tier 2).
- **fix**: corrected behavior in both modes (tier 1, or tier 3 when there is no Magic output to match).
- **platform**: a Hefesto platform decision (X-3/X-4).
- **other**: none of the above.

## Judgement items

None remain (all 56 approved as drafted on 2026-11-13). Every item is settled by X-2, X-4, D-7 or an earlier resolution (Q-EXTRACT-5), so no row needed a product or scope decision. The three closest calls are kept below for the record; the user took the recommended option (a) for each.

1. **Q-EXTRACT-12: path-length values depend on tile decomposition.**
   The entry recommended a behavioural golden tolerance. Since Q-EXTRACT-5 already commits Hefesto to Magic's tile decomposition and enumeration order, the draft asks for a byte-exact port of the flood instead. This raises the porting bar for `extract length`; a tolerance would be cheaper but would hide port errors.
   - (a) **Recommended.** replicate tile-exact (`Compatibility: exact`)
   - (b) behavioural comparison within a tolerance (the entry's original recommendation)
2. **Q-EXTRACT-2: diode substrate types parsed from the terminal argument.**
   The entry treated this as a bug against a documented syntax, but that syntax is only in Hefesto's own spec table; Magic's manual has none, and sky130A's photodiode line uses argument 5 as the substrate node name. The draft drops the lint warning the entry proposed.
   - (a) **Recommended.** replicate Magic's syntax `OTHER [SUBNODE]`, no lint warning
   - (b) fix: substrate types = argument 5, name = argument 6 (breaks sky130A)
3. **Q-NETLIST-37: flat SPICE ignores ports for subcircuit marking.**
   Magic 8.3.694 changed this on purpose (commit 9800d98d), and its documentation still says `top auto` wraps cells with ports. The draft follows the standing rule; with magic-compat OFF, flat netlists of cells with ports gain a `.subckt` wrapper, which is the visible product change.
   - (a) **Recommended.** compat: ON as Magic (only `top on` wraps); OFF `top auto` wraps when the root has ports
   - (b) replicate in both modes (the entry's original recommendation)
   - (c) fix: mark subcircuits in flat mode as documented, in both modes

## Decision table

Every row is approved (2026-11-13); the Recommendation column is the decision recorded in the entry's `Resolution:` line. Each row's full text is the entry's `Recommendation:` line. Where the draft changes the entry's original recommendation, that line ends with `(corrected 2026-11-13, was ...)`.

### Extraction technology section

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-1 | defaultsideoverlap shield direction chosen by plane index, not plane order | **compat** | ON: direction by plane index; OFF: by plane order; no PDK affected |
| Q-EXTRACT-2 | Diode substrate types parsed from the terminal argument | **replicate** | OTHER is terminal and substrate types, argument 5 is the substrate node name (Magic's syntax; sky130 photodiode relies on it) |
| Q-EXTRACT-3 | Device perimeter capacitance (gscap) scaled as an area | **compat** | ON: gscap scaled as an area; OFF: as a per-length coefficient; no PDK sets gscap |
| Q-EXTRACT-19 | Truncations in step and halo parsing | **compat** | ON: truncate step/halo; OFF: round after unit conversion; PDKs identical |
| Q-EXTRACT-21 | Fractional parameter limits in lambda units | **compat** | ON: /1000 truncation of limits and offsets; OFF: milli-unit comparison |

### Nodes, labels and hierarchy

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-4 | Name selection when the first attached label is an attribute | **compat** | ON: Magic's list order (latest name wins after an attribute head); OFF: first attached name wins |
| Q-EXTRACT-6 | Temporary unique labels not reverted for some cells | **fix** | revert temporary `_uqN` labels in every renamed cell, top cell included (tier 1 data loss) |
| Q-EXTRACT-22 | Shorted-ports warning suppressed by lumped resistance | **compat** | ON: warning suppressed by lumped resistance; OFF: only by extresist; .ext unchanged |
| Q-EXTRACT-23 | Which node keeps its name under unique renaming | **replicate** | later node keeps the name, earlier port wins over a non-port |
| Q-EXTRACT-26 | Cumulative-area sticky-label merge may create phantom nodes | **replicate** | unify every sticky label in the cumulative def; add array/labelcheck goldens |

### Capacitance

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-17 | Shield fractions accumulate across multiple sideoverlap rules (fringe-halo model) | **compat** | ON: sum shield fractions per rule, unreduced type; OFF: once per tile, residue type |
| Q-EXTRACT-18 | Legacy sideoverlap substrate subtraction uses whole-segment length | **compat** | ON: MIN with whole-segment overlap; OFF: MIN with clipped length, counted once (legacy model only) |
| Q-EXTRACT-20 | Single-precision denominator in sidewall coupling | **replicate** | single-precision offset and `sep + offset` (byte identity of %lg) |
| Q-EXTRACT-24 | Inconsistent concave-corner tests (substrate vs coupling) | **compat** | ON: type test for substrate corners; OFF: coupling test (same node, free quadrant space) |

### Devices, file format and commands

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-EXTRACT-10 | Version string written in .ext | **replicate** | write `version 8.3` as the .ext format version; warn on read when it differs |
| Q-EXTRACT-11 | Output path creation and current-directory fallback | **platform** | desktop as Magic; sandboxed hosts: project folder is the cwd, directories via file coordination |
| Q-EXTRACT-12 | Path-length values depend on tile decomposition | **replicate** | port the flood exactly; `distance` byte-identical (Q-EXTRACT-5 fixes the tiles), no tolerance |
| Q-EXTRACT-16 | device cell property dimensions (W L order, upl scaling) | **replicate** | pass property tokens through in written order, truncating upl scaling |
| Q-EXTRACT-25 | Halo/step overrides lost on style reload | **replicate** | overrides change the loaded style, lost on style reload, scaled on grid rescale |

### Antenna check

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MISC-1 | Cumulative antenna model never reports violations | **compat** | ON: cumulative model reports nothing; OFF: accumulate over planes with order ≤ check plane |
| Q-MISC-3 | Antenna line without calc type and no default | **fix** | store 'no calculation type'; layer contributes no area (uninitialised read) |
| Q-MISC-4 | Contact cut count mixes internal and output units | **compat** | no-rule contacts give area 0 and zero pitch one cut in both modes; ON: Magic's unit mix; OFF: consistent units |
| Q-MISC-7 | Connectivity restriction uses plane numbers and one contact type | **compat** | ON: first contact on the plane, plane numbers (gf180 metal4 differs); OFF: all contacts, plane order |

### ext flattening (read and names)

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-8 | Parameter scale and offset handling | **compat** | `*S` offset is 0 in both modes; ON: offset only in the SI scale-1 branch; OFF: in every branch |
| Q-NETLIST-9 | Declared parameter order is reversed on output | **replicate** | reverse token order of declared and verbatim parameters, as Magic |
| Q-NETLIST-10 | Name preference favors the lexically greater name | **replicate** | on a full tie the lexically greater root-first name wins |
| Q-NETLIST-12 | `resist` values are offset by +0.5 mΩ | **compat** | ON: `resist` = 0.5 + rscale×R; OFF: rscale×R, still float |
| Q-NETLIST-30 | Leaf-name conversion deletes characters anywhere | **compat** | ON: delete `!`/`#` anywhere in the leaf; OFF: trailing only; leading-comma underwrite guarded in both |
| Q-NETLIST-31 | Shorted-port devices create a `dummy` node | **compat** | ON: shared `dummy` terminal 0 (tee mode joins all shorted pairs); OFF: anonymous terminal 0 per short device |

### SPICE netlisting

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-4 | Flat writer prints resistance for an unnamed c parameter | **compat** | ON: flat `c` writes the resistance; OFF: the capacitance |
| Q-NETLIST-5 | Flat-mode tee for rsubckt never writes its second half | **compat** | ON: flat tee writes only X<n>A; OFF: also X<n>B as in hierarchical mode |
| Q-NETLIST-6 | Flat merge multipliers desynchronize on 1-terminal devices | **fix** | merge multiplier and killed flag keyed by device; 1-terminal devices get M = 1 (UB in Magic) |
| Q-NETLIST-7 | Terminal area/perimeter divided by M only in scale mode | **compat** | ON: divide area/perimeter by m only in scale mode; OFF: always |
| Q-NETLIST-13 | `ext2spice run` and unknown option words become the cell name | **compat** | ON: `run` and ambiguous words become the cell name; OFF: `run` keyword, ambiguous prefix is an error |
| Q-NETLIST-19 | Hierarchical and flat SPICE formatting disagree | **compat** | compat for flat SI perimeter 1e-12 and hierarchical `%d` w/l; replicate signed thresholds and connected test per mode |
| Q-NETLIST-20 | Hierarchical output omits node capacitances without a `substrate` record | **compat** | ON: no hierarchical node lines without a `substrate` record; OFF: written as in flat mode |
| Q-NETLIST-32 | Consumed `l<n>`/`w<n>` parameters leave a stray space | **compat** | ON: consumed `l<n>`/`w<n>` leave a space; OFF: skipped |
| Q-NETLIST-34 | Flat device names from gate attributes lack a separator | **compat** | ON: `Mu1foo`; OFF: `Mu1/foo` |
| Q-NETLIST-35 | Hierarchical voltage-source counter not reset per cell | **compat** | ON: V counter reset per run only; OFF: per subcircuit |
| Q-NETLIST-37 | Flat SPICE ignores ports for subcircuit marking | **compat** | ON: flat `top auto` never wraps; OFF: wraps when the root has ports; `descend` inert in flat mode |
| Q-NETLIST-41 | `subcircuits descend auto` changes the top setting | **compat** | ON: `descend auto` sets top to auto; OFF: usage error |

### ext2sim

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-15 | ext2sim node-capacitance line lost its GND field | **compat** | ON: `C <name>0 <cap>`; OFF: `C <name> GND <cap>` (irsim) |
| Q-NETLIST-16 | Name-conversion flags leak between commands | **compat** | ON: session-wide OR-only name flags; OFF: per-run flags from each command's base |
| Q-NETLIST-17 | ext2sim merges devices of different class or type | **compat** | ON: ext2sim merge ignores class/type; OFF: also require equal class and type |
| Q-NETLIST-18 | ext2sim `-c` does not enable coupling output | **compat** | ON: flatten caps/resists by option thresholds; OFF: by the run's `-c`/`-r` |
| Q-NETLIST-48 | sim2simp is shipped as source only and is not built | **other** | omit sim2simp; `ext2sim merge` covers it |

### extresist

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-NETLIST-23 | `extresist skip` takes a single type | **fix** | `skip` takes a comma list; bad names leave masks unchanged (OOB write in Magic) |
| Q-NETLIST-24 | Gate load uses (W+W) for source/drain capacitance | **replicate** | keep `2·W·sdCap`: gscap is per edge length and a gate has two edges |
| Q-NETLIST-25 | Path-resistance heap uses integer keys | **compat** | ON: int keys and Magic's heap ties; OFF: double keys, insertion-order ties |
| Q-NETLIST-28 | `mindelay` query reports raw zeptoseconds in the standalone build | **replicate** | query returns ps, as the normative Tcl build (D-7) |
| Q-NETLIST-29 | `attr` records apply to the previous node in extresist | **compat** | ON: `attr` applies to the previous node; OFF: to the named node; orphan attr and res:min overflow guarded in both |
| Q-NETLIST-39 | `.res.ext` device records omit value and L/W for non-MOS classes | **compat** | ON: Magic's .res.ext device record; OFF: resistor/capacitor/caprev get value or L W and extractor terminals |
| Q-NETLIST-40 | FastHenry unnamed-node ids restart for every net | **compat** | ON: FastHenry N/E counters per net; OFF: per file |
| Q-NETLIST-43 | Near-device tile with a single breakpoint loses its area | **compat** | ON: single-breakpoint near-device tile adds no area; OFF: adds width × height |
| Q-NETLIST-44 | Delta-to-star conversion adds 0.5 mΩ instead of rounding | **compat** | ON: star arms + 0.5 mΩ; OFF: no offset |
| Q-NETLIST-46 | Path-resistance heap behaves differently for the first net of a session | **compat** | ON: session-wide heap state; OFF: fresh per-net queue |

## Counts

| Kind | Count | IDs |
|---|---|---|
| compat | 38 | EXTRACT-1, EXTRACT-3, EXTRACT-4, EXTRACT-17, EXTRACT-18, EXTRACT-19, EXTRACT-21, EXTRACT-22, EXTRACT-24, MISC-1, MISC-4, MISC-7, NETLIST-4, NETLIST-5, NETLIST-7, NETLIST-8, NETLIST-12, NETLIST-13, NETLIST-15, NETLIST-16, NETLIST-17, NETLIST-18, NETLIST-19, NETLIST-20, NETLIST-25, NETLIST-29, NETLIST-30, NETLIST-31, NETLIST-32, NETLIST-34, NETLIST-35, NETLIST-37, NETLIST-39, NETLIST-40, NETLIST-41, NETLIST-43, NETLIST-44, NETLIST-46 |
| replicate | 12 | EXTRACT-2, EXTRACT-10, EXTRACT-12, EXTRACT-16, EXTRACT-20, EXTRACT-23, EXTRACT-25, EXTRACT-26, NETLIST-9, NETLIST-10, NETLIST-24, NETLIST-28 |
| fix | 4 | EXTRACT-6, MISC-3, NETLIST-6, NETLIST-23 |
| platform | 1 | EXTRACT-11 |
| other | 1 | NETLIST-48 |

Total: 56. IDs are given without the `Q-` prefix.

## Notable changes to the entries' original recommendations

- **Q-EXTRACT-1**: replicate → compat.
- **Q-EXTRACT-2**: replicate with a lint warning → replicate.
- **Q-EXTRACT-3**: replicate → compat.
- **Q-EXTRACT-4**: replicate → compat.
- **Q-EXTRACT-12**: behavioral, golden comparison within a tolerance → replicate.
- **Q-EXTRACT-17**: replicate → compat.
- **Q-EXTRACT-18**: replicate → compat.
- **Q-EXTRACT-19**: replicate → compat.
- **Q-EXTRACT-21**: replicate → compat.
- **Q-EXTRACT-22**: fix → compat.
- **Q-EXTRACT-24**: replicate → compat.
- **Q-MISC-4**: replicate by default with a fixed mode; the undefined-behaviour cases are now fixed in both modes → compat.
- **Q-NETLIST-4**: fix → compat.
- **Q-NETLIST-7**: replicate, decided with PDK users → compat.
- **Q-NETLIST-8**: replicate → compat.
- **Q-NETLIST-12**: replicate → compat.
- **Q-NETLIST-13**: fix → compat.
- **Q-NETLIST-15**: fix with a compatibility flag → compat.
- **Q-NETLIST-16**: fix → compat.
- **Q-NETLIST-17**: fix → compat.
- **Q-NETLIST-18**: fix → compat.
- **Q-NETLIST-19**: replicate per mode, perimeter factor fixed behind a flag → compat.
- **Q-NETLIST-20**: replicate → compat.
- **Q-NETLIST-25**: replicate → compat.
- **Q-NETLIST-28**: fix → replicate.
- **Q-NETLIST-29**: fix → compat.
- **Q-NETLIST-30**: replicate → compat.
- **Q-NETLIST-31**: replicate → compat.
- **Q-NETLIST-32**: replicate → compat.
- **Q-NETLIST-34**: replicate → compat.
- **Q-NETLIST-35**: replicate → compat.
- **Q-NETLIST-37**: replicate → compat.
- **Q-NETLIST-39**: fix → compat.
- **Q-NETLIST-40**: fix → compat.
- **Q-NETLIST-41**: fix → compat.
- **Q-NETLIST-43**: fix → compat.
- **Q-NETLIST-44**: replicate → compat.
- **Q-NETLIST-46**: fix → compat.

## Evidence corrections

27 entries had wrong line numbers or an incomplete or wrong description; each entry's `Evidence:` line now carries the corrected lines. The ones that change the substance:

- **Q-EXTRACT-2**: the "documented syntax" is Hefesto's spec table, not Magic's; sky130A-extract.tech:1027 depends on argument 5 being the substrate node name.
- **Q-EXTRACT-6**: wider than described: the top cell of `extract`/`extract cell` is never reverted, and the `_uqN` renames are permanent edits that a later save writes to the .mag file.
- **Q-NETLIST-12**: commit fc62a63d shows the +0.5 is a leftover integer-rounding term, so it is a bug; values are floats, so the entry's "round to integer" option is wrong.
- **Q-NETLIST-23**: a list or bad name runs `TTMaskZero(&ResCopyMask[j])` with j < 0, an out-of-bounds write.
- **Q-NETLIST-24**: the premise is wrong: gscap is per edge length and a gate has two diffusion edges, so `W+W` is intended.
- **Q-NETLIST-29**: `curnode` is uninitialised before the first `node` record, and `res:min=` overflows a 20-byte stack buffer.
- **Q-NETLIST-30**: the trim flags are defined as "delete trailing"; a leading comma in a root prefix writes before the output buffer.
- **Q-NETLIST-31**: the `dummy` node is shared by every short device in a cell and reaches the netlist in resistor tee mode, where it shorts separate shorted-port pairs together; pi mode moves its capacitance to the ports.
- **Q-NETLIST-34**: the missing separator can make two flat device names collide (`Mu11foo`).
- **Q-NETLIST-35**: the counter is reset per run, so it only continues across subcircuits within one run.
- **Q-NETLIST-37**: commit 9800d98d made the change on purpose; `descend` is inert in flat mode.
- **Q-NETLIST-39**: only resistor, capacitor and caprev records are affected; the resistor record is rejected on read, losing the overlay's rewiring.
- Line numbers only, or a minor addition: Q-EXTRACT-3, Q-EXTRACT-4, Q-EXTRACT-20, Q-EXTRACT-26, Q-NETLIST-9, Q-NETLIST-10, Q-NETLIST-15, Q-NETLIST-16, Q-NETLIST-17, Q-NETLIST-18, Q-NETLIST-19, Q-NETLIST-25, Q-NETLIST-28, Q-NETLIST-32, Q-NETLIST-41.

Entries whose evidence was right carry a `Verified 2026-11-13:` line instead. Notable additions:

- **Q-MISC-4**: with no CIF style or no squares/slots rule, the cut parameters are left uninitialised, and a zero pitch divides by zero.
- **Q-MISC-7**: a live PDK case: gf180mcuD's metal4 plane declares mimcc before via4, so only mimcc is restricted at the metal4 check.
- **Q-NETLIST-6**: the misindexing reads uninitialised or out-of-bounds slots (undefined behaviour), not just a wrong M.
- **Q-NETLIST-8**: an explicit `*1` reads the uninitialised offset.
- **Q-EXTRACT-25**: a grid rescale does not reload the style; it scales the overrides.
- **Q-NETLIST-46**: after the first removal the heap stays "built", so the first net of a session is ordered differently.
- PDK checks: Q-EXTRACT-1, Q-EXTRACT-3, Q-EXTRACT-19, Q-EXTRACT-21 and Q-MISC-1 do not affect any of the three PDKs in either mode.


## Spec changes after approval

Applied to `openspec/specs/*/spec.md` after the 2026-11-13 approval ("Align the M7 specs with the approved decisions"). Each spec got the text below, and each entry its `Resolution:` line. Items are listed under the first spec that cites them; the text names any other spec it touches.

### extract-tech-section

- **Q-EXTRACT-1**: extract-tech-section (line 255): replace 'choosing the comparison direction by comparing the plane *indices* of P1 and P2 (see Q-EXTRACT-1)' with: with magic-compat ON the direction is chosen by comparing the plane indices; with it OFF, by comparing plane order. Cite the resolution.
- **Q-EXTRACT-2**: extract-tech-section, class table (line 294): change the diode/pdiode/ndiode row to `OTHER [SUBNODE]`, adding 'substrate types = OTHER (none when OTHER is `None`)', and remove the Q-EXTRACT-2 pointer in favour of the resolution.
- **Q-EXTRACT-3**: extract-tech-section, line 374 (micron conversion) and line 397 (grid rescale): state that gscap is scaled by dscale² and n²/d² with magic-compat ON, and like perimeter caps (dscale, n/d) with it OFF. Replace the 'see Q-EXTRACT-3' references with the resolution.
- **Q-EXTRACT-19**: extract-tech-section, 'Step size and side halo parsing' (line 131): put the truncation under magic-compat ON and add the OFF rounding. Replace 'See Q-EXTRACT-19' with the resolution.
- **Q-EXTRACT-21**: extract-tech-section (line 305), and the rescale text at line 397 'parameter offsets and limits': put the /1000 truncation under magic-compat ON and add the OFF milli-unit comparison. Replace the pointer with the resolution.

### extract-nodes-and-labels

- **Q-EXTRACT-4**: extract-nodes-and-labels, line 91, consequence (b): qualify the attribute-first exception as magic-compat ON only; with it OFF, the first attached name label wins. Correct the Magic refs to ExtRegion.c:315-331 and :489-503.
- **Q-EXTRACT-6**: extract-nodes-and-labels, 'Temporary unique' scenario (line 180): drop '(see Q-EXTRACT-6 for an exception)' and state that every temporarily renamed cell, the top cell included, is reverted, without leaving the cell modified. Update the Magic refs to ExtMain.c:842-848 and :1108-1115. Add a regression scenario for a cell with no isolated substrate.
- **Q-EXTRACT-22**: extract-nodes-and-labels (line 139): state the suppression condition per mode. Replace 'See Q-EXTRACT-22' with the resolution.
- **Q-EXTRACT-23**: extract-nodes-and-labels, 'Unique naming of duplicate labels' (line 172): replace 'See Q-EXTRACT-23' with the resolution, noting that it depends on the node-list order fixed by Q-EXTRACT-5.

### extract-hierarchy

- **Q-EXTRACT-26**: extract-hierarchy, 'Sticky labels across the hierarchy' (line 149): replace 'See Q-EXTRACT-26' with the resolution and fix the Magic refs to ExtHier.c:521-575.

### extract-capacitance

- **Q-EXTRACT-17**: extract-capacitance (line 158): describe the ON accumulation (per rule, only between different nodes, unreduced inside type) and the OFF single-count with residue reduction. Replace 'See Q-EXTRACT-17' with the resolution and fix the refs to ExtCouple.c:1530-1580 and :688.
- **Q-EXTRACT-18**: extract-capacitance (line 144): give the ON formula MIN(accounted, whole-segment overlap) and the OFF one MIN(accounted-once, clipped length). Replace 'See Q-EXTRACT-18' with the resolution and fix the refs to ExtCouple.c:1677-1686 and :1780-1782.
- **Q-EXTRACT-20**: extract-capacitance (line 125): state that the offset is single-precision and that sep+offset is evaluated in float, and fix the ref to extractInt.h:476. Replace 'See Q-EXTRACT-20' with the resolution.
- **Q-EXTRACT-24**: extract-capacitance, 'Corner fringe capacitance to substrate' (line 61): state that with magic-compat ON concave means connecting types, as now, and with it OFF the same node plus a free quadrant that is space. Replace 'See Q-EXTRACT-24' with the resolution.

### extract-devices

- **Q-EXTRACT-16**: extract-devices, 'Device cell property' (line 289): say that D1 and D2 are written in property order (Magic's code reads them as width then length for mosfet, asymmetric, bjt and modelled resistor, and as area then perimeter for fet and modelled capacitor/caprev), with no reordering to L W; lengths, widths and perimeters scale by upl and areas by upl², truncated. State that after scaling urx is incremented when urx ≤ llx, and ury when ury ≤ lly. Replace 'See Q-EXTRACT-16' with the resolution and fix the refs to ExtBasic.c:353-556.

### extract-file-format

- **Q-EXTRACT-10**: extract-file-format, line 89: replace 'see Q-EXTRACT-10' with a fixed `version 8.3` (the format version Hefesto targets) and fix the ref to ExtCell.c:576. ext-flattening-read: the version warning compares against 8.3.

### extract-commands

- **Q-EXTRACT-11**: extract-commands, 'Output path and file location' (line 117): add a platform clause mapping the current directory to the project folder on sandboxed hosts, saying that directory creation goes through file coordination and that out-of-sandbox paths fail as unwritable. Replace the Q-EXTRACT-11 pointer with the resolution.
- **Q-EXTRACT-12**: extract-commands, 'Path length extraction' (line 141): change 'Compatibility: behavioral (see Q-EXTRACT-12)' to 'Compatibility: exact'. Specify the neighbour walk order and the depth-first marking; that marks are kept until every start tile under lab1 is done (ExtLength.c:647-674), so only first-arrival paths reach the destination and set min/max; the `CLIENTDEFAULT` test of the contact branch (:864); and the 1-unit halo search of other planes in the non-contact case (:878-905). The refs are ExtLength.c:640-1040.
- **Q-EXTRACT-25**: extract-commands, 'Style, halo and step size' (line 105): add that overrides survive a grid rescale (scaled) and are lost on style selection or reload. Replace 'See Q-EXTRACT-25' with the resolution.

### antenna-check

- **Q-MISC-1**: antenna-check (line 180): put 'neither sum is accumulated' under magic-compat ON and add the OFF cumulative accumulation. Replace 'see Q-MISC-1' with the resolution.
- **Q-MISC-3**: antenna-check (line 33): replace 'the calculation type is left undefined (see Q-MISC-3)' with 'the layer gets no calculation type and contributes no antenna area'.
- **Q-MISC-4**: antenna-check (line 149): add the both-mode rule for contacts with no cut rule (area 0) and a zero pitch. Put the current formula under magic-compat ON and add the OFF unit-consistent formula. Replace 'see Q-MISC-4' with the resolution and cite CIFtech.c:2339-2369.
- **Q-MISC-7**: antenna-check (line 127): put 'compares plane numbers, applies to one contact type only' under magic-compat ON and add the OFF rule (all contacts on the plane, plane order). Replace 'see Q-MISC-7' with the resolution.

### ext-flattening-read

- **Q-NETLIST-8**: ext-flattening-read (line 172): replace 'The offset is then left undefined; see Q-NETLIST-8' with 'the offset is 0'. netlist-spice-devices parameter text: state that the offset is applied only in the SI, scale-1 branch with magic-compat ON, and in all branches with it OFF.
- **Q-NETLIST-9**: ext-flattening-read, Requirement 'Device parameter declarations' (spec.md:176): keep the text. Add one sentence saying the .ext `parameters` line is already in tech-file order, so output is the reverse of the tech-file declaration order. Update the Magic ref to extflat/EFbuild.c:777-852, :982-985.
- **Q-NETLIST-12**: ext-flattening-read, Requirement 'Use, port, cap, resist, attr and connect records' (spec.md:226): give the `resist` bullet both modes (ON: +0.5; OFF: rscale×R). extresist-output, Requirement 'Consumption by netlisters' (spec.md:74-78): the same, and limit the '0.0005 Ω offset' sentence to magic-compat ON. Update the Magic ref to extflat/EFread.c:626-635.
- **Q-NETLIST-31**: ext-flattening-read, Requirement 'Equivalence, merge and kill records' (spec.md:199): give rule 3 both modes: ON: every short device in the def takes the shared terminal-0 node `dummy`; OFF: each short device gets its own anonymous terminal-0 node that no name lookup can reach. netlist-spice-devices: note that in resistor tee mode the terminal-0 node is the midpoint of the two halves, and in pi mode its capacitance moves to the ports. Add a golden scenario (tee on, `short resistor`, two shorted-port pairs in one cell) per mode. Update the Magic refs to extflat/EFbuild.c:638-648, :1817-1836.

### ext-flattening-names

- **Q-NETLIST-10**: ext-flattening-names, Requirement 'Name preference order' (spec.md:55-63): no behavioural change. Drop the open-question pointer once resolved and update the Magic ref to extflat/EFname.c:510-568, :590-607.
- **Q-NETLIST-30**: ext-flattening-names, leaf-conversion requirement (spec.md:118-129): change 'These deletions apply anywhere in the leaf' to ON: anywhere; OFF: `!`/`#` only as the last character. Add a scenario with `a!b`. Add a note to the prefix rule about the leading-comma guard. Update the Magic ref to ext2spice/ext2spice.c:4307-4375.

### netlist-spice-devices

- **Q-NETLIST-4**: netlist-spice-devices (line 134): replace 'Flat mode wrongly writes the resistance for c (Q-NETLIST-4)' with: with magic-compat ON, flat mode writes the resistance for `c`; with it OFF, it writes the capacitance.
- **Q-NETLIST-5**: netlist-spice-devices (line 179): replace 'In flat mode that second line is never written (Q-NETLIST-5)' with the per-mode rule.
- **Q-NETLIST-6**: netlist-spice-devices (line 209): replace 'devices with exactly one terminal desynchronize the multiplier list (Q-NETLIST-6)' with 'devices with exactly one terminal take part in no merge and are written with M = 1' (devices with no terminals are never written).
- **Q-NETLIST-7**: netlist-spice-devices (line 87): keep the current text under magic-compat ON and add 'with it OFF, the non-scale values are also divided by m'. Replace the Q-NETLIST-7 pointer with the resolution.
- **Q-NETLIST-19**: netlist-spice-devices, parameter table (spec.md:107): give the flat `p`/`p0` SI entry both modes (ON 1e-12; OFF 1e-6). netlist-spice-hierarchy (spec.md:100-106): add 'Modeled resistor in scale mode: ON `w=%d l=%d` truncated; OFF `%g`', and keep the |C| threshold and connectivity bullets as normative per-mode behaviour. Update the Magic refs to ext2spice.c:2286-2299, :4002, :4157-4161, :4180 and ext2hier.c:567-570, :1494, :1655-1660, :1673.
- **Q-NETLIST-32**: netlist-spice-devices (spec.md:118): change 'A blanked entry yields a lone space' to ON: lone space; OFF: nothing written. Update the Magic refs to ext2spice.c:2388, :2433, :2495-2497 and ext2hier.c:379, :424.
- **Q-NETLIST-34**: netlist-spice-devices (spec.md:23): change 'preceded by the raw `/`-joined instance path without a trailing separator' to ON: no separator; OFF: path + `/`. Add one scenario per mode. Update the Magic ref to ext2spice/ext2spice.c:2876-2881.

### netlist-spice-hierarchy

- **Q-NETLIST-20**: netlist-spice-hierarchy, Requirement on section body order (spec.md:62): change item 5 to ON: 'only if a substrate record was read'; OFF: 'always, to the substrate node or `0`'. Update the Magic ref to ext2spice/ext2hier.c:2431-2441.
- **Q-NETLIST-35**: netlist-spice-hierarchy, Counters bullet (spec.md:64): change 'The voltage-source counter is not reset' to ON: not reset per section (reset per run); OFF: reset with the others. Update the Magic refs to ext2hier.c:2453-2459 and ext2spice.c:828.

### netlist-spice-options

- **Q-NETLIST-13**: netlist-spice-options, Requirement 'Command options' (spec.md:17): replace 'If the first word is missing or does not match, all words SHALL be treated as run arguments' with the two-mode rule: ON as now; OFF `run` is a keyword and an ambiguous prefix is an error. Add one scenario per mode for `ext2spice run`.
- **Q-NETLIST-37**: netlist-spice-options, Requirement 'Flat netlist structure' (spec.md:158): change item 1 to ON: 'only `top on` writes a wrapper'; OFF: '`top on`, or `top auto` when the root has port records'. Add a sentence that `descend` has no effect in flat mode. Add one scenario per mode. Update the Magic refs to ext2spice.c:888, :1123-1145 and extflat/EFread.c:469-472.
- **Q-NETLIST-41**: netlist-spice-options, option table (spec.md:31): change the `subcircuits descend` row to ON: 'Giving auto sets top to auto'; OFF: 'auto is a usage error'. Update the Magic ref to ext2spice/ext2spice.c:629-646.

### netlist-sim

- **Q-NETLIST-15**: netlist-sim, Requirement 'File structure and order' (spec.md:57-58): give `nodeline` both forms (ON `name "0 "`; OFF `name " GND "`). Requirement 'Parasitic and node records' (spec.md:139): state both modes. Scenario 'Node capacitance line': add the OFF result `C out GND 5.0`. Update the Magic ref to ext2sim/ext2sim.c:1628-1680.
- **Q-NETLIST-16**: netlist-sim, Requirement 'Name conversion state' (spec.md:159-167): change 'SHALL reproduce this session-state dependence by default' to an ON/OFF statement and add an OFF scenario (after ext2spice, `vdd!` stays `vdd!`). ext-flattening-names: the same per-run rule for ext2spice `-t`. Update the Magic refs to ext2spice.c:1071-1075 and ext2sim.c:495.
- **Q-NETLIST-17**: netlist-sim, Requirement 'File structure and order' (spec.md:64): change 'Merging SHALL ignore device class and type' to ON: ignore; OFF: also require equal class and type. Update the Magic ref to ext2sim/ext2sim.c:1770-1858.
- **Q-NETLIST-18**: netlist-sim, Requirement 'File structure and order' (spec.md:51): change item 2 to ON: 'if cthresh was finite when the options were set'; OFF: 'if the run's cap threshold is finite'. Apply the same to resistor flattening. Update the Magic ref to ext2sim/ext2sim.c:675-677.
- **Q-NETLIST-48**: netlist-sim, Requirement 'Deprecated sim2simp filter' (spec.md:171-): no normative change. Mark the requirement as informative or remove it, and point to the `merge` option. Update the Magic ref to ext2sim/Makefile.in:20.

### extresist-command

- **Q-NETLIST-23**: extresist-command, option table and note (spec.md:16-36): change `skip <type>` to `skip <type>[,<type>…]` and remove the sentence 'skip accepts only a single type'. Add 'An unknown or ambiguous type reports an error and leaves the masks unchanged'. Update the Magic ref to resis/ResRex.c:572-588.
- **Q-NETLIST-28**: extresist-command, option table (spec.md:16): make `mindelay [v]` 'Query returns the value in ps (stored ×1e9 zs)'. Drop the informative note about the non-Tcl build, or keep it marked as informative. Update the Magic ref to resis/ResRex.c:371-390.
- **Q-NETLIST-29**: extresist-command, Requirement 'Input model' (spec.md:87): the `attr` bullet gets both modes. Add 'An attr before any node record is ignored with a warning; res:min values of any length are accepted.' Update the Magic ref to resis/ResReadExt.c:139, :204-210, :1000-1060.

### extresist-network

- **Q-NETLIST-24**: extresist-network, Requirement 'Elmore delay and pruning' (spec.md:541): write the term as `2·W·sdCap`, add 'sdCap is the per-length gate–diffusion capacitance (gscap)', and drop the Q-NETLIST-24 pointer. Update the Magic ref to resis/ResSimple.c:798-802.
- **Q-NETLIST-25**: extresist-network, path-resistance requirement (spec.md:505-518): mark the int truncation and the heap algorithm as magic-compat ON. Add OFF: float keys and node resistances, ties by insertion order. Update the Magic ref to resis/ResSimple.c:1083-1210.
- **Q-NETLIST-43**: extresist-network, near-device cutting pseudo-code (spec.md:330) and the scenario at :368: ON: discard; OFF: add the tile area to the breakpoint's node, then discard. Add an OFF scenario.
- **Q-NETLIST-44**: extresist-network, triangle(N) (spec.md:459-462): mark `+ 0.5` as magic-compat ON only and give the OFF formulas without it. Update the Magic ref to resis/ResMerge.c:630-641, :765-776.
- **Q-NETLIST-46**: extresist-network, heap 'Persistence' bullet (spec.md:516): mark it as magic-compat ON. Add OFF: per-net queue with an insertion-order tie-break. Add to the golden-harness notes that NL-RESNET captures run in a fresh session.

### extresist-output

- **Q-NETLIST-39**: extresist-output, device-record requirement (spec.md:37): change 'absent for other classes, which therefore carry no value or L/W fields' to ON: absent; OFF: resistor/capacitor/caprev get value or L W and their extractor terminal layout. Add a scenario for a resistor overlay in each mode. Update the Magic refs to resis/ResPrint.c:109-185 and extflat/EFbuild.c:1082-1110, :1160-1167.
- **Q-NETLIST-40**: extresist-output, FastHenry requirement (spec.md:104): change 'Unnamed nodes get ids from a per-net counter starting at 1' to ON: per net; OFF: per file. Apply the same to segment numbering. Update the Magic ref to resis/ResRex.c:1077, :1396.
