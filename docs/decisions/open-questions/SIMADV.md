# Open Questions — SIMADV (advanced analog simulation)

Capabilities: `simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`, `simulation-measurements-and-statistics`, `simulation-optimization-hooks`. Product decision: `docs/decisions/product.md#P-19`.

These capabilities are `native` and have no Magic counterpart. "Evidence" points at the sources below. They were cloned read-only for this research; nothing was built or run:
- ngspice: SourceForge `ngspice` master @ `7aa47164` (after ngspice-47, 2026-11-05)
- sky130: `fossi-foundation/skywater-pdk-libs-sky130_fd_pr` @ `29b55153` (`combined_models/`), installed by open_pdks @ `c6df28c1` (1.0.617)
- gf180mcu: `google/gf180mcu-pdk-libs-gf180mcu_fd_pr` @ `aacc04cc` (`models/ngspice/`); open_pdks installs it from the FOSSi fork
- IHP: `IHP-GmbH/IHP-Open-PDK` @ `5e6d592e` (`ihp-sg13g2/libs.tech/ngspice/`, `libs.tech/xschem/sg13g2_tests/`)

### Q-SIMADV-1: New `sim` options belong in the `simulation-command` table
- Kind: doc-mismatch
- ngspice/PDK behavior: not applicable. The new capabilities add the `sim` options `sweep`, `doe`, `runset`, `corners`, `mc`, `meas`, `spec`, `stats` and `opt`. Capability `simulation-command` says its subcommand table is the complete list, and `sim help` prints it. Its `corner sweep` form and its `run -all` overlap with corner sets and run sets.
- Evidence: openspec/specs/simulation-command/spec.md (Requirement "Subcommand table"); openspec/specs/simulation-netlist-preparation/spec.md (Requirement "Corners")
- Options: (a) add the new rows to `simulation-command` and define `corner sweep` as creating an unnamed user corner set; (b) keep the rows in the new capabilities and make the table in `simulation-command` say it is extended by them.
- Recommendation: (a), in the next change to `simulation-command`. Also make `sim help` list the new options, and make `sim run -all` start a run set (the new capabilities already assume this).

### Q-SIMADV-2: sky130 model file, section list and descriptor
- Kind: doc-mismatch
- ngspice/PDK behavior: open_pdks installs the "combined" models at `libs.tech/combined/sky130.lib.spice`. xschem_sky130 uses that path (`SKYWATER_MODELS`). The descriptor in `simulation-netlist-preparation` names `sky130.lib.spice` without a directory, and its model root may be the legacy `libs.tech/ngspice`. The combined file defines 49 sections: `tt sf ff ss fs ll hh hl lh`, 15 device-and-R/C sections, their 24 `_mm` mismatch forms, and `mc`. The header comment lists `sf_ll` twice. There is no `sf_lh` and no `sf_lh_mm`, although every other process/RC combination exists.
- Evidence: combined_models/sky130.lib.spice:20-79 (header), :82-1378 (`.lib` lines); open_pdks sky130/Makefile.in:1508-1513; xschem_sky130 xschemrc:502
- Options: use `libs.tech/combined` as the sky130 model root everywhere; report the missing `sf_lh` upstream; keep `simulation-corners` rejecting it.
- Recommendation: switch the sky130 descriptor in `simulation-netlist-preparation` to `libs.tech/combined` (this also closes the sky130 part of Q-SIM-8). Check whether `set ng_nomodcheck` is still needed with the combined models.

