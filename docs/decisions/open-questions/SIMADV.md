# Open Questions — SIMADV (advanced analog simulation)

Capabilities: `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`, `simulation-measurements-and-statistics`, `simulation-optimization-hooks`. Product decision: `docs/decisions/product.md#P-19`.

These capabilities are `native` and have no Magic counterpart. "Evidence" points at the sources below. They were cloned read-only for this research; nothing was built or run:
- ngspice: SourceForge `ngspice` master @ `7aa47164` (after ngspice-47, 2026-11-05). On 2026-11-22 (task 1.4 of `openspec/changes/m9-lvs-simulation`) the ngspice line references were re-checked against the pinned release ngspice-47, tag commit `a80f6e3e` (Q-SIM-7), read through the SourceForge raw file view and not cloned; the `Evidence:` lines now refer to that tree, and the differences that matter are noted under the entries
- sky130: `fossi-foundation/skywater-pdk-libs-sky130_fd_pr` @ `29b55153` (`combined_models/`), installed by open_pdks @ `c6df28c1` (1.0.617)
- gf180mcu: `google/gf180mcu-pdk-libs-gf180mcu_fd_pr` @ `aacc04cc` (`models/ngspice/`); open_pdks installs it from the FOSSi fork
- IHP: `IHP-GmbH/IHP-Open-PDK` @ `5e6d592e` (`ihp-sg13g2/libs.tech/ngspice/`, `libs.tech/xschem/sg13g2_tests/`)

### Q-SIMADV-1: New `sim` options belong in the `simulation-command` table
- Kind: doc-mismatch
- ngspice/PDK behavior: not applicable. The new capabilities add the `sim` options `sweep`, `doe`, `runset`, `corners`, `mc`, `meas`, `spec`, `stats` and `opt`. Capability `simulation-command` says its subcommand table is the complete list, and `sim help` prints it. Its `corner sweep` form and its `run -all` overlap with corner sets and run sets.
- Evidence: openspec/specs/simulation-command/spec.md (Requirement "Subcommand table"); openspec/specs/simulation-netlist-preparation/spec.md (Requirement "Corners")
- Options: (a) add the new rows to `simulation-command` and define `corner sweep` as creating an unnamed user corner set; (b) keep the rows in the new capabilities and make the table in `simulation-command` say it is extended by them.
- Recommendation: (a), in the next change to `simulation-command`. Also make `sim help` list the new options, and make `sim run -all` start a run set (the new capabilities already assume this).
- Resolution: fix. `simulation-command` now lists `corners`, `sweep`/`doe`/`runset`, `mc`, `meas`/`spec`/`stats` and `opt` in its table (syntax delegated to the owning capabilities), `sim help` lists them, an exact option name wins over longer names it prefixes, `corner sweep` defines an unnamed user corner set, and `run -all` starts a run set (2026-11-06, audit G4).

### Q-SIMADV-2: sky130 model file, section list and descriptor
- Kind: doc-mismatch
- ngspice/PDK behavior: open_pdks installs the "combined" models at `libs.tech/combined/sky130.lib.spice`. xschem_sky130 uses that path (`SKYWATER_MODELS`). The descriptor in `simulation-netlist-preparation` names `sky130.lib.spice` without a directory, and its model root may be the legacy `libs.tech/ngspice`. The combined file defines 49 sections: `tt sf ff ss fs ll hh hl lh`, 15 device-and-R/C sections, their 24 `_mm` mismatch forms, and `mc`. The header comment lists `sf_ll` twice. There is no `sf_lh` and no `sf_lh_mm`, although every other process/RC combination exists.
- Evidence: combined_models/sky130.lib.spice:20-79 (header), :82-1378 (`.lib` lines); open_pdks sky130/Makefile.in:1508-1513; xschem_sky130 xschemrc:502
- Options: use `libs.tech/combined` as the sky130 model root everywhere; report the missing `sf_lh` upstream; keep `simulation-corners` rejecting it.
- Recommendation: switch the sky130 descriptor in `simulation-netlist-preparation` to `libs.tech/combined` (this also closes the sky130 part of Q-SIM-8). Check whether `set ng_nomodcheck` is still needed with the combined models.
- Resolution: fix. The sky130 descriptor in `simulation-netlist-preparation` uses model root `libs.tech/combined`; `simulation-corners` owns the full section list (2026-11-06, audit G4). The `ng_nomodcheck` check remains open under Q-SIM-8.

