# DIGITAL golden test-case inventory

These cases cover the `digital-flow-*` capabilities (decision P-17). They are not captured by running Magic. Their fixtures come from LibreLane `librelane/librelane` @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (version 3.0.17), run once on Linux through Nix with sky130A (`sky130_fd_sc_hd`) at open_pdks commit `8afc8346a57fe1ab7934ba5a6056ea8b43078e71`, unless a case says otherwise. Fixture runs use the bundled examples `spm` and `spm-user_project_wrapper`. The run directories are stored as fixtures, so most cases need no LibreLane installation.

Cases marked **(live)** launch LibreLane and run only in the desktop integration test job. Cases marked **(fake)** use a fake `librelane` executable that writes a scripted run directory and exit status, so launch, progress and cancellation can be tested deterministically.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| DF-IMP-01 | digital-flow-block-import | `spm` run fixture; import `final/` | block manifest, list of stored views and corner folders | Import sources; Block storage and manifest |
| DF-IMP-02 | digital-flow-block-import | `MACROS` entry with `dir::` paths, gzipped GDS and LEF | block views; byte-identical copies | Import sources |
| DF-IMP-03 | digital-flow-block-import | `MACROS` entry with empty `lef` list | diagnostic; no block created | Import sources |
| DF-IMP-04 | digital-flow-block-import | LIB corner key `*_tt_025C_1v80` | stored folder name and manifest key | Block storage and manifest |
| DF-IMP-05 | digital-flow-block-import | manifest with path `../../etc/passwd`; manifest with `hefesto_block: 2` | rejection diagnostics; no reads outside the block | Block storage and manifest; Untrusted block views |
| DF-IMP-06 | digital-flow-block-import | GDS 0.2 µm wider than LEF `SIZE`; `pnl` module renamed | warnings listing each mismatch | Name and geometry consistency checks |
| DF-IMP-07 | digital-flow-block-import | `spm` block; switch full → abstract → undo | instance transform before and after; displayed geometry | Abstract and full representations |
| DF-IMP-08 | digital-flow-block-import | LEF bus pin `data[3]` with GDS label `data<3>`; LEF pin without GDS label | port names, indices, directions; warning | Pin mapping |
| DF-IMP-09 | digital-flow-block-import | paint into block cell; detach as `spm_edit` | error diagnostic; block unchanged; new editable cell | Read-only block cells |
| DF-IMP-10 | digital-flow-block-import | GDS write of a layout with the full `spm` block | output bytes of `spm` structure equal the source | Read-only block cells |
| DF-IMP-11 | digital-flow-block-import | modify a stored `lib` attachment, reopen | hash-mismatch warning | Attached views |
| DF-IMP-12 | digital-flow-block-import | open `def` view of `spm` | `spm__def` cell with net-name labels | DEF view inspection |
| DF-IMP-13 | digital-flow-block-import | update `spm` from a run with pin `b` removed | change list; instance warnings; undo restores | Block update |
| DF-IMP-14 | digital-flow-block-import | `MACROS` entry using `refg::` | diagnostic; import fails | Untrusted block views |
| DF-IMP-15 | digital-flow-block-import | project with block opened by the iPadOS build | block loads; import/update unavailable | Platform availability |
| DF-RUN-01 | digital-flow-run | register native install printing `3.0.17` for `--bare-version` (fake) | recorded version; untested-version prompt for `3.1.0` | Flow installation |
| DF-RUN-02 | digital-flow-run | new flow design `counter` (JSON) | `config.json` contents | Design directory and configuration file |
| DF-RUN-03 | digital-flow-run | edit `FP_CORE_UTIL` in `spm/config.yaml` | byte diff limited to that value | Configuration editor |
| DF-RUN-04 | digital-flow-run | add block `spm` to `MACROS` with instance `u_spm` | JSON entry text | Macros in the configuration |
| DF-RUN-05 | digital-flow-run | PDK store commit differs from `pdk_hashes.yaml` (fake) | warning text; both commits in `hefesto-run.json` | PDK selection through the PDK manager |
| DF-RUN-06 | digital-flow-run | fixed clock 2026-11-06 14:03:09, native install (fake) | argument vector; working directory; tag collision suffix | Launch |
| DF-RUN-07 | digital-flow-run | resume a cancelled fixture run (fake) | argument vector with existing tag; no new directory | Resuming a run |
| DF-RUN-08 | digital-flow-run | scripted run directory growing step by step (fake) | progress fractions after each step; current step id | Run directory layout; Progress reporting |
| DF-RUN-09 | digital-flow-run | fake exits 0 with `final/`; exits 1 with `final/` and deferred error; exits 1 without `final/` | statuses | Run status |
| DF-RUN-10 | digital-flow-run | fake ignores interrupt and termination | signal sequence with timestamps; final status | Cancellation |
| DF-RUN-11 | digital-flow-run | delete, keep and clean up runs; second run on same design | confirmations; refusal diagnostic | Run directory management |
| DF-RUN-12 | digital-flow-run | rerun after changing `src/spm.v` (fake) | `hefesto-run.json`; warning listing changed input | Reproducibility record |
| DF-RUN-13 | digital-flow-run | first run on a new design; agent-proposed run; headless without `--allow-external-flow`; process running as root | consent dialog shown; no process without approval; refusals | Consent and isolation |
| DF-RUN-14 | digital-flow-run | `flow run` on iPadOS build | diagnostic; no files written | Platform availability |
| DF-RUN-15 | digital-flow-run | `spm` full Classic flow (live) | status succeeded; `final/` present; metrics match fixture within the tolerances of `hefesto-output-determinism` | Launch; Run status |
| DF-RES-01 | digital-flow-results | `spm` fixture run; run started outside Hefesto | summary fields; unknown version | Run summary |
| DF-RES-02 | digital-flow-results | `metrics.json` with corner-qualified timing metrics | grouped table | Metrics model |
| DF-RES-03 | digital-flow-results | metrics with hold violation; missing KLayout metric | gate states | Metric evaluation |
| DF-RES-04 | digital-flow-results | 300 MiB synthetic `flow.log` | paged mode; peak memory | Log viewer |
| DF-RES-05 | digital-flow-results | open final layout of `spm` | top cell; read-only state | Opening the final layout |
| DF-RES-06 | digital-flow-results | `drc.magic.rpt` with two rules, duplicate boxes, interleaved `[INFO]` lines | parsed violations and multiplicities | Magic DRC report |
| DF-RES-07 | digital-flow-results | detailed-routing `.drc` with a short and a spacing violation; truncated record | parsed violations; diagnostic line number | OpenROAD routing DRC report |
| DF-RES-08 | digital-flow-results | `drc.klayout.lyrdb` with box, polygon, edge-pair and text values | parsed items | KLayout report databases |
| DF-RES-09 | digital-flow-results | `antenna.rpt` with three violations | sorted table | Antenna report |
| DF-RES-10 | digital-flow-results | `lvs.netgen.json` with one unmatched net | result-browser entries | LVS report |
| DF-RES-11 | digital-flow-results | zoom to a DRC violation; antenna net with and without final DEF | feedback area in internal units; highlighted net or no-location message | Mapping results to layout |
| DF-RES-12 | digital-flow-results | two `spm` runs differing in `FP_CORE_UTIL` | metric and configuration diffs | Comparing runs |
| DF-RES-13 | digital-flow-results | malformed report; results on iPadOS build | diagnostic; read-only browsing | Untrusted results and platform availability |
| DF-MS-01 | digital-flow-mixed-signal | place `spm` at (100, 50) µm orientation `S`; off-grid position; `MACROS` orientation `R90` | transform; refusal with nearest positions; mapping to `W` | Placing block instances |
| DF-MS-02 | digital-flow-mixed-signal | parent wire inside a 2 µm halo | halo warning | Placement halo and keep-out |
| DF-MS-03 | digital-flow-mixed-signal | supply pin on wrong net; shorted supplies; instance `u_spm.0` | supply-check report; `PDN_MACRO_CONNECTIONS` string | Supply hookup |
| DF-MS-04 | digital-flow-mixed-signal | extract `top` with `spm` instance | hierarchical SPICE with empty `spm` and call | Extraction with blocks |
| DF-MS-05 | digital-flow-mixed-signal | LVS of `top` with swapped block pins; block without metrics | LVS result; sign-off evidence line | LVS with black-boxed blocks |
| DF-MS-06 | digital-flow-mixed-signal | generate symbol for block with `data[7:0]` and `y` | symbol pin list and sides | Schematic symbols for blocks |
| DF-MS-07 | digital-flow-mixed-signal | export analog cell `ldo` | package tree; `macro.json`; `.vh` text byte for byte | Export for top-level digital flows; Verilog black-box module |
| DF-MS-08 | digital-flow-mixed-signal | export with off-grid port; zero-area port | errors; nothing written | Pre-export pin checks |
| DF-MS-09 | digital-flow-mixed-signal | `ldo` package consumed by `spm-user_project_wrapper` variant (live) | LibreLane run status; Magic DRC and Netgen LVS metrics | Export for top-level digital flows |
| DF-MS-10 | digital-flow-mixed-signal | LVS with a block on the iPadOS build | same result as desktop | Platform availability |
