# Golden cases — SIM area

The SIM capabilities are `native`: they have no Magic counterpart. References come from two sources:
- **Reference ngspice.** A stock ngspice executable built from the same pinned source and patch level as Hefesto's engine, run in batch mode (`ngspice -b -r <out.raw> deck.spice`). It produces reference vectors and raw files.
- **Magic `fc5daa9e`.** Headless Magic produces the DUT netlists (`ext2spice` with the settings of `simulation-netlist-preparation`, Requirement "Layout DUT netlist generation").

Comparisons ignore the `Date:` and `Command:` raw-file lines (Q-SIM-17), and use the tolerances of `simulation-engine-embedding`, Requirement "Cross-platform result consistency".

Abbreviations:
- **min tech**: the minimal test technology used by other areas
- **sky130A**, **gf180mcuD**, **sg13g2**: open_pdks installs at the pinned versions
- **RC1**: deck `V1 in 0 PULSE(0 1 0 1p 1p 1 2)`, `R1 in out 1k`, `C1 out 0 1p`, `.tran 1p 10n`
- **DIV**: deck `V1 in 0 1.8`, `R1 in out 1k`, `R2 out 0 2k`, `.op`

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| SIM-ENG-01 | simulation-engine-embedding | DIV as a raw deck; `sim run -wait` on Linux x86_64, macOS arm64 and iPadOS | `v(out)` per platform; reference ngspice `print v(out)` | Cross-platform result consistency; ngspice embedded as a library |
| SIM-ENG-02 | simulation-engine-embedding | `.spiceinit` with `set ngbehavior=ps` in HOME and CWD; run a sky130A inverter testbench | effective compatibility mode in the job log; vectors identical to a run without the file | Deterministic engine initialisation |
| SIM-ENG-03 | simulation-engine-embedding | deck with `V1 n 0 trnoise(10m 1n 0 0)`, `R1 n 0 1k`, `.tran 1n 1u`, run twice in fresh engines | bitwise comparison of `v(n)` | Deterministic engine initialisation (rndseed) |
| SIM-ENG-04 | simulation-engine-embedding | two jobs A (RC1, tstop 1 ms) and B (DIV) submitted back to back to one in-process engine | state timeline of B (`queued` until A is final); `ngSpice_AllPlots` before B loads | Single engine instance per address space |
| SIM-ENG-05 | simulation-engine-embedding | RC1 with tstop 1 ms; cancel at ~30 % | final state, partial `time` length and last value, time from cancel to `cancelled` (< 2 s) | Cancellation and fault states |
| SIM-ENG-06 | simulation-engine-embedding | deck that makes ngspice take its controlled-exit path (fatal parse error, e.g. unbalanced `.subckt`, bypassing Hefesto validation via a test hook) | job diagnostic; next DIV job succeeds after reset | Cancellation and fault states; Callback contract |
| SIM-ENG-07 | simulation-engine-embedding | fuzz set of 50 malformed decks run in sequence in one engine | number of `unavailable` transitions; correctness of a DIV run after each | Cancellation and fault states (Q-SIM-11) |
| SIM-ENG-08 | simulation-engine-embedding | macOS worker mode; deck that kills the worker (test hook sends SIGKILL) | job state and diagnostic `simulation engine terminated`; unsaved layout edit still present | Crash containment on desktop |
| SIM-ENG-09 | simulation-engine-embedding | iPad: modify cell `inv`, start a simulation | autosave timestamp before deck load in the event log | In-process execution on iPadOS |
| SIM-ENG-10 | simulation-engine-embedding | iPad: background the app during RC1 (tstop 1 ms), return after 30 s | pause/resume events; final vectors equal to an uninterrupted run | In-process execution on iPadOS |
| SIM-ENG-11 | simulation-engine-embedding | 5-corner sky130A sweep with workers N=1 and N=4 on an 8-core Mac | concurrency timeline; bitwise vector equality between N=1 and N=4 | Multiple simulations and corner parallelism |
| SIM-ENG-12 | simulation-engine-embedding | iPad deck with `pre_osdi /x/mymodel.osdi` | error text; no file opened as code | No runtime loading of executable code on iPadOS |
| SIM-ENG-13 | simulation-engine-embedding | sg13g2 inverter transient on iPad and Linux; host PDK dir holds an OSDI 0.3 `psp103_nqs.osdi` | which module was used (log); `v(out)` vs reference ngspice with OpenVAF-reloaded `.osdi` | OSDI compact models |
| SIM-ENG-14 | simulation-engine-embedding | release build license manifest | component and license list | ngspice build configuration and license hygiene |
| SIM-ENG-15 | simulation-engine-embedding | `sim engine` on each platform | key-value list | ngspice embedded as a library; Engine subcommand |
| SIM-NET-01 | simulation-netlist-preparation | min tech inverter `inv`; edit after extraction; `sim run` with source `layout inv` | re-extraction event; DUT text vs Magic `ext2spice` with the forced settings | Layout DUT netlist generation |
| SIM-NET-02 | simulation-netlist-preparation | same cell, parasitic modes `none`, `c`, `rc` (with `extresist-command`) | the three DUT texts vs Magic output; presence of `R0 a.t0 a.n0 2.0005`-style lines in `rc` | Parasitic modes |
| SIM-NET-03 | simulation-netlist-preparation | cell `buf` = two instances of `inv` (`inv_0`, `inv_1`), mode `all` | net-name map; vector list from reference ngspice (`display`) | Net name map |
| SIM-NET-04 | simulation-netlist-preparation | DUT with labels `A` and `a` on different nets | renamed deck lines; warning text; map entries | Case-collision handling (Q-SIM-6) |
| SIM-NET-05 | simulation-netlist-preparation | sky130A `inv` testbench, bindings for 3 of 4 ports | composed deck text, byte for byte | Testbench composition; Stimulus definitions |
| SIM-NET-06 | simulation-netlist-preparation | sources: pulse, sin with all 6 params, pwl with 4 points, dc+ac; invalid pwl | emitted lines; validation errors | Stimulus definitions |
| SIM-NET-07 | simulation-netlist-preparation | sky130A, gf180mcuD, sg13g2 default corners | model lines and init commands; successful reference-ngspice run of the same deck | Simulation model descriptor in the PDK store; PDK initialisation commands (Q-SIM-8) |
| SIM-NET-08 | simulation-netlist-preparation | imported netlist with relative `.include`, `$PDK_ROOT/...` and `$MYLIB/...` | rewritten absolute paths; error for `$MYLIB` | Model library resolution |
| SIM-NET-09 | simulation-netlist-preparation | sweep `tt,ss` × `27,-40` × supply 1.8 V × `1.0,0.9` | job names; pairwise deck diffs limited to the allowed lines | Corners |
| SIM-NET-10 | simulation-netlist-preparation | imported netlist with `.control … shell ls … .endc`; another with `.control run plot .endc` | errors / stripped-section warning; final deck | Deck validation and sanitisation (Q-SIM-9) |
| SIM-NET-11 | simulation-netlist-preparation | raw deck with no analysis; raw deck RC1 | error; deck passed unchanged (hash) | Raw decks; Deck delivery |
| SIM-NET-12 | simulation-netlist-preparation | job re-run after layout change, with and without "regenerate" | deck hash of each re-run | Deck delivery |
| SIM-RES-01 | simulation-analyses-and-results | one testbench per analysis type: op, dc (nested), ac dec, tran with tstart/tmax/uic, noise, tf | emitted cards; plot names and vector lists vs reference ngspice | Supported analyses |
| SIM-RES-02 | simulation-analyses-and-results | invalid parameters for each analysis | error texts | Supported analyses |
| SIM-RES-03 | simulation-analyses-and-results | deck failing with `Timestep too small` | final state, diagnostic, partial data | Job lifecycle |
| SIM-RUN-03 | simulation-analyses-and-results | `.op` + `.tran` job; then the same job run 3 more times in one engine | sequence of status strings and derived progress; monotonicity | Progress reporting (Q-SIM-14) |
| SIM-RES-04 | simulation-analyses-and-results | RC1 tstop 100 ns paused at ~40 ns and resumed | `time` vector monotonic, endpoints; vectors vs uninterrupted run | Pause, resume and cancel |
| SIM-RES-05 | simulation-analyses-and-results | tran with a voltage source `Vdd` | stored raw/display names `vdd#branch` / `i(vdd)` | Final vector retrieval |
| SIM-RES-06 | simulation-analyses-and-results | save modes `default`, `all`, `all+currents`, list on `buf` | vector lists | Saved vectors and memory budget |
| SIM-RES-07 | simulation-analyses-and-results | iPad job with 1000 saved vectors and budget 512 MiB | cancel point and diagnostic | Saved vectors and memory budget |
| SIM-RES-08 | simulation-analyses-and-results | completed job | folder listing; `job.json` content; `log.txt` prefixes | Result storage |
| SIM-RES-09 | simulation-analyses-and-results | Hefesto raw (binary and ASCII) of op, tran, ac plots | byte comparison with reference ngspice `write` output excluding Date/Command; reference ngspice `load` + `print` | Raw file format (Q-SIM-17) |
| SIM-RES-10 | simulation-analyses-and-results | CSV export of RC1 tran and an ac plot in `reim` and `dbphase` | file bytes | CSV export |
| SIM-RES-11 | simulation-analyses-and-results | import raw files from ngspice 30, 42, 47; padded/unpadded; multi-plot; truncated | imported plots/vectors; error texts | Raw file import |
| SIM-RES-12 | simulation-analyses-and-results | measurements on the triangle vector and on RC1 `v(out)` | numbers vs reference ngspice `meas` (`cross`, `avg`, `rms`, `trig/targ`) | Waveform measurements |
| SIM-UI-01 | simulation-ui-and-crossprobe | two corner jobs, one panel; frequency plot added to a time panel | legend texts; error text | Waveform viewer layout |
| SIM-UI-02 | simulation-ui-and-crossprobe | 0–20 ns panel; ac panel 1 Hz–1 GHz | tick labels; default axis modes | Axes, units and complex display |
| SIM-UI-03 | simulation-ui-and-crossprobe | 1 000 000-point trace with a single 1.8 V sample at 800 px | rendered image column max | Waveform rendering performance |
| SIM-UI-04 | simulation-ui-and-crossprobe | probe `inv_0/mid` in `buf` with modes `all` and `default`; probe a device current with `all+currents` (sky130A wrapped device and min-tech primitive) | vector names; error texts | Probing a layout net; Probing device currents (Q-SIM-6) |
| SIM-UI-05 | simulation-ui-and-crossprobe | op annotation of DIV-like layout | label texts; saved `.mag` contains no annotation | Operating-point annotation |
| SIM-UI-06 | simulation-ui-and-crossprobe | `rc` mode job on a metal wire with known w/t; cursor at 7.5 ns | overlay dataset values (I, Jw, J, interpolated V) | Data feed to 3D overlays |
| SIM-UI-07 | simulation-ui-and-crossprobe | saved view referencing a deleted job | restored placeholder text | Viewer state persistence |
| SIM-CMD-01 | simulation-command | `sim stat`, `sim s`, `sim frob`, `sim analysis add tran` (missing args) | results / error texts | Command and option matching |
| SIM-CMD-02 | simulation-command | the "Build and run a testbench" script | job id format; `sim status` result | Subcommand table; Return values and errors in scripts |
| SIM-CMD-03 | simulation-command | numbers `10ps 20NS 1meg 1m 1.8V 1x` | parsed values / error | Numeric argument syntax |
| SIM-CMD-04 | simulation-command | headless corner sweep script; same with one failing corner | exit status; files | Headless and batch use |
| SIM-CMD-05 | simulation-command | `sim bind A in; sim bind A clk; undo` | `sim bind A` result; `testbench.json` | Testbench persistence |
| SIM-XP-01 | simulation-engine-embedding | sky130A inverter-chain (5 stages, `c` mode) tran on all platforms | per-platform deviation statistics against Linux x86_64 | Cross-platform result consistency (Q-SIM-10) |