### Q-SIMADV-3: sky130 Monte Carlo switches
- Kind: ambiguity
- Status: approved 2026-11-22 as a conditional verify rule (design D1 of `openspec/changes/m9-lvs-simulation`; `docs/decisions/m9-decision-table.md`); `Verified` 2026-11-26 by task 3.3 (M9 R2)
- ngspice/PDK behavior: every section sets `.param MC_MM_SWITCH`, `MC_PR_SWITCH`, `corner_factor`, `process_mc_factor` and `mismatch_factor`. `tt` and the other process corners set both switches to 0. `<c>_mm` sets `MC_MM_SWITCH=1`. `mc` sets `MC_PR_SWITCH=1` and `MC_MM_SWITCH=0`, and is built on the typical parameters only. Process variation uses `X_mcz = 'agauss(0,1,1)'` and `X_mcd = 'X_mcz'` (one draw per die). Mismatch is an `AGAUSS(0,1.0,1)/sqrt(l*w*mult)` term in each FET subcircuit's `.param` (one draw per instance). No section enables process and mismatch together.
- Evidence: combined_models/sky130.lib.spice:82-87, :730-735, :1378-1384; continuous/models_global.spice:86-137; continuous/parameters_fet_tt.spice:17-41; continuous/models_fet/sky130_fd_pr__nfet_01v8.spice:32-40
- Options: (a) mode `both` = section `mc` plus a later `.param MC_MM_SWITCH=1` (current spec); this relies on how ngspice treats a second top-level definition of the same parameter, which was not verified; (b) define a Hefesto-side section that includes the `mc` contents with both switches set; (c) do not offer `both` for sky130.
- Recommendation: verify (a) with the reference ngspice: a deck with `.lib ... mc` then `.param MC_MM_SWITCH=1` must show instance-to-instance spread. If ngspice keeps the first definition, use (b). Also decide whether to expose `process_mc_factor` and `mismatch_factor` (variation scaling) in the UI.
- Resolution: verify — by the D-22 reference run: (a) if a top-level `.param MC_MM_SWITCH=1` placed after `.lib ... mc` takes effect (instance-to-instance spread appears), mode `both` is section `mc` plus that later `.param`; otherwise (b), mode `both` uses a Hefesto-side section that includes the `mc` contents with both switches set. In either case the variation-scaling factors (`process_mc_factor`, `mismatch_factor`) are not exposed in M9. A result that neither branch covers goes back to the user before gate G2-SIM. Approved by the user 2026-11-22.
- Verified 2026-11-26: branch (a). golden-capture run 45859754101 (mode `sim-refruns`, image `ngspice-47-hefesto2`, harness `tools/golden/refruns/run_refruns.py`): with `.lib sky130.lib.spice tt` and two identical `nfet_01v8` instances, a later top-level `.param MC_MM_SWITCH=1` takes effect (switch 1, instance-to-instance spread 0.0187) while the baseline without it has switch 0 and spread 0. Mode `both` is section `mc` plus that later `.param`; the variation-scaling factors are not exposed in M9.

