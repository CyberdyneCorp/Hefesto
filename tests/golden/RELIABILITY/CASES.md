# Golden cases — RELIABILITY area

The RELIABILITY capabilities are `native`: Magic has no EM or IR-drop analysis. References come from three sources:
- **Magic `fc5daa9e`.** Headless Magic with `extresist all`, `simplify off` and `extout on` writes the `.res.ext` overlay. It is the reference for the analysis-network resistances, widths and via counts (capability `extresist-*`).
- **Reference ngspice.** It is built as in the SIM area. It solves the same linear networks written as R, C, V and I cards (`.op` for static runs; `.tran` with `method=gear maxord=1` for backward Euler, or `method=trap`). It is the reference for the solver.
- **Hand-computed values.** These come from the spec scenarios, for the metric and limit arithmetic.

Abbreviations:
- **min tech**: the minimal test technology used by other areas
- **sg13g2**, **gf180mcuD**, **sky130A**: open_pdks / IHP installs at the pinned versions
- **LAD**: the two-node ladder of `reliability-ir-drop` (1.8 V source, R 0–1 = 1 Ω, R 1–2 = 1 Ω, 10 mA loads at nodes 1 and 2)
- **MESH10**: a 10 × 10 mesh of 0.1 Ω resistors, a 1.2 V source at one corner, and 1 mA loads at all nodes
- **INV-CHAIN**: an sg13g2 chain of 8 inverters with a metal1/metal2 supply grid and VDD/VSS pads, simulated with a 1 GHz input for 10 ns, with device currents saved

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| REL-EMR-01 | reliability-em-rules | Bundled sg13g2 file; `emrules show` | the full output, one line per rule | Bundled rules for IHP sg13g2; Rule file authoring and inspection |
| REL-EMR-02 | reliability-em-rules | Bundled gf180mcuA–D files; `emrules show` for each | outputs; top-metal row per variant (30, 11, 9, 11 kÅ) | Bundled rules for gf180mcu |
| REL-EMR-03 | reliability-em-rules | sky130A with no project file and no adapter; `emcheck -supplies` | `REL-EMR-020`; all items `unchecked` | No bundled rules for sky130; Rule source precedence |
| REL-EMR-04 | reliability-em-rules | Invalid files: malformed JSON, value 0, non-ascending tC, `perArea` without thickness, overlapping widths, `unavailable` with rules, a type in both via and contact | codes REL-EMR-001/003/004/005/006/007/009 | Rule file validation |
| REL-EMR-05 | reliability-em-rules | Rule naming `metal9` on sg13g2 | `REL-EMR-008`; the rule still applies to the other types | Rule file validation |
| REL-EMR-06 | reliability-em-rules | Lookups on sg13g2: metal1 w = 0.20, 0.36, 0.50; metal2 w = 1.00; via6 | I_lim = 0.36, 0.36, 0.50, 2.00 and 10 mA; rule ids | Limit lookup |
| REL-EMR-07 | reliability-em-rules | gf180mcuD metal1 uni rule at 27, 85, 100, 110, 117.5, 125 and 130 °C | 2.09, 2.09, 1.3271, 1.0, 0.8154 (4 decimals), 0.67, then unchecked with `REL-EMR-010` | Temperature dependence |
| REL-EMR-08 | reliability-em-rules | Project override of `JMAXM2` = 1.5 on sg13g2 | override used; source path in `run.json` | EM rule file in the PDK store; Rule source precedence |
| REL-EMR-09 | reliability-em-rules | Verify the bundled type lists against the pinned Magic tech files (Q-RELIABILITY-4) | list of rule types missing from the tech; must be empty | Bundled rules for IHP sg13g2; Bundled rules for gf180mcu |
| REL-IR-01 | reliability-ir-drop | Network LAD fed directly to the solver | V(1) = 1.780, V(2) = 1.770; branch currents 20 and 10 mA | Static analysis; Branch currents |
| REL-IR-02 | reliability-ir-drop | LAD with R_s = 0.5 Ω | V(0) = 1.790; ΔV(2) = 40 mV | Grid solver |
| REL-IR-03 | reliability-ir-drop | LAD with 40 mA loads | `REL-IR-010` for the device at node 2 (120 mV against 90 mV) | Drop budget and violations |
| REL-IR-04 | reliability-ir-drop | MESH10 solved by Hefesto (Cholesky and forced CG) and by reference ngspice `.op` | per-node drops; maximum deviation | Grid solver; Accuracy targets |
| REL-IR-05 | reliability-ir-drop | Single node, 1 Ω to 1.8 V, C = 1 nF, 10 mA step, h = 0.1 ns, BE and trap, 50 steps | drop series; versus reference ngspice `.tran` | Dynamic analysis; Accuracy targets |
| REL-IR-06 | reliability-ir-drop | min tech: a VPWR metal1 strap of 3 abutting tiles plus an isolated metal2 island with 2 loads | analysis-network resistor count = 3 (versus `.res.ext` from Magic with `simplify off`); `REL-IR-003` for the island | Power-grid network extraction; Grid solver |
| REL-IR-07 | reliability-ir-drop | INV-CHAIN, `sim` static and dynamic | `irdrop-nodes.csv`, `irdrop-devices.csv`, `run.json` | Load current sources; Static analysis; Dynamic analysis; Reliability run storage |
| REL-IR-08 | reliability-ir-drop | INV-CHAIN, `cosim` (supply grid in the DUT netlist) | maximum drop; compared with REL-IR-07 (10 % target) | Load current sources; Accuracy targets |
| REL-IR-09 | reliability-ir-drop | Job without device currents, `-source sim` | `REL-IR-004` with the device list and the hint | Load current sources |
| REL-IR-10 | reliability-ir-drop | Window average of samples 0, 20, 20, 0 mA at 0–3 ns | 13.333 mA | Static analysis |
| REL-IR-11 | reliability-ir-drop | MESH10 run with 1, 4 and 8 threads | byte-identical CSV files | Grid solver |
| REL-IR-12 | reliability-ir-drop | Synthetic 1 000 × 1 000 grid (10⁶ nodes) on the reference desktop | solve time and peak memory | Performance and memory |
| REL-IR-13 | reliability-ir-drop | iPadOS simulator with the budget forced to 64 MiB, on a 10⁶-node grid | `REL-IR-020` before allocation; app alive | Performance and memory |
| REL-IR-14 | reliability-ir-drop | `hefesto-cli -T sg13g2 -c "load core; irdrop -net VDD=1.2:power -net VSS:ground -job j7; quit"` | exit code; printed run id; run folder | The irdrop command |
| REL-EM-01 | reliability-em-check | Waveform 0, 2, 2, −2, 0 mA at 0–4 ns | avg 0.5, absavg 1.25, rms 1.41421, peak 2, peakdir 0.875 mA; direction `bi` | Current metrics |
| REL-EM-02 | reliability-em-check | Static −3 mA | all metrics 3 mA; direction `uni` | Current metrics |
| REL-EM-03 | reliability-em-check | sg13g2 metal2 w = 1.0 µm at 1.5 and 1.7 mA avg | `pass` 0.75, `warn` 0.85 | Limit comparison |
| REL-EM-04 | reliability-em-check | sg13g2 via1 with 2 cuts, 1.0 mA avg | `fail` 1.25, rule `JMAXVIA1`; the CSV row byte-exact | Limit comparison; EM report file |
| REL-EM-05 | reliability-em-check | gf180mcuD metal2 w = 0.5 µm, `bi`, peakdir 0.70, avg 0.10 mA at 110 °C | ratios 0.9333 (`warn`) and 0.2 (`pass`); item `warn` | Limit comparison |
| REL-EM-06 | reliability-em-check | Three abutting failing metal1 items (1.1, 1.3, 1.2) | one `REL-EM-010`, ratio 1.3, union box | Violation grouping and diagnostics |
| REL-EM-07 | reliability-em-check | Contact type with via resistance 0 in min tech | item `unchecked`, reason `no-resistance` | Check scope and items |
| REL-EM-08 | reliability-em-check | Two-node signal net (C_B = 10 fF, 1 V/ns, 10 µA driver), `redistribute` | branch current 10 µA; no `REL-EM-004` | Branch current sources |
| REL-EM-09 | reliability-em-check | INV-CHAIN clock net: `redistribute` versus `cosim` | per-item avg and rms error distribution (Q-RELIABILITY-10) | Branch current sources |
| REL-EM-10 | reliability-em-check | INV-CHAIN supplies from REL-IR-07 | `em-items.csv`; `run.json` counts and `limitations` | EM report file; Modelling limitations |
| REL-EM-11 | reliability-em-check | REL-EM-10 with 1 and 8 threads | byte-identical `em-items.csv` | Determinism |
| REL-EM-12 | reliability-em-check | Sign-off checklist with the EM item, before and after an edit | `fail`/`pass`, then stale | Sign-off checklist item |
| REL-EM-13 | reliability-em-check | `hefesto-cli ... emcheck -supplies --fail-on-violation` on a failing design | exit code 1; run id | The emcheck command |
| REL-VIS-01 | reliability-visualization | `ir.drop` overlay on REL-IR-07; hover over node `VDD.n12` | readout equals the CSV value in mV | Overlay datasets; 2D heat map |
| REL-VIS-02 | reliability-visualization | One failing 0.2 µm via in a 2 mm chip, fully zoomed out | error-colored marker present (screenshot pixel probe) | 2D heat map |
| REL-VIS-03 | reliability-visualization | Save a cell with the overlay on and off | byte-identical `.mag` files | 2D heat map |
| REL-VIS-04 | reliability-visualization | 3D view of REL-EM-04; pick the via1 solid | error color; readout ratio 1.25, rule `JMAXVIA1`; browser row selected | 3D overlays |
| REL-VIS-05 | reliability-visualization | Dynamic run with the worst drop at 4.2 ns; "worst time" | cursor at 4.2 ns in the overlay and the waveform viewer | Time scrubbing |
| REL-VIS-06 | reliability-visualization | Browser: sort, filter, step next; select the top row | view box equals the row box plus a 10 % margin | Violation browser |
| REL-VIS-07 | reliability-visualization | Review note on a violation; rerun EM | note shown for the matching key | Review notes |
| REL-VIS-08 | reliability-visualization | "Show current" for R17 from a `cosim` run; "show in schematic" with no schematic | trace added with lines at the value and the limit; message `No schematic net for VDD` | Cross-probing |
| REL-VIS-09 | reliability-visualization | iPadOS simulator long press on a red via | menu items as specified | Touch and platform parity |
