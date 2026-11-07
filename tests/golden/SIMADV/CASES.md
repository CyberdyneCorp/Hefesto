# Golden cases — SIMADV area

The SIMADV capabilities are `native`: they have no Magic counterpart. References come from:
- **Reference ngspice.** A stock ngspice executable built from the same pinned source as Hefesto's engine (`7aa47164`), with the same portable-seed patch (capability `simulation-monte-carlo`, Requirement "Portable random numbers"; Q-SIMADV-6), run in batch mode. Seeds are set with `setseed <n>` in a `.control` block before the deck is sourced.
- **Computed values.** Seeds, DOE rows, statistics and costs are computed by an independent script (Python, binary64) from the formulas in the specs.
- **Hefesto on several platforms.** Linux x86_64 is the reference platform. Results from other platforms are compared with the tolerances of capability `simulation-engine-embedding`, Requirement "Cross-platform result consistency".

Comparisons ignore raw-file `Date:` and `Command:` lines (Q-SIM-17), run-set timestamps and job ids.

Abbreviations:
- **sky130A**, **gf180mcuD**, **sg13g2**: open_pdks / IHP-Open-PDK installs at the pinned versions (Q-SIMADV-2, Q-SIMADV-4, Q-SIMADV-5)
- **INV130**: sky130A inverter testbench: `sky130_fd_pr__nfet_01v8` W=0.65 L=0.15 and `sky130_fd_pr__pfet_01v8` W=1 L=0.15, supply `Vdd` 1.8 V marked `supply`, input pulse 0→1.8 V, 100 ps edges, 20 fF load, `.tran 1p 4n`; measurement `tpd` = `delay` 50 %→50 %, rise then fall
- **DIFF130**: sky130A 5-transistor OTA, NMOS input pair W=4 L=0.5, `.op` plus `.ac dec 20 1 1g`; measurements `vos` (input-referred offset via `op`), `dcgain`, `ugbw`, `phase_margin`
- **RC1**, **DIV**: as in `tests/golden/SIM/CASES.md`

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| SIMADV-SW-01 | simulation-sweeps | DIV with `R2={r2}`; `sim sweep add param:r2 lin 1k 3k -points 5; sim runset run -wait` | 5 point decks; `v(out)` per point (expected 1.8·r2/(1k+r2): 0.9, 1.08, 1.2, 1.2857, 1.35) | Run sets; Sweep point generation; Point decks |
| SIMADV-SW-02 | simulation-sweeps | level generation only: `lin 0.9 1.1 -step 0.05`, `dec 1k 1meg 2`, `oct 1 8 1`, `dec 1meg 1k 1` (downwards), `list 3 1 3` | level count and `%.12g` text of each level | Sweep point generation |
| SIMADV-SW-03 | simulation-sweeps | INV130; sweeps `temp list -40 27 125` (outer), `supply lin 0.9 1.1 -points 3` (inner) | point index → (temp, supply) table; `points.csv`; `tpd` per point vs reference ngspice | Run sets; Point decks; Sweep result views |
| SIMADV-SW-04 | simulation-sweeps | schematic deck with `.param wn=1 ln=0.15`; sweep `param:wn`; second deck with `vdd` defined twice at top level | point deck diff (title + one token); error text for the duplicate | Point decks |
| SIMADV-SW-05 | simulation-sweeps | DOE `oat` with `param:wn:1:3` and `temp:-40:120`; DOE `lhs -points 10 -seed 7` with 2 factors on [0,1]; DOE `random -points 5 -seed 1` | rows, compared bit-exactly with the reference script; stratum check for LHS | Design of experiments |
| SIMADV-SW-06 | simulation-sweeps | 12-point INV130 sweep with workers 1 and 4 (Linux, 8 cores) | bit comparison of every point's vectors and `points.csv` | Run set execution |
| SIMADV-SW-07 | simulation-sweeps | 40-point sweep on iPad, cancelled after 17 points, then resumed | which points ran in each session; final `points.csv` vs uninterrupted run | Run set execution |
| SIMADV-SW-08 | simulation-sweeps | 500-point sweep, default `keep`, specification failing at 2 known points | files present per point folder | Run set storage |
| SIMADV-SW-09 | simulation-sweeps | 3 sweep dimensions of 100 levels; `source:Vin` sweep on a pulse source | error texts; no jobs created | Run sets; Sweep variables |
| SIMADV-SW-10 | simulation-sweeps | scripts from the "Nested sweep from a script" and "Unknown factor syntax" scenarios, headless | return values; error text; `testbench.json` unchanged on error | Sweep command options |
| SIMADV-CO-01 | simulation-corners | sky130A bundled descriptor: compare every corner section word with `.lib` lines of the pinned `sky130.lib.spice` | list of words; import error for a user corner `sf_lh` | Corner definitions in the PDK descriptor; Bundled corner tables |
| SIMADV-CO-02 | simulation-corners | same check for gf180mcuD (`sm141064.ngspice`) and sg13g2 (`corner*.lib`) | missing or extra sections | Bundled corner tables (Q-SIMADV-4, Q-SIMADV-5) |
| SIMADV-CO-03 | simulation-corners | INV130 with corner set `pvt` (45 corners) | level order (first 3 names); deck of `ss_hh_125_1v62` model and temp lines; `tpd` per corner vs reference ngspice | Bundled corner tables; Corner dimension |
| SIMADV-CO-04 | simulation-corners | sg13g2 inverter with `rppd` load, corner `ss` | emitted `.lib` lines (no `cornerHBT.lib`) | Bundled corner tables |
| SIMADV-CO-05 | simulation-corners | INV130, specification `tpd max 120p` with override `ss_* → max 180p`, 5 process corners at 27 °C | verdict per corner; matrix summary row; matrix CSV | Pass and fail per corner; Corner matrix |
| SIMADV-CO-06 | simulation-corners | user corner set `signoff` with 3 tuples; duplicate tuple variant | corner names; duplicate error | User-defined corners and corner sets |
| SIMADV-CO-07 | simulation-corners | "re-run failed" with `-regenerate` after a schematic edit | new run set size, `rerunOf`, base deck hash changed | Run all and re-run failed corners |
| SIMADV-CO-08 | simulation-corners | `sim corners define`/`list`/`matrix` scripts on sky130A and sg13g2 | return values; error texts | Corner command options |
| SIMADV-MC-01 | simulation-monte-carlo | seed derivation only: B = 1, i = 0…2 and 99; B = 42, i = 0…1; B = 2147483647, i = 99999 | seeds (expected 875784623, 2120735756, 1645304719, 1888265916; 1749773200, 112509509) | Sample seeds |
| SIMADV-MC-02 | simulation-monte-carlo | patched generator: `setseed 1`; dump of the eight state words through a test hook; first 10 `agauss(0,1,1)` draws | state words (expected 29963 3443 21757 5511 10925 11052 26345 26017); draws on Linux, macOS, iPad (≤ 4 ulp) | Portable random numbers |
| SIMADV-MC-03 | simulation-monte-carlo | deck `.param p='agauss(0,1,1)'`, `R1 a 0 {1k*(1+0.1*p)}`, `V1 a 0 1`, `.op`, run in two fresh worker processes with seed 1 and once with stock (unpatched, pid-seeded) behaviour | `i(v1)` per run: identical for Hefesto workers; differs for pid seeding | Sample seeds; Reproducibility (Q-SIMADV-7) |
| SIMADV-MC-04 | simulation-monte-carlo | INV130, mode `mismatch`, corner `tt`, N = 200, B = 1, workers 1 and 4 and iPad | per-sample `tpd`; bit equality between worker counts; iPad within tolerance | Monte Carlo dimension; Parallel and sequential execution; Reproducibility |
| SIMADV-MC-05 | simulation-monte-carlo | DIFF130, mode `mismatch` vs mode `process` vs mode `both`, N = 500 | `vos` mean and sigma per mode; `both` shows instance spread (verifies Q-SIMADV-3 override) | Monte Carlo modes (Q-SIMADV-3) |
| SIMADV-MC-06 | simulation-monte-carlo | gf180mcuD inverter, mode `both`, N = 200 | deck model and `.param` lines; `tpd` spread vs reference ngspice | Monte Carlo modes (Q-SIMADV-4) |
| SIMADV-MC-07 | simulation-monte-carlo | sg13g2 `mc_lv_nmos_cs_loop` circuit as a single-op testbench, mode `mismatch`, N = 100; DUT variant with `mm_ok=0` | `Vgs` spread vs the IHP `.control` loop under reference ngspice with the same seeds; warning list for `mm_ok=0` | Monte Carlo modes (Q-SIMADV-5) |
| SIMADV-MC-08 | simulation-monte-carlo | INV130 N = 100 then N = 200 with B = 5 | samples 0–99 identical in both runs | Monte Carlo dimension |
| SIMADV-MC-09 | simulation-monte-carlo | schematic deck with `.option seed=12` and `.control` `setseed 3`; deck setting `stat_param_as_func` | warnings and line numbers; rejection text | Sample seeds |
| SIMADV-MC-10 | simulation-monte-carlo | macOS, 500 samples, 4 workers, test hook kills the worker at sample 233 | states of all samples; diagnostic of 233 | Parallel and sequential execution; Failed samples and early stopping |
| SIMADV-MC-11 | simulation-monte-carlo | synthetic run where samples 3, 40, 120, 200, 260 fail, N = 1000, `-stop 0.01` | `stoppedEarly` (expected 500); checkpoint half-widths | Failed samples and early stopping |
| SIMADV-MC-12 | simulation-monte-carlo | user parameter `rload gauss 10k 0.05 3` (process) on RC1-style deck | sample and nominal deck lines; spread of `v(out)` vs reference ngspice | User statistical parameters |
| SIMADV-MC-13 | simulation-monte-carlo | iPad, 2 000-sample run with a 0.8 s nominal | confirmation dialog estimate; background pause/resume events | Parallel and sequential execution |
| SIMADV-MEAS-01 | simulation-measurements-and-statistics | synthetic vectors: `x=[0,1,2,3]`, `y=[0,4,-2,0]`; triangle and RC rise vectors from `tests/golden/SIM` | `pp`, `integ`, `deriv_at`, `min_at`, `max_at`, `when ... last`, `falltime` | Measurement definitions |
| SIMADV-MEAS-02 | simulation-measurements-and-statistics | AC vector `H(f)=1000/(1+jf/1k)` at 100 pts/dec, 1 Hz–100 MHz; second vector with 3 poles | `dcgain`, `ugbw`, `bw3db`, `phase_margin`, `gain_margin` | Measurement definitions |
| SIMADV-MEAS-03 | simulation-measurements-and-statistics | expressions `-2^2 + 10/4`, `a0*fc`, `ln(0)`, cyclic `a=b+1; b=a` | values; no-value reasons; definition error | Measurement expressions |
| SIMADV-MEAS-04 | simulation-measurements-and-statistics | IHP `inv_mc_tb.sch` `.control` block; `.meas tran` TRIG/TARG, FIND AT, WHEN, AVG FROM/TO, `ERR` lines | imported measurement list; Hefesto values vs ngspice log values | Import of ngspice measure statements (Q-SIMADV-10) |
| SIMADV-MEAS-05 | simulation-measurements-and-statistics | specs `range 1.1 1.3` (v=1.25), `max 100p` with no crossing, `min 0` with and without `-scale` | result, margin, verdict | Specifications |
| SIMADV-MEAS-06 | simulation-measurements-and-statistics | values 1…5 with `range 0 6` and `max 4.5`; 1 000 values in [0.40, 0.62]; pairs (1,2),(2,4),(3,5),(4,9) | mean, s, percentiles, Cp, Cpk, bins, `r`, `ρ` vs reference script | Descriptive statistics; Process capability; Histograms; Correlation |
| SIMADV-MEAS-07 | simulation-measurements-and-statistics | 100 samples with 95 passes; 0 counted samples | yield and Wilson bounds in `%.17g` | Yield |
| SIMADV-MEAS-08 | simulation-measurements-and-statistics | SIMADV-CO-03 run set extended with MC (corner × 50 samples) | `statistics.csv` (per-corner groups then `all`), `points.csv` columns, JSON export | Result export; Descriptive statistics |
| SIMADV-MEAS-09 | simulation-measurements-and-statistics | add a measurement after a 20-point run with all waveforms kept, and after a 500-point run with `keep failing` | recomputed values without new jobs; `stale` marks | Measurement and statistics command options |
| SIMADV-OPT-01 | simulation-optimization-hooks | variable `wn 0.42 10 -grid 0.005`; candidates 1.2337, 0.3, 1.235 | snapped values; range error; cache hit on the third | Design variables; Evaluation interface |
| SIMADV-OPT-02 | simulation-optimization-hooks | synthetic results: reference power 1 mW; A 0.8 mW feasible; B 0.5 mW with margin −0.2 | `J(A)` = 0.8, `J(B)` = 4.5, ranking | Objective and cost |
| SIMADV-OPT-03 | simulation-optimization-hooks | INV130 candidates at `corners` and `corners+mc` (N = 50, fixed B) | worst-case `f`, minimum margins, yield; identical results across worker counts | Robustness levels; Evaluation interface |
| SIMADV-OPT-04 | simulation-optimization-hooks | agent task with an evaluation budget of 50; proposal evaluated only at `nominal` | budget error; "unverified" mark; project folder unchanged before acceptance | Budgets, isolation and approval |
| SIMADV-OPT-05 | simulation-optimization-hooks | 12 evaluations with constraints `gain`, `pm`; `sim opt history -export` | CSV header and row count; `sim opt eval` without a session | Evaluation history; Optimisation command options |