### Q-SIMADV-4: gf180mcu statistical switches and defaults
- Kind: doc-mismatch
- Status: approved 2026-11-22 as a conditional verify rule (design D1 of `openspec/changes/m9-lvs-simulation`; `docs/decisions/m9-decision-table.md`); `Verified` 2026-11-26 by task 3.3 (M9 R2)
- ngspice/PDK behavior: `design.ngspice` sets `sw_stat_global = 0` and `sw_stat_mismatch = 0`, but its comment says the default is 1 for both. `mc_skew = 3` multiplies process deltas drawn with `agauss(0, 1, 3)`, which gives one standard deviation in total. The `typical` section and the other corner sections include `fets_mm`, whose instances use `agauss(0, var, 1)` multiplied by `sw_stat_mismatch`. So mismatch can be switched on at any corner, while process variation needs section `statistical`. The repository files end in `.ngspice`. open_pdks installs `*.spice` from the FOSSi fork and adds `.ngspice` symlinks. The smoke test's `.spiceinit` sets `ngbehavior=hs`, while the bundled descriptor uses `hsa`. The nominal temperature in the default corner set (25 °C) is assumed, not checked against the models' `tnom`.
- Evidence: models/ngspice/design.ngspice:22-71; models/ngspice/sm141064.ngspice:105-139, :566-711, :47041-47063; models/ngspice/testing/smoke_test/.spiceinit; open_pdks gf180mcu/Makefile.in:1115-1136
- Options: follow the files (current spec: Hefesto sets the switches explicitly); confirm the file names in the pinned open_pdks install; confirm `tnom` and the compatibility mode.
- Recommendation: keep the explicit `.param` overrides, which work whatever the file defaults are. Confirm that a `.param` placed after `.include design.ngspice` overrides it (same question as Q-SIMADV-3). Report the comment mismatch upstream.
- Resolution: verify — by the D-22 reference run: (a) if the later top-level definition wins, Hefesto keeps the explicit `.param sw_stat_global`/`sw_stat_mismatch` overrides after `.include design.ngspice`; otherwise (b), it uses a Hefesto-side section that sets the switches, as in Q-SIMADV-3 (b). The compatibility mode follows D-27: (a) `hs`, as the PDK sets, unless the D-27 run shows a difference that `hsa` fixes, then (b) `hsa`. The same runs confirm the installed file names and `tnom`. The comment mismatch is reported upstream. A result that no branch covers goes back to the user before gate G2-SIM. Approved by the user 2026-11-22.
- Verified 2026-11-26: D-22 branch (a), D-27 branch (a). golden-capture run 45859754101 (mode `sim-refruns`, image `ngspice-47-hefesto2`, harness `tools/golden/refruns/run_refruns.py`): after `.include design.ngspice`, later top-level `sw_stat_global=1` and `sw_stat_mismatch=1` take effect (spread 0.00204 against 0 at baseline), so Hefesto keeps the explicit overrides. `ngbehavior=hs` and `hsa` give identical currents on the PDK device test (in 4.01139e-4, ip 1.69727e-4), so the descriptor uses `hs`, as the PDK sets (`simulation-netlist-preparation` table amended). The pinned open_pdks install has both `.ngspice` and `.spice` files (`design`, `sm141064`, `sm141064_mim`, `smbb000149`), and the models' `tnom` is 25, matching the default corner set. The upstream report of the comment mismatch is still to be filed.