### Q-SIMADV-3: sky130 Monte Carlo switches
- Kind: ambiguity
- ngspice/PDK behavior: every section sets `.param MC_MM_SWITCH`, `MC_PR_SWITCH`, `corner_factor`, `process_mc_factor` and `mismatch_factor`. `tt` and the other process corners set both switches to 0. `<c>_mm` sets `MC_MM_SWITCH=1`. `mc` sets `MC_PR_SWITCH=1` and `MC_MM_SWITCH=0`, and is built on the typical parameters only. Process variation uses `X_mcz = 'agauss(0,1,1)'` and `X_mcd = 'X_mcz'` (one draw per die). Mismatch is an `AGAUSS(0,1.0,1)/sqrt(l*w*mult)` term in each FET subcircuit's `.param` (one draw per instance). No section enables process and mismatch together.
- Evidence: combined_models/sky130.lib.spice:82-87, :730-735, :1378-1384; continuous/models_global.spice:86-137; continuous/parameters_fet_tt.spice:17-41; continuous/models_fet/sky130_fd_pr__nfet_01v8.spice:32-40
- Options: (a) mode `both` = section `mc` plus a later `.param MC_MM_SWITCH=1` (current spec); this relies on how ngspice treats a second top-level definition of the same parameter, which was not verified; (b) define a Hefesto-side section that includes the `mc` contents with both switches set; (c) do not offer `both` for sky130.
- Recommendation: verify (a) with the reference ngspice: a deck with `.lib ... mc` then `.param MC_MM_SWITCH=1` must show instance-to-instance spread. If ngspice keeps the first definition, use (b). Also decide whether to expose `process_mc_factor` and `mismatch_factor` (variation scaling) in the UI.

### Q-SIMADV-4: gf180mcu statistical switches and defaults
- Kind: doc-mismatch
- ngspice/PDK behavior: `design.ngspice` sets `sw_stat_global = 0` and `sw_stat_mismatch = 0`, but its comment says the default is 1 for both. `mc_skew = 3` multiplies process deltas drawn with `agauss(0, 1, 3)`, which gives one standard deviation in total. The `typical` section and the other corner sections include `fets_mm`, whose instances use `agauss(0, var, 1)` multiplied by `sw_stat_mismatch`. So mismatch can be switched on at any corner, while process variation needs section `statistical`. The repository files end in `.ngspice`. open_pdks installs `*.spice` from the FOSSi fork and adds `.ngspice` symlinks. The smoke test's `.spiceinit` sets `ngbehavior=hs`, while the bundled descriptor uses `hsa`. The nominal temperature in the default corner set (25 °C) is assumed, not checked against the models' `tnom`.
- Evidence: models/ngspice/design.ngspice:22-71; models/ngspice/sm141064.ngspice:105-139, :566-711, :47041-47063; models/ngspice/testing/smoke_test/.spiceinit; open_pdks gf180mcu/Makefile.in:1115-1136
- Options: follow the files (current spec: Hefesto sets the switches explicitly); confirm the file names in the pinned open_pdks install; confirm `tnom` and the compatibility mode.
- Recommendation: keep the explicit `.param` overrides, which work whatever the file defaults are. Confirm that a `.param` placed after `.include design.ngspice` overrides it (same question as Q-SIMADV-3). Report the comment mismatch upstream.

### Q-SIMADV-5: IHP sg13g2 statistical sections and `mm_ok`
- Kind: ambiguity
- ngspice/PDK behavior: the corner libraries define `<x>_mismatch` sections for MOS (all five corners), RES, CAP, HBT and MOSCAP, and `_stat` sections only for typical (`mos_tt_stat`, `res_stat`, `cap_typ_stat`, `hbt_typ_stat`, `dio_tt_stat`, `moscap_tt_stat`). `cornerRES.lib` also has `res_stat_mismatch`. No MOS section enables process and mismatch together. Process variation uses `gauss(nominal, rel, num_sigmas)` with `num_sigmas = 1`. Mismatch uses instance-line `agauss(..., (mm_ok != 1 ? 0 : 1))`: the subcircuit default is `mm_ok=0`, and the xschem symbols pass `mm_ok=@mm_ok` with template value 1. The IHP Monte Carlo testbenches also set top-level `.param mm_ok=1` and `.param mc_ok=1`. `mc_ok` is not used by any model file, and the top-level `mm_ok` is shadowed by the subcircuit parameter. The sg13g2 `.spiceinit` loads six OSDI modules (`psp103`, `psp103_nqs`, `r3_cmc`, `mosvar`, `cap_cmomi`, `cap_cmomf`). The built-in set in `simulation-engine-embedding` names only the first four.
- Evidence: ngspice/models/cornerMOSlv.lib:74-565; cornerRES.lib:19-240; sg13g2_moslv_stat.lib:21-30; sg13g2_moslv_mod_mismatch.lib:66-118; xschem/sg13g2_pr/sg13_lv_nmos.sym:20-26; xschem/sg13g2_tests/mc_lv_nmos_cs_loop.sch:30-64; ngspice/.spiceinit
- Options: (a) no `both` mode for sg13g2 (current spec); (b) a Hefesto-defined combined section that includes both the `_stat` and the `_mod_mismatch` libraries; (c) ask IHP for an official combined section.
- Recommendation: (a) now and (c) upstream. Add `cap_cmomi` and `cap_cmomf` to the built-in OSDI list (a gap in `simulation-engine-embedding`; see Q-SIM-2). For layout-extracted DUTs, decide whether the netlister emits `mm_ok=1` on sg13g2 devices; otherwise extracted circuits never show mismatch.

