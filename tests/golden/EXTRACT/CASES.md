# Golden Cases — EXTRACT

Test cases to be produced later by running Magic 8.3.694 (`fc5daa9e`) headless (`magic -dnull -noconsole`) and capturing the `.ext` output. Unless stated, use a minimal test technology `exttest.tech` (planes `well, active, metal1, metal2, metal3, mimcap`; types `pwell, nwell, dnwell, ndiff, pdiff, psd, nfet, pfet, poly, rpoly, metal1, metal2, metal3, mimcap, m1c(poly/metal1 contact), via(metal1/metal2)`; `planeorder well 0 active 1 metal1 2 metal2 3 metal3 4 mimcap 5`; `cscale 1`, `rscale 1`, `lambda 1`, 1:1 grid) with the rule lines named in each case. Compare `.ext` files per Q-EXTRACT-5/Q-EXTRACT-7: node/device/use records as sets within their class, `cap` records as unordered pairs with summed values, `merge` records as name sets with values.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| EX-001 | extract-file-format | empty style; one metal1 rect; `extract all` | full `.ext` header lines | Header records; Lexical conventions |
| EX-002 | extract-capacitance | `areacap metal1 30`, `perimc metal1 space 20`; metal1 (0,0)-(10,4) | node record C = 1760; repeat with `cscale 1000` → 1.76 | Area capacitance to substrate; Perimeter capacitance |
| EX-003 | extract-capacitance | same rules; metal1 triangle in 4×4 split tile | node C = 500 | Perimeter capacitance (diagonal) |
| EX-004 | extract-capacitance | `overlap metal2 metal1 20`, `areacap metal2 10`; metal2 over metal1 10×10, separate labels | node C of metal2, `cap` value 2000 | Overlap coupling with shielding |
| EX-005 | extract-capacitance | `overlap metal3 metal1 8 metal2`; metal3/metal1 10×10, metal2 10×4 between | `cap` metal3–metal1 = 480; metal3 node C | Overlap coupling with shielding |
| EX-006 | extract-capacitance | `sidewall metal1 space space metal1 40`, `sidehalo 4`, `extract no fringehalo`; two parallel wires length 10 sep 2 | `cap` = 200 | Sidewall coupling value; Sidewall halving |
| EX-007 | extract-capacitance | EX-006 with offset `0.5` | `cap` = 160 | Sidewall coupling value (offset, float) |
| EX-008 | extract-capacitance | three parallel wires spacing 2, halo 10 | no A–C cap | Nearest-neighbor shadowing |
| EX-009 | extract-capacitance | `sideoverlap metal2 space metal1 6`, `perimc metal2 space 9`, legacy model; metal2 10×10 over larger metal1 | `cap` 240; metal2 node C reduced by 360 | Legacy sideoverlap |
| EX-010 | extract-capacitance | EX-009 with `overlap metal2 metal1 20`, fringehalo on, `sidehalo 10` | `cap` ≈ 202.56 + overlap; node C | Fringe-halo sideoverlap |
| EX-011 | extract-capacitance | metal3 edge over a metal1 plate with `sideoverlap metal3 space metal1 6 metal2` and a metal2 shield covering half of the fringe region | cap and node C | Fringe-halo sideoverlap shielding |
| EX-012 | extract-capacitance | EX-010 with in-plane metal2 neighbor at distance 3 | node C reduction (blocked fringe) | Nearest-neighbor shadowing (substrate removal) |
| EX-013 | extract-capacitance | `extract do corners` / `extract do corners 0.5`; lone metal2 rect; L-shaped metal2 (one concave corner) over metal1 plate | node C and cap deltas vs. no corners | Corner fringe capacitance; Corner fringe coupling |
| EX-014 | extract-capacitance | `extract no capacitance`; EX-004 layout | node C = 0, cap present | Capacitance option gating |
| EX-015 | extract-capacitance | `extract no coupling`; EX-004 layout | no `cap` records | Capacitance option gating |
| EX-016 | extract-capacitance | `defaultareacap metal1 metal1 well 30` with `substrate *psd,space,pwell well`, metal1 over pwell tied to substrate | no cap to substrate node; node C | Coupling to the substrate node |
| EX-017 | extract-resistance-lumped | `resist metal1 80`, `rscale 1`; metal1 10×2 and 4×4 | R = 400 and 80; with `rscale 1000` R = 0 | Lumped resistance formula; Resistance output rounding |
| EX-018 | extract-resistance-lumped | two classes poly/metal1 with m1c contact node | per-class area/perim pairs | Per-class area and perimeter |
| EX-019 | extract-nodes-and-labels | unlabeled nodes at negative coordinates; split tile right side | generated names `metal1_n10_n6#`, `...r#` | Node reference point and generated name |
| EX-020 | extract-nodes-and-labels | node with labels in order int1, A(port 1), B(port 2) | name B, port records, equiv lines | Node name selection; Port records; Alias output |
| EX-021 | extract-nodes-and-labels | node with first label `x@` then `a`, `b` | name `b`, attr record | Name selection quirk (Q-EXTRACT-4); Node and attribute records |
| EX-022 | extract-nodes-and-labels | label on boundary of two nodes; floating label over space | attachment target; sticky node record | Label attachment |
| EX-023 | extract-nodes-and-labels | `extract no aliases` vs `do aliases`, node with labels a, b | equiv presence | Alias output |
| EX-024 | extract-nodes-and-labels | two unconnected `net1` labels; `extract do unique`; then `extract unique`; then `extract unique #` with `vdd!` | `.ext` names `_uq0`; layout labels after run | Unique naming of duplicate labels |
| EX-025 | extract-nodes-and-labels | pwell abutting psd with well–psd connect | single node | Node connectivity (cross-plane halo) |
| EX-026 | extract-devices | `device mosfet nfet nfet ndiff None GND!`; 2×4 nfet with labeled s/d/g | exact device line of the spec scenario | Terminal discovery; MOSFET-family length and width; Terminal output |
| EX-027 | extract-devices | asymmetric mosfet `... ndiff,ndc ndiffh pwell VSUB` with drain on left | terminal order not sorted | Device declaration parsing; Terminal output and ordering |
| EX-028 | extract-devices | annular (ring) nfet, inner 4×4, ring width 2 | L=2, W=24 | Effective length and width |
| EX-029 | extract-devices | bent (L-shaped) gate with different lengths | L, W and "multiple lengths" message | Effective length and width |
| EX-030 | extract-devices | MOS capacitor (source and drain tied) | L, W | Boundary lists (split) |
| EX-031 | extract-devices | `resist rpoly 2000`, `device resistor None rpoly poly`, 10×2 resistor | device line with value 10 | Resistor dimensions and value |
| EX-032 | extract-devices | serpentine rpoly resistor with model name; multi-tile | L, W | Resistor dimensions (multi-tile, side lists) |
| EX-033 | extract-devices | `device capacitor None mimcap metal2 10 2`; 10×10 mim over metal2 12×12 | value 0.6, terminal `144,48` | Capacitor dimensions and value; Terminals off the device plane |
| EX-034 | extract-devices | modeled capacitor non-rectangular (L-shape) | "W area/W" output | Capacitor dimensions and value |
| EX-035 | extract-devices | `device diode dn ndiff pwell a=area p=pj` | params a, p; substrate field (Q-EXTRACT-2) | Diode records; Device declaration parsing |
| EX-036 | extract-devices | msubcircuit with `l=l w=w*1e6 c=cg r=rr` and devresist area/perimeter | `parameters` record and device params | Parameters records; Device parameter values |
| EX-037 | extract-devices | two records with `l<5` / `l>=5` ranges; gates of length 2 and 8 | chosen model per gate | Model selection by parameter ranges |
| EX-038 | extract-devices | identifier layer variant `+thkox`; substrate-layer variants | chosen models; error when no match | Device record selection |
| EX-039 | extract-devices | nfet with diffusion on one side only; `extract warn no fets` | copied terminal, feedback count | Missing terminals and rejected devices |
| EX-040 | extract-devices | two fingers sharing diffusion | terminal area/perim halved | Terminal area, perimeter and sharing |
| EX-041 | extract-devices | `$` and `^` attribute labels, `mult=2^` | attribute output and param | Terminal output; Device parameter values |
| EX-042 | extract-devices | cell property `device` = `primitive m=1`; and `device mosfet nmod 0 0 2 4 4 2 extra` | primitive/property device lines | Device cell property |
| EX-043 | extract-devices | mimcap duplicated in child at same location | parent has no device | Subcell device priority |
| EX-044 | extract-devices | legacy `fet` line | `fet` record | Device record header and substrate field |
| EX-045 | extract-substrate | `substrate *psd,space,pwell well $SUB -dnwell`, nfet in dnwell island and outside | node list, device substrate fields | Isolated substrate regions; Device substrate terminals |
| EX-046 | extract-substrate | metal-only cell with `substrate space well VSUB` | `substrate "VSUB" 0 0 -1073741817 ...` | Global substrate node discovery; Substrate record |
| EX-047 | extract-substrate | top with two standard-cell instances (1-D array of 3) | `merge "x[1]/VSUB" ...` names | Subcell substrate connections |
| EX-048 | extract-substrate | `LEFview` cell | `abstract`, no `substrate` record | Substrate record; Cell properties |
| EX-049 | extract-hierarchy | parent metal1 over identical child metal1, labels b / c1/a | merge line values | Merge records and pass-2 adjustments; Name unification order |
| EX-050 | extract-hierarchy | parent metal2 crossing child metal1 (different nets) | hierarchical `cap` | Hierarchical coupling records |
| EX-051 | extract-hierarchy | parent poly overlapping child metal1 on shared plane, no connect | feedback error | Interaction processing and connections |
| EX-052 | extract-hierarchy | unlabeled child node touched by parent | `I1/metal1_10_4#` name | Hard-way node names |
| EX-053 | extract-hierarchy | 1×4 array abutting elements; 2-D 2×2 array with diagonal overlap | array merge names with ranges | Array interactions |
| EX-054 | extract-hierarchy | `extract do labelcheck` sticky parent label over child | merge VDD | Sticky labels across the hierarchy |
| EX-055 | extract-hierarchy | large cell spanning several step squares (`step 20`) with coupling across square borders | number and sum of `cap` records | Interaction halo and step squares |
| EX-056 | extract-hierarchy | cell tree with an empty (label-only) child | no `.ext` / `use` for empty child | Bottom-up extraction order |
| EX-057 | extract-tech-section | style variants `(),(si)`; `variants` conditional lines | values per style | Extraction styles and variants |
| EX-058 | extract-tech-section | `units microns` with known cifoutput scale; areacap, perimc, sidewall offset, sidehalo, step | `scale` line, extracted values | Micron conversion |
| EX-059 | extract-tech-section | `scalegrid 1 2` with lambda style | `scale` line and values | Scaling to internal units |
| EX-060 | extract-tech-section | tech `format 33` vs `format 35`; `extract do` listing after load and after style change | fringehalo setting | Fringe model default chosen from tech format |
| EX-061 | extract-tech-section | missing planeorder lines; `noplaneordering` | warnings, default orders | Plane ordering |
| EX-062 | extract-tech-section | malformed lines (too many args, `cscale abc`, `resist m1 1.5`) | error messages | Extract section line grammar; Resistance classes |
| EX-063 | extract-tech-section | `defaultperimeter`/`defaultsideoverlap` with substrate default type | resulting caps on a test layout | Default capacitance shorthands |
| EX-064 | extract-commands | `extract` twice without edits; edit child; run again | which files rewritten (mtime) | Incremental extraction |
| EX-065 | extract-commands | `extract cell myinv` on selected instance | `myinv.ext` exists, no child files | Extract all, cell, parents and showparents |
| EX-066 | extract-commands | `extract path ext/` with missing directory | file location | Output path and file location |
| EX-067 | extract-commands | `extract do` with no args at startup | option listing | Option defaults; Extraction options |
| EX-068 | extract-commands | `extract length driver d`, `receiver r`, straight wire | `distance` record | Path length extraction |
| EX-069 | extract-commands | `extract halo 8` then extract; `extract style` change then extract | halo used | Style, halo and step size |
| EX-070 | extract-file-format | rotated/mirrored instances and arrays | `use` records | Use records; Record grammar |