### Q-SIMADV-5: IHP sg13g2 statistical sections and `mm_ok`
- Kind: ambiguity
- Status: approved 2026-11-22 (the rest, `mm_ok=1` for extracted devices, D-23 option (a); recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice/PDK behavior: the corner libraries define `<x>_mismatch` sections for MOS (all five corners), RES, CAP, HBT and MOSCAP, and `_stat` sections only for typical (`mos_tt_stat`, `res_stat`, `cap_typ_stat`, `hbt_typ_stat`, `dio_tt_stat`, `moscap_tt_stat`). `cornerRES.lib` also has `res_stat_mismatch`. No MOS section enables process and mismatch together. Process variation uses `gauss(nominal, rel, num_sigmas)` with `num_sigmas = 1`. Mismatch uses instance-line `agauss(..., (mm_ok != 1 ? 0 : 1))`: the subcircuit default is `mm_ok=0`, and the xschem symbols pass `mm_ok=@mm_ok` with template value 1. The IHP Monte Carlo testbenches also set top-level `.param mm_ok=1` and `.param mc_ok=1`. `mc_ok` is not used by any model file, and the top-level `mm_ok` is shadowed by the subcircuit parameter. The sg13g2 `.spiceinit` loads six OSDI modules (`psp103`, `psp103_nqs`, `r3_cmc`, `mosvar`, `cap_cmomi`, `cap_cmomf`). The built-in set in `simulation-engine-embedding` names only the first four.
- Evidence: ngspice/models/cornerMOSlv.lib:74-565; cornerRES.lib:19-240; sg13g2_moslv_stat.lib:21-30; sg13g2_moslv_mod_mismatch.lib:66-118; xschem/sg13g2_pr/sg13_lv_nmos.sym:20-26; xschem/sg13g2_tests/mc_lv_nmos_cs_loop.sch:30-64; ngspice/.spiceinit
- Options: (a) no `both` mode for sg13g2 (current spec); (b) a Hefesto-defined combined section that includes both the `_stat` and the `_mod_mismatch` libraries; (c) ask IHP for an official combined section.
- Recommendation: (a) now and (c) upstream. Add `cap_cmomi` and `cap_cmomf` to the built-in OSDI list (a gap in `simulation-engine-embedding`; see Q-SIM-2). For layout-extracted DUTs, decide whether the netlister emits `mm_ok=1` on sg13g2 devices; otherwise extracted circuits never show mismatch.
- Resolution: fix. `cap_cmomi` and `cap_cmomf` (and `psp103`) added to the built-in OSDI set in `simulation-engine-embedding` and to the sg13g2 descriptor in `simulation-netlist-preparation`; option (a) stays (2026-11-06, audit G4). The rest (D-23, option (a)): simulation preparation adds `mm_ok=1` to extracted sg13g2 device instances when the Monte Carlo mode includes mismatch, inside the deck only; `ext2spice` output does not change (`simulation-netlist-preparation` "sg13g2 mismatch flag on extracted devices"). Approved by the user 2026-11-22.

### Q-SIMADV-6: [platform] Random sequence depends on the C library
- Kind: platform
- ngspice/PDK behavior: `setseed n` calls `srand(n)`, and the eight state words of the Tausworthe/LCG generator are then taken from `rand()`. glibc and Apple's libc have different `rand()` sequences, so the same seed gives different Monte Carlo samples on Linux and on macOS/iPadOS. The generator itself (`CombLCGTaus`) is integer arithmetic and portable. The Gaussian draw uses `log` and `sqrt`.
- Evidence: src/maths/misc/randnumb.c:101-116, :140-150, :220-235, :292-322
- Options: (a) a Hefesto patch that derives the state words from SplitMix64 (current spec); (b) ship a fixed portable `rand()` replacement linked into ngspice only; (c) accept platform-dependent samples and compare only statistics across platforms.
- Recommendation: (a), and offer it upstream as an option (`set portable_seed`). The reference ngspice for golden cases must carry the same patch. Check that `log` differences between math libraries never flip a rejection-sampling decision (`r >= 1.0`) for the golden seeds.
- Resolution: fix. Option (a) as specified in `simulation-monte-carlo` (Requirement "Portable random numbers"); `simulation-engine-embedding` now requires the patch in the build manifest (2026-11-06, audit G4).

### Q-SIMADV-7: [platform] Engine seeding and `set rndseed=1`
- Kind: doc-mismatch
- ngspice/PDK behavior: `ngSpice_Init` sets the variable `rndseed` to 1 and then calls the seeding routine with no argument, which seeds from `getpid()` plus a counter. `set rndseed=<n>` only changes the variable. The variable is read back only by the vector functions (`rnd`, `sgauss`, ...) through `checkseed`, and then only when the value changes. So the numparam statistical functions (`agauss`, `gauss`, ...) are seeded from the process id unless `setseed` or `.option seed` is used. `simulation-engine-embedding` (Requirement "Deterministic engine initialisation") sends `set rndseed=1` and claims bit-identical random sources across fresh engines. That holds only if the source in question reads `rndseed`. Independently, ngspice after 47 changed when top-level statistical parameters are drawn (once per load, not per reference); `stat_param_as_func` restores the old behaviour.
- Evidence: src/sharedspice.c:930-935, :1079-1081; src/maths/misc/randnumb.c:71-92, :286-322; src/frontend/trannoise/wallace.c:76-85; src/maths/cmaths/cmath2.c:137; src/frontend/inpcom.c:8226-8245; src/frontend/inp.c:426-470
- Options: change the fixed init list in `simulation-engine-embedding` to send `setseed 1` before every deck load (not only for Monte Carlo); verify which RNG `trnoise`/`trrandom` sources use; pin the post-47 statistical-parameter semantics.
- Recommendation: send `setseed <s>` before every deck load, with `s = 1` for ordinary jobs and `s_i` for Monte Carlo samples. Update scenario "Reproducible random sources" in `simulation-engine-embedding`. Add a golden case that fails if the seed came from the pid (SIMADV-MC-03).
- Resolution: fix. `simulation-engine-embedding` no longer sends `set rndseed=1`; it sends `setseed <s>` before every deck load (`s = 1`, or `s_i` for Monte Carlo samples). Verified in the ngspice clone: `setseed` calls `srand`, `TausSeed` and `initw` (the Wallace pool used by `trnoise`), randnumb.c:292-330. The post-47 statistical-parameter semantics are pinned by `simulation-monte-carlo` (no `stat_param_as_func`) (2026-11-06, audit G4).
- Re-checked 2026-11-22 against ngspice-47 (`a80f6e3e`): the verification above was made in master `7aa47164`. In ngspice-47, `setseed` (`com_sseed`, randnumb.c:292-322) calls `srand` and `TausSeed` but not `initw`; the `trnoise` pool is seeded with `srand(getpid())` by `initw` (wallace.c:76-85), called once by `ngSpice_Init` (sharedspice.c:1079-1081). `.option seed=` is evaluated when a deck is loaded (inp.c:426-470), not by the `option` command. ngspice-47 has no `stat_param_as_func`: every top-level `.param` with a statistical function becomes a function re-evaluated at each reference (inpcom.c:8226-8238); the draw-once semantics this Resolution pins are post-47. Whether the Hefesto patch series carries these post-47 behaviours is raised in `docs/decisions/m9-decision-table.md` ("Trace re-check against ngspice-47").

### Q-SIMADV-8: Monte Carlo around non-typical corners
- Kind: ambiguity
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice/PDK behavior: the statistical sections of all three PDKs vary around the typical process. sky130 has `corner_factor` and `process_mc_factor` parameters that could combine a corner shift with process variation. gf180mcu and sg13g2 have no equivalent.
- Evidence: combined_models/sky130.lib.spice:82-87; continuous/parameters_fet_tt.spice:17-41; sm141064.ngspice:566-711; cornerMOSlv.lib:163-205
- Options: (a) reject `process`/`both` at non-typical corners unless the descriptor names a statistical counterpart (current spec); (b) for sky130, build "corner plus process MC" from `parameters_fet_<c>.spice` with `MC_PR_SWITCH=1`.
- Recommendation: (a) for the first release; evaluate (b) with the PDK maintainers.
- Resolution: other — option (a): `process` and `both` are rejected at non-typical corners unless the descriptor names a statistical counterpart; option (b) for sky130 is evaluated with the PDK maintainers later. Approved by the user 2026-11-22.

### Q-SIMADV-9: Overriding parameters in point decks
- Kind: ambiguity
- Status: approved 2026-11-22 as a conditional verify rule (design D1 of `openspec/changes/m9-lvs-simulation`; `docs/decisions/m9-decision-table.md`); `Verified` 2026-11-26 by task 3.3 (M9 R2)
- ngspice/PDK behavior: point decks replace the single top-level `.param` definition of a swept name. A name may also be defined inside a `.lib` section or an included file (PDK switches), with `place=end` code blocks in schematics, or changed at run time by `alterparam` in `.control`. How ngspice resolves two top-level definitions of one name was not verified.
- Evidence: src/frontend/inpcom.c:1140-1160; src/frontend/inp.c:1741-1744
- Options: (a) only top-level deck text is changed and definitions in included files are reported as "cannot sweep" (current spec, partly); (b) always append an override line after the last model line, if ngspice gives the last definition priority; (c) use `alterparam` plus `mc_source` from Hefesto.
- Recommendation: determine ngspice's precedence with a reference test. If the last definition wins, switch to (b) for names defined in included files, since it is simpler and does not edit user text.
- Resolution: verify — by the D-22 reference run: (b) if the last definition wins, point decks append override lines after the last model line for names defined in included files; otherwise (a), only top-level deck text is changed and names defined in included files are reported as "cannot sweep". A result that neither branch covers goes back to the user before gate G2-SIM. Approved by the user 2026-11-22.
- Verified 2026-11-26: branch (b). golden-capture run 45859754101 (mode `sim-refruns`, image `ngspice-47-hefesto2`, harness `tools/golden/refruns/run_refruns.py`): the last top-level definition wins in all three PDKs (sky130 `MC_MM_SWITCH`, gf180mcu `sw_stat_*`, IHP `sg13g2_lv_nmos_muew` 0.85 in `cornerMOSlv.lib mos_tt` overridden to 0.5 by a later `.param`). Point decks append override lines after the last model line for names defined in included files.

### Q-SIMADV-10: Hefesto measurements versus ngspice `.meas`
- Kind: ambiguity
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice/PDK behavior: ngspice computes `.meas` results itself, with its own interpolation, crossing counting, `TD` handling and `PHASE_MARGIN`/`GAIN_MARGIN` definitions, and prints them in the log. Hefesto computes imported measurements on the stored vectors, so the two values can differ slightly. The schematic testbenches of IHP use `meas` in `.control` blocks on `let` vectors (for example `gain_dB = vdb(inv_out)`). It was not verified that every `let` vector is part of the plot that Hefesto copies out.
- Evidence: src/frontend/com_measure2.c:51-57, :207-257; ihp-sg13g2/libs.tech/xschem/sg13g2_tests/inv_mc_tb.sch:91-140
- Options: (a) Hefesto value authoritative and ngspice value in the log (current spec); (b) also capture the ngspice value and flag differences larger than a tolerance.
- Recommendation: (a) plus (b) as a diagnostic in debug builds. Add golden cases that compare both (SIMADV-MEAS-04).
- Resolution: other — option (a) with (b) in debug builds: Hefesto's measurement value is authoritative and ngspice's printed value stays in the log; debug builds also capture ngspice's value and warn when the two differ by more than the tolerance (SIMADV-MEAS-04). Approved by the user 2026-11-22.
- Re-checked 2026-11-22 against ngspice-47 (`a80f6e3e`): its `.meas` has no `PHASE_MARGIN` or `GAIN_MARGIN` form (com_measure2.c:51-57, :207-257); those are post-47 additions, so ngspice-47 itself does not recognise them as measure forms.

### Q-SIMADV-11: Waveform retention and stale measurements
- Kind: ambiguity
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice/PDK behavior: not applicable. A 10 000-sample transient Monte Carlo run with all waveforms kept can need many gigabytes, which is unworkable on iPad. With `keep = failing`, measurements added after the run cannot be computed for points without waveforms.
- Evidence: openspec/specs/simulation-sweeps/spec.md (Requirement "Run set storage"); openspec/specs/simulation-analyses-and-results/spec.md (Requirement "Saved vectors and memory budget")
- Options: the defaults in the spec (`all` up to 100 points, otherwise `failing`); keep only the vectors that measurements use; store decimated waveforms for overlays.
- Recommendation: add a `measured` retention mode (only the vectors referenced by measurements, at full resolution), and make it the default above 100 points once its storage cost has been measured.
- Resolution: other — the `measured` retention mode is added now; it becomes the default above 100 points only after `sim-bench` has measured its storage cost and the user approves (D-24); until then the defaults stay as specified. Approved by the user 2026-11-22.

### Q-SIMADV-12: Layout geometry as optimisation variables
- Kind: ambiguity
- ngspice/PDK behavior: not applicable. Design variables are testbench parameters, so they size schematic devices. For post-layout tuning (capability `agent-schematic-driven-layout`), sizes live in PCell parameters and drawn geometry, which need a new layout, a new extraction and a re-prepared base deck for each candidate.
- Evidence: openspec/specs/agent-schematic-driven-layout/spec.md (Requirement "Iterative improvement"); openspec/specs/simulation-optimization-hooks/spec.md (Requirement "Evaluation interface")
- Options: (a) parameters only, with the agent editing layout between sessions (current spec); (b) PCell-parameter variables that regenerate and re-extract in the scratch session for each candidate.
- Recommendation: (a) for the first release. Specify (b) together with the agent's PCell tools, including its cost per evaluation and how it interacts with the result cache.

### Q-SIMADV-13: Worker pool size and result order for run sets
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan, `openspec/changes/m9-lvs-simulation` design D9; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`)
- ngspice/PDK behavior: there is one ngspice instance per address space (Q-SIM-5). Sweeps, corners and Monte Carlo each run many jobs. Q-SIM-5 recommends reviewing the default worker count `min(4, cores − 1)` against the memory that large RC netlists use.
- Evidence: `openspec/specs/simulation-engine-embedding/spec.md` (Requirement "Multiple simulations and corner parallelism"); `openspec/specs/simulation-sweeps/spec.md` (Requirement "Run set execution"); Q-SIM-5
- Options:
  - (a) Use `max(1, min(4, physical_cores − 1, ⌊B_total / B_job⌋))` workers, defined once in `simulation-engine-embedding` "Multiple simulations and corner parallelism" (which today allows 1 to the core count) and referenced by the run-set capabilities:
    - `B_total` is half the physical memory;
    - `B_job` is the job's result budget plus an estimate of the deck size;
    - results are stored and reported by point index, whatever order the jobs finish in.
  - (b) Use `min(4, cores − 1)` workers, with no memory term.
  - (c) A user setting only.
- Recommendation: (a), plus a user setting that can only lower the count. Results then do not depend on how many workers ran, and a large post-layout deck does not start four copies that exhaust memory.
- Resolution: platform — option (a): `N = max(1, min(4, physical_cores − 1, ⌊B_total / B_job⌋))` workers, with `B_total` half the physical memory and `B_job` the job's result budget plus an estimate of its prepared deck size, defined once in `simulation-engine-embedding` "Multiple simulations and corner parallelism"; a user setting can only lower `N`, never raise it; results are stored and reported in point-index order, whatever order the jobs finish in. Approved by the user 2026-11-22.