### Q-SIMADV-6: [platform] Random sequence depends on the C library
- Kind: platform
- ngspice/PDK behavior: `setseed n` calls `srand(n)`, and the eight state words of the Tausworthe/LCG generator are then taken from `rand()`. glibc and Apple's libc have different `rand()` sequences, so the same seed gives different Monte Carlo samples on Linux and on macOS/iPadOS. The generator itself (`CombLCGTaus`) is integer arithmetic and portable. The Gaussian draw uses `log` and `sqrt`.
- Evidence: src/maths/misc/randnumb.c:101-116, :140-150, :220-235, :304-330
- Options: (a) a Hefesto patch that derives the state words from SplitMix64 (current spec); (b) ship a fixed portable `rand()` replacement linked into ngspice only; (c) accept platform-dependent samples and compare only statistics across platforms.
- Recommendation: (a), and offer it upstream as an option (`set portable_seed`). The reference ngspice for golden cases must carry the same patch. Check that `log` differences between math libraries never flip a rejection-sampling decision (`r >= 1.0`) for the golden seeds.

### Q-SIMADV-7: [platform] Engine seeding and `set rndseed=1`
- Kind: doc-mismatch
- ngspice/PDK behavior: `ngSpice_Init` sets the variable `rndseed` to 1 and then calls the seeding routine with no argument, which seeds from `getpid()` plus a counter. `set rndseed=<n>` only changes the variable. The variable is read back only by the vector functions (`rnd`, `sgauss`, ...) through `checkseed`, and then only when the value changes. So the numparam statistical functions (`agauss`, `gauss`, ...) are seeded from the process id unless `setseed` or `.option seed` is used. `simulation-engine-embedding` (Requirement "Deterministic engine initialisation") sends `set rndseed=1` and claims bit-identical random sources across fresh engines. That holds only if the source in question reads `rndseed`. Independently, ngspice after 47 changed when top-level statistical parameters are drawn (once per load, not per reference); `stat_param_as_func` restores the old behaviour.
- Evidence: src/sharedspice.c:930-936; src/maths/misc/randnumb.c:71-92, :287-330; src/maths/cmaths/cmath2.c:137; src/frontend/inpcom.c:8380-8412
- Options: change the fixed init list in `simulation-engine-embedding` to send `setseed 1` before every deck load (not only for Monte Carlo); verify which RNG `trnoise`/`trrandom` sources use; pin the post-47 statistical-parameter semantics.
- Recommendation: send `setseed <s>` before every deck load, with `s = 1` for ordinary jobs and `s_i` for Monte Carlo samples. Update scenario "Reproducible random sources" in `simulation-engine-embedding`. Add a golden case that fails if the seed came from the pid (SIMADV-MC-03).

### Q-SIMADV-8: Monte Carlo around non-typical corners
- Kind: ambiguity
- ngspice/PDK behavior: the statistical sections of all three PDKs vary around the typical process. sky130 has `corner_factor` and `process_mc_factor` parameters that could combine a corner shift with process variation. gf180mcu and sg13g2 have no equivalent.
- Evidence: combined_models/sky130.lib.spice:82-87; continuous/parameters_fet_tt.spice:17-41; sm141064.ngspice:566-711; cornerMOSlv.lib:163-205
- Options: (a) reject `process`/`both` at non-typical corners unless the descriptor names a statistical counterpart (current spec); (b) for sky130, build "corner plus process MC" from `parameters_fet_<c>.spice` with `MC_PR_SWITCH=1`.
- Recommendation: (a) for the first release; evaluate (b) with the PDK maintainers.

### Q-SIMADV-9: Overriding parameters in point decks
- Kind: ambiguity
- ngspice/PDK behavior: point decks replace the single top-level `.param` definition of a swept name. A name may also be defined inside a `.lib` section or an included file (PDK switches), with `place=end` code blocks in schematics, or changed at run time by `alterparam` in `.control`. How ngspice resolves two top-level definitions of one name was not verified.
- Evidence: src/frontend/inpcom.c:1140-1160; src/frontend/inp.c:1831-1834
- Options: (a) only top-level deck text is changed and definitions in included files are reported as "cannot sweep" (current spec, partly); (b) always append an override line after the last model line, if ngspice gives the last definition priority; (c) use `alterparam` plus `mc_source` from Hefesto.
- Recommendation: determine ngspice's precedence with a reference test. If the last definition wins, switch to (b) for names defined in included files, since it is simpler and does not edit user text.

### Q-SIMADV-10: Hefesto measurements versus ngspice `.meas`
- Kind: ambiguity
- ngspice/PDK behavior: ngspice computes `.meas` results itself, with its own interpolation, crossing counting, `TD` handling and `PHASE_MARGIN`/`GAIN_MARGIN` definitions, and prints them in the log. Hefesto computes imported measurements on the stored vectors, so the two values can differ slightly. The schematic testbenches of IHP use `meas` in `.control` blocks on `let` vectors (for example `gain_dB = vdb(inv_out)`). It was not verified that every `let` vector is part of the plot that Hefesto copies out.
- Evidence: src/frontend/com_measure2.c:55-62, :215-262; ihp-sg13g2/libs.tech/xschem/sg13g2_tests/inv_mc_tb.sch:91-140
- Options: (a) Hefesto value authoritative and ngspice value in the log (current spec); (b) also capture the ngspice value and flag differences larger than a tolerance.
- Recommendation: (a) plus (b) as a diagnostic in debug builds. Add golden cases that compare both (SIMADV-MEAS-04).

### Q-SIMADV-11: Waveform retention and stale measurements
- Kind: ambiguity
- ngspice/PDK behavior: not applicable. A 10 000-sample transient Monte Carlo run with all waveforms kept can need many gigabytes, which is unworkable on iPad. With `keep = failing`, measurements added after the run cannot be computed for points without waveforms.
- Evidence: openspec/specs/simulation-sweeps/spec.md (Requirement "Run set storage"); openspec/specs/simulation-analyses-and-results/spec.md (Requirement "Saved vectors and memory budget")
- Options: the defaults in the spec (`all` up to 100 points, otherwise `failing`); keep only the vectors that measurements use; store decimated waveforms for overlays.
- Recommendation: add a `measured` retention mode (only the vectors referenced by measurements, at full resolution), and make it the default above 100 points once its storage cost has been measured.

### Q-SIMADV-12: Layout geometry as optimisation variables
- Kind: ambiguity
- ngspice/PDK behavior: not applicable. Design variables are testbench parameters, so they size schematic devices. For post-layout tuning (capability `agent-schematic-driven-layout`), sizes live in PCell parameters and drawn geometry, which need a new layout, a new extraction and a re-prepared base deck for each candidate.
- Evidence: openspec/specs/agent-schematic-driven-layout/spec.md (Requirement "Iterative improvement"); openspec/specs/simulation-optimization-hooks/spec.md (Requirement "Evaluation interface")
- Options: (a) parameters only, with the agent editing layout between sessions (current spec); (b) PCell-parameter variables that regenerate and re-extract in the scratch session for each candidate.
- Recommendation: (a) for the first release. Specify (b) together with the agent's PCell tools, including its cost per evaluation and how it interacts with the result cache.
