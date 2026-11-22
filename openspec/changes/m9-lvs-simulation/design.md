# Design: M9 LVS, simulation and advanced analog simulation

> Drafted 2026-11-22 against origin/main 072f50c8. **No decision this design cites is approved yet.** Every choice is drafted as the recommended option of `docs/decisions/m9-decision-table.md`. If the user picks another option, the D-numbers listed in "Decisions this design depends on" are revised before gate G0 passes.

## Context

See proposal.md for the motivation and the current state of the code. The conventions of M0–M8 still hold:
- SPDX headers.
- Catch2 tags `[cap:<capability>][req:<exact requirement heading>]`.
- Per-area diagnostic tables `AREA-TOPIC-NNN` in `docs/diagnostics/codes/<area>.md`.
- Cognitive complexity of at most 15 per function in the core and 8–12 in the shell view models. The readers (`lvs/read`, `sim/result/raw`) may go to 25–35. Irreducible functions are flagged.
- No UI, GPU, Raven or process headers in the core (`scripts/check_core_includes.py`).
- Golden expected outputs are captured only in CI.
- Golden cases run with the compatibility settings ON, and the OFF behaviours are unit-tested with one test per mode.

What is new in M9:

1. **Two reference tools, neither of them Magic.**
   - netgen is GPL. Its `Copying` file is GNU GPL version 1 text. The `lvs-*` specs and `LVS.md` were written from its source, as the schematic specs were written from xschem's.
   - ngspice is Modified BSD, with LGPL parts (KLU, numparam) and an MPL part (OSDI). Hefesto runs it rather than reimplementing it.
   - Both have a licence boundary that the code layout has to make visible (D2).
2. **A long-lived worker process.** No earlier milestone ran one. The platform layer can only run a process to completion (`platform::ProcessRunner`).
3. **The first milestone that consumes two other milestones' outputs.**
   - M7: extraction and netlisting, through the entry points of m7 design D18.
   - M8: schematic netlists and decks, through m8 design D13.
   
   Both are still being implemented. M9 therefore orders its rounds so that most LVS and simulation work needs neither of them (D1).

Magic's own code in this area matters only at the edges:
- **The `ext2spice lvs` preset (M7).** M9 takes it from M7 as it is.
- **The `getnode` hierarchical naming that probes use (Q-SIM-24).** M9 brings the naming function forward from M12 without its fixed 256-byte and 30-byte buffers (`sim/SimExtract.c:357`, `:734-765`).
- **Magic's Tcl glue (`magic.tcl.in:94-151`).** It loads netgen and IRSIM into Magic's own interpreter. Hefesto deliberately does not do this (D2).

The netlist window and the IRSIM bridge (`rsim`, `startrsim`, `simcmd`) stay in M12, and UI-CLIENT-002 is not retired. The crashes and undefined behaviour visible in that Magic code are recorded for M12 and do not need M9 code:
- NULL `EditCellUse` dereferences in netmenu;
- the uninitialised `indx` in `NMWriteAll`;
- the dropped argument and buffer overflow in `simcmd`, and its `cleanName[-1]` read.

## Goals / Non-Goals

**Goals:**
- Every requirement of the 17 M9 capabilities (184).
- The M9 part of `schematic-simulation-integration` (m8 D13).
- The LVS goldens match netgen 1.5.327 under the canonical comparison of `lvs-reports` with `netgen-compat` ON.
- The SIM and SIMADV goldens match the patched reference ngspice within the tolerances of `simulation-engine-embedding` "Cross-platform result consistency".
- Simulation results are bit-identical between runs on one machine.
- `comp.out`, JSON and every non-numeric output are byte-identical across the three CI platforms.
- One branch point per `netgen-compat` entry, each tested in both modes.
- Every crash or undefined behaviour fixed in both modes.
- A licence boundary that is enforced by the build: no ngspice symbol in the editor, the CLI or the core, and no netgen anywhere in the product.
- The Desktop Beta flow on sky130, gf180mcu and IHP on Linux and macOS (Q-SIM-27).

**Non-Goals:** the items listed under "Non-goals" in proposal.md.

## Decisions

### D1. Rounds and gates

| Gate | Passes when | Unlocks |
|---|---|---|
| G0 decisions | The M9 decision table is approved and the approved answers are recorded as `Resolution:` lines and in `audit/DECISIONS.md`. This change's spec deltas are applied to the living specs. `check_milestone_readiness.py --milestone M9` reports "ready". | every other task |
| G1 LVS clean room | CLEANROOM tasks 2.1–2.4 are done: roles recorded, paraphrase review of the eight `lvs-*` specs and `LVS.md`, `check_cleanroom.py` covering netgen. | every LVS implementation task |
| G2 golden | The LVS, SIM and SIMADV cases are captured (roadmap principle 2). The reference runs of D-22, D-26 and D-27 are recorded, and the spec team has recorded the texts that depend on captures. | the matching implementation workstreams (LVS after the LVS capture, SIM after the SIM capture) |
| G3 M7 | M7 has exited, and `render_with_map` (Q-LVS-33) has landed. | LVSCMD layout source, SIMDECK layout source and parasitic modes, SIMPROBE layout probes |
| G4 M8 | M8 has exited, and `schem::NetlistResult::names` has landed. | SCHEMSIM, LVSCMD schematic source, SIMDECK schematic source |
| Exit | D25 | M9 done, Desktop Beta |

Rounds. Workstreams edit only what D3 assigns them.

| Round | Work | Needs |
|---|---|---|
| R0 | the decisions (tasks section 1) | nothing |
| R1 | CLEANROOM, PDK data sets, GOLDEN capture and comparators, NGBUILD, SCAFFOLD, PLATFORM `WorkerHost` | G0 |
| R2 | LVSREAD, LVSSETUP, LVSALG; SIMD and SIMENG (engine, jobs); SIMDECK for raw and SPICE-file sources; SIMRES | G1, G2 |
| R3 | LVSRED, LVSPROP, LVSHIER, LVSREP; SIMCMD; SIMADV and SIMMEAS on raw and SPICE-file testbenches; DESIGN boards | R2 |
| R4 | LVSCMD, SIMDECK layout and schematic sources, SIMPROBE, SCHEMSIM, SHELL, SECURITY | G3, G4 |
| R5 | GOLDEN SCHEM-SIM/LVS and the Beta end-to-end cases, PERF | R4 |
| R6 | INTEGRATION exit | R5 |

R1–R3 need neither M7 nor M8, because the LVS readers take SPICE files and the simulation engine takes decks. This is why design Open Question 2 recommends starting them as soon as G0 passes, without waiting for M7 or M8 to exit.

### D2. Licence boundary (Q-LVS-30, Q-SIM-20, -22, -26, D-28)

**netgen (GPL).**
- netgen is never linked, vendored, shipped or started by the product.
- Its only use is as the golden reference and the performance baseline. The capture and benchmark jobs run it as a subprocess in the pinned container (`tools/golden/goldenlib/refrunners.py`: `netgen -batch source <script>`).
- The `lvs-command` requirement "Execution model" (no fork or exec, no external netgen) stays as it is.
- Under Q-LVS-30 (a), the M8 two-team process applies to LVS:
  - **Spec team.** It owns the `lvs-*` specs, `LVS.md` and the texts that depend on captures. It may read netgen source and run netgen.
  - **Implementation team.** It writes `src/core/*/lvs/`, the tests and the golden inputs from the specs only. It never opens netgen's repository or the capture image's `/opt/src/netgen`, and it never follows `Evidence:` lines or `(netgen ref: ...)` traces.
  - **Allowed observation.** Running the netgen binary in the capture runner, and reading the public reference manual, are allowed for both teams.
  - **Paraphrase review.** The spec team reviews the eight specs and `LVS.md` for paraphrase before G1. The algorithm specs (`lvs-comparison-algorithm`, `lvs-device-reduction`) come first, because they describe internal procedures.
  - **`check_cleanroom.py`.** It gains netgen's GPL header, `netgen ref:` traces under `src/` and `tests/`, and the `provenance` rule for `tests/golden/LVS/`.
  - **Counsel.** Counsel review of the LVS process joins the M13 counsel item that already exists for J-1.
- `THIRD_PARTY_NOTICES.md` does not list netgen, because nothing of it is distributed. `hefesto-licensing` gains the requirement "Process boundary for external engines", which says so explicitly.

**ngspice on desktop (Q-SIM-20 (a)).**
- NGBUILD builds the pinned ngspice (Q-SIM-7) with the Hefesto patch series as one shared library, `libngspice-hefesto`, with the feature table of `simulation-engine-embedding`:
  - KLU, OSDI and XSPICE on, with the code models registered statically;
  - `table`, FFTW3, CIDER, `d_cosim` and readline off.
- `hefesto-simd` is a small executable. It is the only binary that links the library, and it finds the library through an install-relative runpath (`$ORIGIN` on Linux, `@loader_path` on macOS).
- `hefesto-simd` contains the engine adapter, the only translation unit that includes `sharedspice.h`, and the IPC server.
- The editor, `hefesto-cli` and the core library never link ngspice. A CI step (`scripts/check_engine_boundary.py`) fails if `nm` finds an `ngSpice_` symbol in any of them, or if a file outside `src/simd/adapter/` includes `sharedspice.h`.
- A user can replace `libngspice-hefesto` with a rebuilt one. The build manifest records the source, the patches and the configure line, which meets the LGPL without a relink kit. The OSDI code (MPL-2.0) ships as source in the manifest's source archive, as MPL requires for modified files.
- The in-process engine (Q-SIM-22 (a)) exists only in developer builds. It loads the same shared library through `dlopen` behind the same engine interface, and release builds compile it out.

**ngspice on iPadOS.**
- There is no process creation on iPadOS, so the engine must be in-process. Whether it is statically linked with a relink offer or loaded as an embedded signed framework is D-28's iPad part.
- Q-SIM-26 (a) splits D-28. The desktop part (Q-SIM-20) is decided in R0. The iPad part is decided before M14, with the legal review starting in M9.
- M9 builds no iPad engine. The engine interface is the same, so M14 adds only an `InProcessEngine` host.

**Third-party notices.** INTEGRATION adds rows to `THIRD_PARTY_NOTICES.md`, and `check_third_party_notices.py` verifies them:
- KLU, numparam, OSDI and sparse under ngspice (sparse already has a row);
- the compiled-in OSDI compact models (PSP103, the `cap_cmom*` modules; Q-SIM-2);
- OpenVAF, which is a build-time tool only and is not distributed.

### D3. Layout of the code, workstreams and ownership

There are two new core areas:
- `lvs`, with the diagnostic prefix `LVS-` and the code document `docs/diagnostics/codes/lvs.md`;
- `sim`, with the prefix `SIM-` and `docs/diagnostics/codes/sim.md`.

| Module | Path | Owner | Team |
|---|---|---|---|
| Netlist database, device classes | `lvs/net/` | LVSREAD | LVS impl |
| SPICE, CDL, Verilog, `.sim`, `.ext` readers | `lvs/read/` | LVSREAD | LVS impl |
| Setup-file commands in the sandbox | `lvs/setup/` | LVSSETUP | LVS impl |
| Graph, partition, refinement, symmetry, `verify` | `lvs/cmp/` | LVSALG | LVS impl |
| Series/parallel reduction, zero-valued removal | `lvs/reduce/` | LVSRED | LVS impl |
| Properties, tolerance, topology | `lvs/prop/` | LVSPROP | LVS impl |
| Compare queue, flattening, pin matching | `lvs/hier/` | LVSHIER | LVS impl |
| `comp.out`, list, JSON, classification | `lvs/report/` | LVSREP | LVS impl |
| `lvs`, `netgen` namespace, sources, result model, cross-probe | `lvs/cmd/` | LVSCMD | LVS impl |
| ngspice CMake wrapper, patches, manifest | `third_party/ngspice/` | NGBUILD | either |
| `hefesto-simd`, engine adapter, IPC server | `src/simd/` | SIMD | either |
| `WorkerHost` contract and hosts | `platform/worker_host.hpp`, `src/cli/`, desktop shell | PLATFORM | either |
| Engine interface, worker client, job manager | `sim/engine/` | SIMENG | either |
| Circuit sources, testbench, stimuli, descriptors, sanitisation | `sim/deck/` | SIMDECK | either |
| Result storage, raw, CSV, waveform measurements | `sim/result/` | SIMRES | either |
| Net name map, probes, op annotation, 3D feed, node naming | `sim/probe/` | SIMPROBE | either |
| `sim` command | `sim/cmd/` | SIMCMD | either |
| Run sets, sweeps, DOE, corners, Monte Carlo, seeds | `sim/runset/` | SIMADV | either |
| Measurements, specs, statistics | `sim/meas/` | SIMMEAS | either |
| Schematic M9 part | `schem/sim/`, `schem/cmd/xschem_command.cpp` (the simulation subcommands) | SCHEMSIM | M8 impl team (xschem clean room) |
| `render_with_map`, `NameMap` | `netlist/spice/`, `schem/net/` | MAPS | the M7 and M8 owners, after their exits |
| PDK data sets, model descriptors | `scripts/fetch_pdk.py`, `pdk/` | PDK | either |
| Golden inputs, capture, comparators | `tests/golden/{LVS,SIM,SIMADV}/`, `tools/golden/` | GOLDEN | LVS impl for LVS inputs |
| Specs, `LVS.md`, capture-fixed texts | `openspec/specs/lvs-*`, `docs/decisions/` | SPEC | LVS spec |
| Process, checks | `docs/contributing/cleanroom.md`, `scripts/check_cleanroom.py` | CLEANROOM | LVS spec |
| Boards | `docs/design/mockups/{LVS-Browser,Waveforms}.dc.html` | DESIGN | either |
| Desktop screens | `src/shell/desktop/src/{lvs_*,waveform_*,runset_*}.cpp` | SHELL | either |
| Security review | review record | SECURITY | reviewer outside both LVS teams |
| Benches | `bench/{lvs,sim}/`, `perf.yml` | PERF | either |
| Shared files, codes, exit | `diag/`, CMake lists, `editor_state.hpp`, docs | INTEGRATION | either |

"Either" means a session that is on neither LVS team; the simulation specs were written from ngspice, which is not GPL. Each core subdirectory has its own `sources.cmake` and `codes.inc`. Shared-file needs go to INTEGRATION as open items.

### D4. Session state and shared interfaces

- `EditorState` gains `lvs::Session` and `sim::Session`.
  - `lvs::Session` holds the last result per document, the run in progress and the name maps of its two circuits.
  - `sim::Session` holds the testbenches, the jobs, the run sets, the result store index and the engine status.
- `NameMap` (Q-LVS-33 (a)) is one value type in `netlist/name_map.hpp`, shared by M7's `render_with_map` and M8's `NetlistResult::names`. It has two parts:
  - `nets`: netlist name → `{hier_path, layout point | schematic wire}`;
  - `devices`: netlist name → `{use_path, location | schematic instance}`.
  
  It is ordered (a sorted vector) for deterministic iteration. LVS and simulation both read it, and neither recomputes names.
- LVS and simulation read the layout through the M7 entry points only:
  - `ensure_extracted`, then `render_with_map` with the `lvs` preset, or with the simulation settings passed by value;
  - `extresist::run` for mode `rc`.
  
  They never call `extract all` (Q-LVS-32) and never change the user's `ext2spice` settings.
- The layout database is read-only while extraction for LVS or simulation is in progress (`lvs-command` "Execution model"). Edits are queued, as the M5 jobs do.

### D5. Compatibility settings

- **`netgen-compat`** (D-2, default ON) governs every `Resolution: compat` entry of `LVS.md`. There are four: Q-LVS-7 (relative tolerance), Q-LVS-9 (parallel signature), Q-LVS-20 (symmetry verdict in `comp.out`) and Q-LVS-24 (placeholder test). The bugs that netgen crashes on or behaves undefined for (Q-LVS-3, -5, -12, -16, -18, -22, -23, -25) are fixed in both modes. Each compat entry has one branch point, implemented as one function in its owning module, and the setting is passed explicitly. Warnings that apply in both modes are emitted outside the branch. Golden cases run with the setting ON. `comp.out` carries no marker of the setting, because `hefesto-magic-compat-mode` specifies none. Hefesto's structured result records the setting's value.
- **`magic-compat`** touches M9 only through the node naming that M9 brings forward (Q-SIM-24). That naming has no length limit in both modes, so there is no branch point.
- **`xschem-compat`** governs only SCHEMSIM's netlist-related paths, through M8's existing branch points.
- **Simulation** has no compat setting: its capabilities are `native`.

### D6. LVS netlist input (LVSREAD; `lvs-netlist-input`)

- One netlist database (`lvs::NetlistDb`) holds cells, devices, nets, pins and properties. Classes are interned. Hand-written lexers read SPICE and CDL, structural Verilog, `.sim` and legacy `.ext`. `readnet ext` is kept for scripts and follows Q-LVS-1, with no GUI entry.
- Readers take a `Source {name, bytes | path}`, so in-memory layout and schematic netlists go through the same code as files ("In-memory netlist sources", delta).
- Includes resolve through the platform file service (Q-LVS-14), with the limits of Q-LVS-36.
- Name folding uses the unsigned-byte table of Q-LVS-3. Numbers are parsed with `std::from_chars` in SI form.

### D7. Setup files (LVSSETUP; `lvs-setup-file`)

- Setup files are Tcl, evaluated in the X-3 sandbox (`script/tcl/sandbox.hpp`) inside a fresh interpreter per run. The interpreter has the `netgen::` command set and the setup commands, and a read-only root over the PDK store and the document folder.
- Evaluation honours run cancellation, and the sandbox's step budget prevents a hang.
- The three PDK setup files are resolved by their installed names (Q-LVS-31): `sky130A_setup.tcl`, `gf180mcuD_setup.tcl` and `ihp-sg13g2_setup.tcl`. Each must evaluate with no error, which is checked by NG-PDK-01..03.
- `property add` follows Q-LVS-6: the single-word rule is copied, and a tolerance that is not a number raises an error.

### D8. Comparison, reduction, properties and hierarchy (LVSALG, LVSRED, LVSPROP, LVSHIER)

- **Comparison** follows the specified iterative refinement over the bipartite device/net graph. It is single-threaded per pair (Q-LVS-34 (a)), and the comparison state holds no globals.
- **Pseudo-random labels and unique class identities** (Q-LVS-4, Q-LVS-29 fix) come from a seeded SplitMix64 generator whose seed is fixed per pair. When labels collide, a structural-signature check splits the class, so no result depends on a hash collision.
- **Arbitrary pairing** is deterministic ("Deterministic arbitrary pairing").
- **Cancellation** is checked between refinement rounds ("Long-running comparisons"). Long reductions also poll it every 65 536 devices.
- **Reduction** is O(n log n) per class: devices are sorted by a terminal-net key before they are combined. Pin order and names are preserved.
- **Properties** are binary64 values parsed with `from_chars`. Tolerance arithmetic follows the spec, and values are printed in netgen's format through `to_chars`. A failed property comparison follows Q-LVS-10: netgen's code is copied, and the structured result carries the detail `property-check-failed`.
- **Hierarchy** is traversed iteratively with an explicit stack, depth at most 256 (Q-LVS-36). Pre-match flattening follows Q-LVS-11: it must behave as netgen does, and the NG-HIER cases validate it. Any divergence becomes a new entry.

### D9. Simulation engine, worker and platform service (NGBUILD, SIMD, PLATFORM, SIMENG; Q-SIM-7, -11, -20, -21, -22, D-25)

**Build.**
- `third_party/ngspice/` holds the pin (Q-SIM-7: the first ngspice release with the CMake build, carried with Hefesto patches) and the patch series:
  - the static OSDI registry;
  - static code-model registration;
  - portable seeding (`simulation-monte-carlo` "Portable random numbers");
  - the deterministic initialisation hooks.
- It also holds a CMake wrapper with the spec's feature table, `-ffp-contract=off` on every platform (Q-SIM-10), OpenMP off (Q-SIM-12; it is measured on macOS before any change), and a generated manifest (`ngspice-manifest.json`: version `<release>-hefesto<n>`, patches, licences).
- IHP OSDI modules are compiled from the PDK's Verilog-A sources with a pinned OpenVAF at build time and registered statically (`simulation-engine-embedding` "OSDI compact models").

**Worker.**
- `hefesto-simd` runs one engine instance and serves one job at a time.
- Its IPC protocol uses length-prefixed little-endian frames over stdin and stdout (Q-SIM-21 (a)). It starts with a handshake that carries the protocol and engine versions, then uses frames of these kinds:

| Frame | Direction | Meaning |
|---|---|---|
| `hello` | worker → core | engine version, protocol version |
| `load` | core → worker | deck lines |
| `cmd` | core → worker | `run`, `pause`, `resume`, `cancel`, `reset`, `quit` |
| `event` | worker → core | output line, progress, state |
| `vec` | worker → core | vector name, type, chunk of float64 values |
| `beat` | worker → core | heartbeat every 2 s |

Malformed frames, oversize frames (64 MiB; Q-SIM-25), unknown kinds and missed heartbeats (10 s) make the core terminate the worker. When that happens, the job fails with `simulation engine terminated`.

**Platform service.**
- `platform::WorkerHost` is a new contract in `hefesto-architecture` ("Long-lived worker processes", delta). `start(WorkerSpec) -> Outcome<std::unique_ptr<WorkerChannel>>`, and the channel offers `send`, an `on_frame` callback, `terminate` and an `on_exit` callback.
- The desktop shell and `hefesto-cli` implement it with `posix_spawn` and pipes. The core default reports it unsupported.
- `ProcessRunner` is unchanged.

**Fault policy (Q-SIM-11, D-25 (a)).**
- On desktop, every fault replaces the worker: a crash, a controlled exit, a timeout or a second error. No `ngSpice_Reset` is attempted.
- On iPadOS (M14) the engine is reset, as the spec says, and the reset is fuzz-tested in SIM-ENG-07.
- The job manager keeps the queue, and the next job starts in the new worker.

### D10. Threading

| Work | Thread | Rule |
|---|---|---|
| LVS run | one background worker (`hefesto-architecture` "Concurrency model") | pairs in queue order, single-threaded (Q-LVS-34) |
| Extraction for LVS or simulation | the M7 driver on the same background worker | the database is read-only meanwhile |
| Job manager | one core thread per session | owns the queue and the worker channels; never blocks the UI thread |
| Worker IO | one reader thread per worker channel | parses frames and posts copies to the job manager |
| ngspice | inside `hefesto-simd` (its own background thread) | callbacks copy data and write frames, never block (Q-SIM-15) |
| Run sets | pool of workers per Q-SIMADV-13 | results stored by point index |
| UI | UI thread | receives decimated streams and finished results by message |

- Results never depend on thread count. The LVS reports do not, and the stored simulation results do not either: each point has its own seed, and the worker is replaced or reset between points.
- The determinism tests run every run-set case with 1 worker and with 4 workers and compare the stored results bit for bit.

### D11. Deck preparation (SIMDECK; `simulation-netlist-preparation`)

**Circuit sources.**
- `layout <cell>`: D4's path, with parasitic modes `none`, `c` and `rc`. Mode `rc` calls `extresist::run` for cells whose `.res.ext` is missing or older than the `.ext` file.
- `schematic <file>`: `schem::build_deck` plus `NameMap`.
- `spice <file>`.
- `raw`: the deck is passed as written, after sanitisation.

**Model descriptors.**
- Model descriptors live in the PDK store, are bundled for the three PDKs and are verified against the pinned PDK sets (Q-SIM-8, D-26, D-27):
  - sky130 scans `libs.tech/combined`, and `ng_nomodcheck` is decided by a reference run;
  - gf180mcu follows the PDK's `ngbehavior=hs` unless a reference run shows a difference;
  - sg13g2 gets the OSDI set including `cap_cmomi` and `cap_cmomf`.
- Includes and `.lib` lines are rewritten against the PDK store (Q-SIM-13).

**sg13g2 mismatch flag.** D-23 (a): when a Monte Carlo mode includes mismatch, preparation adds `mm_ok=1` to the sg13g2 MOS instances of a layout DUT, inside the deck only. `ext2spice` output does not change ("sg13g2 mismatch flag on extracted devices", delta).

**Case collisions.** They are renamed `<name>__c<k>` (Q-SIM-6). The name map records each renamed name.

**`.control` sections.** They are stripped and blocked as specified (Q-SIM-9). The allowed subset on iPadOS is reviewed in M14.

**Limits.** Deck size and include depth are limited as in D18.

### D12. Analyses and results (SIMRES; `simulation-analyses-and-results`)

**Jobs.** A job follows the specified lifecycle. Progress combines the status strings with the streamed time point (Q-SIM-14).

**Vectors.**
- Vectors arrive from the worker in `vec` chunks and are stored in Hefesto-owned arrays. The final vectors are identical to what an in-process engine returns ("Chunked result transfer", delta).
- The memory budget is 512 MiB on iPadOS and 4 GiB on desktop (Q-SIM-16: kept as specified and revisited after the PERF profiles).

**Storage.** Results are stored per job under the project's `sim/` folder:
- raw files are always little-endian (Q-SIM-17);
- CSV export is supported;
- raw import is capped (D18).

**Waveform measurements.** They are computed in Hefesto with binary64 arithmetic.

### D13. Probing, operating point and 3D feed (SIMPROBE; `simulation-ui-and-crossprobe`)

**Layout node names.** Q-SIM-24 (a): SIMPROBE implements the `irsim-interface` requirement "Hierarchical node names", including preferred names and globals. Its tests are tagged to that requirement. The names have no length limit. The `getnode` command is not implemented.

**Probes.**
- A layout probe resolves a net through the name map, as specified.
- A schematic probe resolves a net through `NameMap` and `schem::vector_name`.
- Device-current probes use `@dev[i]` vectors when `savecurrents` is on.

**Operating point.** Annotation writes view-only overlays, never database labels. Magic's IRSIM path painted `RSIM=` labels into the root cell; Hefesto does not.

**3D feed.** It provides the voltage and resistor-edge current datasets of the requirement "Data feed to 3D overlays" as a read-only interface (`sim/probe/overlay_feed.hpp`), which M10 P3 consumes.

### D14. Run sets: sweeps, corners, Monte Carlo (SIMADV)

**Run sets.** A run set is a list of points over the dimensions sweep × corner × sample, generated in a fixed order:
- the corner is the outer loop;
- the sweep is lexicographic in declaration order;
- the sample comes last.

Each point is one job on the worker pool (Q-SIMADV-13 (a)), and results are stored by point index.

**Parameter overrides.** They follow D-22's reference run:
- if the last top-level `.param` wins in the pinned ngspice, overrides are appended after the last model line (Q-SIMADV-9 (b));
- otherwise only top-level text is changed, and definitions in included files are reported as "cannot sweep".

**Corners.**
- Corners come from the descriptor and the bundled tables. User corners and corner sets are supported.
- The corner matrix has pass and fail per spec, with "run all" and "re-run failed".

**Monte Carlo.**
- The modes are `process`, `mismatch` and `both`:
  - sky130 `both` per Q-SIMADV-3, verified by D-22;
  - gf180mcu explicit switches per Q-SIMADV-4;
  - no `both` mode for sg13g2, per Q-SIMADV-5.
- Process Monte Carlo at a non-typical corner is rejected unless the descriptor names a statistical counterpart (Q-SIMADV-8 (a)).
- Sample `k` uses seed `H(base_seed, k)` with the portable generator patch. Its result is identical whatever worker runs it and however many workers run ("Sample independence from worker assignment", delta).

**Waveform retention.** The modes `all`, `failing` and `measured` are available. `measured` is added by D-24, and it becomes the default above 100 points only after PERF has measured its storage cost (Q-SIMADV-11).

### D15. Measurements and statistics (SIMMEAS)

- Measurements, `.meas` import, expressions, specs, descriptive statistics, yield, Cpk, histograms, correlation and export, all as specified.
- All arithmetic is binary64, and summations use a fixed order (point index) with Neumaier compensation. The statistics are therefore identical across platforms and worker counts.
- Q-SIMADV-10 (a)+(b): the Hefesto value is authoritative. Debug builds also capture ngspice's `.meas` value and warn when the two differ by more than the tolerance (SIMADV-MEAS-04).

### D16. LVS reports, command and cross-probing (LVSREP, LVSCMD)

**Reports.**
- `comp.out` uses netgen's two-column layout.
- The structured list result and `comp.json` are written as specified, with Q-LVS-13's escaping and naming fixes and Q-LVS-19's values.
- The order of sections that netgen takes from hash order is deterministic, and the comparator treats those sections as sets (Q-LVS-2 fix).
- The extra `.` line is copied (Q-LVS-21).

**Layout references.** When circuit 1 is a layout, every net and device in the structured result carries the layout reference from the name map ("Layout object references in structured results", delta). `comp.out` and `comp.json` stay as netgen writes them.

**The `lvs` command.**
- The netgen form and `lvs -layout`.
- A `.sch` file on either side is netlisted in memory in LVS mode through `schem::netlist` (G4).
- Setup and inputs are resolved per Q-LVS-31.
- Results go to the result browser and cross-probing, and the command works headless.
- The `netgen` helper only names `lvs` in its diagnostic (D-34).

**Status chip.** The top-bar chip shows `LVS not run`, `LVS match`, `LVS mismatch (<n>)`, `LVS error` or `LVS stale`. It becomes `LVS stale` when the layout or the schematic changed after the run ("LVS status chip", delta).

### D17. Schematic simulation integration, M9 part (SCHEMSIM)

This work is taken over from m8 D13. Implementation sessions follow the M8 clean room: no xschem source, and the spec only.

| Requirement of `schematic-simulation-integration` | M9 work |
|---|---|
| Schematic circuit source | run the deck from `schem::build_deck` through SIMDECK's `schematic` source |
| Simulation launchers | `sim` calls run, and CORE-NOTIMPL is retired |
| Operating-point annotation | `@spice_get_*` values and `annotate_op` from the op result (SCHEM-SIM-02) |
| Embedded graphs | graph objects drawn from stored vectors |
| Waveform cross-probing | net ↔ trace through `vector_name` |
| Schematic to layout cross-probing | through the two `NameMap`s and an LVS match |
| Schematic-driven LVS command | `lvs` with a `.sch` side (D16) |
| Results interchange with xschem | raw read and write (`xschem raw ...`, `xschem simulate`) |

The exact outputs of `to_eng`, `tabulate` and the three `ngspice::` procedures are pinned from the SCHEM-SIM-02 capture by the M8 spec team (`schematic-symbols-and-libraries`).

### D18. Resource caps

Each cap has its own diagnostic and ends the run as `error` (LVS) or `failed` (simulation). Every cap is a setting that can be lowered, never raised past twice the default. The caps apply in both compat modes. The values are proposed in Q-LVS-36 and Q-SIM-25.

| Cap | Default | Where |
|---|---|---|
| LVS include depth | 64 | `lvs/read` |
| LVS logical line | 16 MiB | `lvs/read` |
| LVS name length | 4 KiB | `lvs/read` |
| LVS hierarchy depth (iterative) | 256 | `lvs/hier` |
| LVS devices per run | 50 million | `lvs/net` |
| Setup evaluation | sandbox step budget, cancellable | `lvs/setup` |
| Prepared deck | 256 MiB | `sim/deck` |
| Deck include depth | 32 | `sim/deck` |
| IPC frame | 64 MiB (vectors chunked) | `sim/engine`, `src/simd` |
| Worker start-up / heartbeat / timeout | 10 s / 2 s / 10 s | `sim/engine` |
| Result memory per job | 4 GiB desktop, 512 MiB iPad (Q-SIM-16) | `sim/result` |
| Points per run set | 100 000 | `sim/runset` |
| Monte Carlo samples | 100 000 | `sim/runset` |
| Raw import | 4 GiB and within the result budget | `sim/result` |
| Workers | `min(4, cores − 1, ⌊B_total / B_job⌋)` | `sim/runset` (Q-SIMADV-13) |

### D19. Golden-case plan (PDK, GOLDEN)

**Data sets.** `fetch_pdk.py` gains these sets, each with tree digests and cache keys like the M5 and M8 sets:
- `gf180mcu-netgen` and `ihp-netgen` (the setup files);
- `sky130-ngspice` (`libs.tech/combined` and `libs.tech/ngspice`; Q-SIMADV-2);
- `gf180mcu-ngspice`;
- `ihp-ngspice` (`libs.tech/ngspice`, plus the Verilog-A sources for OSDI).

**Capture image.** Under Q-SIM-23 (a):
- ngspice is built from the engine's pin with the Hefesto patch series and the spec's feature table, as both the executable and `--with-ngshared`;
- a pinned OpenVAF compiles the IHP OSDI modules;
- netgen stays at 1.5.327 @ `93dcda14`;
- `pins.toml` and its test are updated.

**Inputs.** Golden inputs are authored by Hefesto (LVS inputs by the LVS implementation team), or they are PDK files. Every `case.toml` carries `provenance`.

**Comparators** (new, Hefesto-written):

| Comparator | Compares |
|---|---|
| `tools/golden/compare/lvs_canonical.py` | `comp.out` (the hash-order sections of Q-LVS-2 as sets; everything else exact) and `comp.json` (parsed, the same set rule) |
| `tools/golden/compare/raw_tolerance.py` | raw files, ignoring `Date:` and `Command:` (Q-SIM-17), values within the cross-platform tolerances (Q-SIM-10) |
| `tools/golden/compare/meas_table.py` | measurement and statistics tables with per-column tolerances |

**Cases.**

| Area | Cases | Reference | Notes |
|---|---|---|---|
| LVS | 77 (ALG 7, CMD 1, HIER 9, NAME 3, PDK 3, PIN 6, PROP 11, READ 13, RED 6, REP 5, SET 13) | netgen | NG-PDK cases use the three setup files |
| SIM | 53 | patched ngspice, Magic for the DUT netlists | SIM-XP compares the three platforms |
| SIMADV | 40 in M9 (CO 8, MC 13, MEAS 9, SW 10) | patched ngspice | SIMADV-OPT (5) is M11 |
| SCHEM | SCHEM-SIM-01, -02, and SCHEM-LVS-01 under Q-SCHEM-39 (a) | xschem, ngspice, netgen | captures kept from M8 |
| Beta | BETA-E2E-SKY130, -GF180, -IHP | Hefesto run (end-to-end), checked against the step goldens | Q-SIM-27 |

**Reference runs before G2** (verification decisions, X-6):
- D-22: `.param` precedence, with one deck per PDK;
- D-26: sky130 `ng_nomodcheck`;
- D-27: gf180mcu `hs` against `hsa`.

Their results are recorded in `SIMADV.md` and `SIM.md` as `Verified` lines.

**Capture runs.** Capture is split into three runs (LVS, SIM, SIMADV) of `golden-capture.yml`, each verified by a second run on a fresh image build.

### D20. Fuzz targets and determinism

**Fuzz targets.** Corpora are seeded from Hefesto-authored inputs only.
- `fuzz_lvs_spice` (SPICE and CDL), `fuzz_lvs_verilog`, `fuzz_lvs_setup` (the setup commands through the sandbox);
- `fuzz_sim_raw` (raw import);
- `fuzz_sim_frames` (IPC frames as the core parses them, i.e. a hostile worker);
- `fuzz_sim_meas` (measurement expressions).

They join the nightly fuzz workflow.

**Determinism.**
- Numbers are parsed and printed with `from_chars` and `to_chars`.
- Iteration uses ordered containers only.
- The LVS generator is seeded per pair.
- The statistics use fixed-order compensated sums.
- ngspice is built with `-ffp-contract=off`.

A cross-platform digest test runs the LVS goldens and a set of decks on the three CI legs: LVS digests must be equal, and simulation results must be equal within tolerance.

### D21. Performance (PERF; Q-LVS-35, Q-SIM-28)

- **`lvs-bench`.** Callgrind instruction counts on the x86_64 runner for three workloads: the `sky130_fd_sc_hd` library cell by cell, a 50k-device flat block and a hierarchical 1M-device block. Each must be at most netgen's count, and after that at most the baseline + 2 %. Wall time is reported only.
- **`sim-bench`.** Hefesto's overhead (the whole job time minus ngspice's analysis time) must be at most 10 % on the inverter `tran` deck, a 1 000-device `rc` deck and a 1 000-point sweep. A 1 000-sample Monte Carlo must scale by at least 0.6 × the worker count up to 4 workers.
- **Waveform viewer.** The rendering requirement of `simulation-ui-and-crossprobe`, measured on the M3 Pro and the Linux runner.
- Baselines are recorded in `bench/lvs/` and `bench/sim/`. `hefesto-performance` gains the requirement "LVS and simulation throughput" (delta).

### D22. Security review (SECURITY; `docs/contributing/security.md`)

A reviewer outside both LVS teams checks four areas:
- **Setup files.** Tcl in the sandbox against `hefesto-security` "Script sandbox enforcement", using hostile setup files that try `exec`, `socket`, `open` outside the roots, `load`, `interp` and infinite loops.
- **Deck sanitisation.** `.control`, `shell`, `system`, includes outside the granted folders, and `.lib` path tricks.
- **The core's frame parser.** It is fed by `fuzz_sim_frames` and must handle a worker that sends malformed, oversize or slow frames.
- **Raw import.**

Each denial gives the sandbox diagnostic or the deck diagnostic. The sign-off is recorded in the M9 exit table, and M9 cannot exit without it.

### D23. Desktop (DESIGN, SHELL; design Open Question 3)

**Boards.** DESIGN draws two boards in the design system before SHELL starts:
- `LVS-Browser.dc.html`: pair list, mismatch groups, pin table, property errors, cross-probe actions;
- `Waveforms.dc.html`: viewer, cursors and readouts, trace list, run-set table, corner matrix, Monte Carlo histogram, spec and yield panel.

`docs/design/m9-ui-conformance.md` records the conformance checks.

**Shell changes.**
- The "LVS not run" chip becomes the status chip.
- The Simulate button opens the testbench panel. It is disabled with the tooltip `simulation engine unavailable: <reason>` when no worker can start.
- The Waveforms tab is enabled.
- Cross-probe highlights reuse the M2 highlight layer.
- Progress and cancel reuse the M5 panel.
- The stale "Waveforms ... M8" labels are corrected (`main_window.cpp:264-265`, `view_models.cpp:411`).

### D24. Changes to M1–M8 code

All of these go through INTEGRATION.
- **`netlist/spice/`:** `render_with_map` and `netlist/name_map.hpp` (MAPS, after M7 exit; additive; `render` unchanged).
- **`schem/net/`:** `NetlistResult::names` (MAPS, after M8 exit).
- **`schem/sim/`, `schem/cmd/`:** SCHEMSIM (D17).
- **`diag/`:** the `lvs` and `sim` areas and `CORE-NOTIMPL-009`. The code is retired at M9 exit and never reused.
- **`editor/editor_state.hpp`:** the two sessions.
- **`pdk/`:** the descriptor store.
- **`platform/`:** `worker_host.hpp`.
- **The desktop shell and `src/cli/`:** the `WorkerHost` hosts.
- **`src/core/src/script/pdk/commands.cpp:55-60`:** the stale UI-CLIENT-002 comment is corrected.
- **`docs/design/m1-ui-conformance.md`:** the LVS and Simulate rows.

### D25. Items deferred to M9 and the exit list

Taken over:
- **From M8 (m8 D13):** the six requirements and two partial requirements of `schematic-simulation-integration`; SCHEM-SIM-01/02 and, under Q-SCHEM-39 (a), SCHEM-LVS-01; `xschem simulate` and `xschem raw`; the exact outputs of `to_eng`, `tabulate` and `ngspice::`; the Waveforms tab.
- **From M7 (m7 D18):** the entry points, plus any change to them as an open item. The one change planned now is Q-LVS-33.
- **From M1:** the LVS chip and the Simulate button.
- **From the audit:** D-22 to D-28.

Exit criteria:
- The LVS, SIM and SIMADV (M9 part) goldens pass under D19's comparators. SCHEM-SIM-01/02 and SCHEM-LVS-01 pass.
- `check_requirement_coverage.py` covers every requirement of the 17 capabilities and the M9 part of `schematic-simulation-integration`.
- Unit, property and fuzz tests are green. LVS and non-numeric output are byte-identical on the three platforms, and simulation results agree within tolerance (D20).
- Each `netgen-compat` branch point is tested in both modes.
- `check_engine_boundary.py`, `check_cleanroom.py` and `check_third_party_notices.py` pass.
- The security sign-off of D22 is recorded.
- The PERF gates of D21 are met.
- The Desktop Beta end-to-end cases pass on Linux x86_64 and macOS arm64, and the user signs off the Beta checklist (Q-SIM-27).
- `openspec validate --all --strict` passes.

## Area contracts

### LVSREAD: `hefesto/lvs/net/`, `hefesto/lvs/read/`
`read_netlist(const Source&, Format, const ReadOptions&) -> diag::Outcome<CellId>` into a `NetlistDb&`. `ReadOptions` carries `netgen_compat`, the limits and the file service. `detect_format(const Source&) -> Format`.

### LVSSETUP: `hefesto/lvs/setup/`
`evaluate_setup(const Source&, SetupContext&) -> diag::Outcome<SetupState>`. `SetupContext` holds the two circuits, the sandbox factory and the cancel token. `SetupState` holds the permutes, properties, equates, ignores, models, globals and flatten lists.

### LVSALG, LVSRED, LVSPROP: `hefesto/lvs/cmp/`, `hefesto/lvs/reduce/`, `hefesto/lvs/prop/`
- `reduce(Cell&, const SetupState&) -> ReductionSummary`.
- `compare_pair(const Cell&, const Cell&, const SetupState&, CompareOptions) -> PairResult`. `CompareOptions` holds the seed, `netgen_compat` and the cancel token.
- `check_properties(const Matching&, const SetupState&) -> PropertyResult`.

### LVSHIER: `hefesto/lvs/hier/`
`run_lvs(Circuit, Circuit, const SetupState&, const RunOptions&, ProgressSink&) -> LvsResult`. The result holds the queue, the per-pair results and the final classification.

### LVSREP: `hefesto/lvs/report/`
`write_comp_out(const LvsResult&) -> std::string`, `write_json(const LvsResult&) -> std::string` and `to_list(const LvsResult&) -> script::Value`.

### LVSCMD: `hefesto/lvs/cmd/`
`register_lvs_commands(cmd::CommandRegistry&)` and `layout_circuit(EditorState&, CellId, Settings) -> diag::Outcome<Circuit>`. `layout_circuit` produces text plus `NameMap`. `cross_probe(const LvsResult&, ObjectRef) -> std::vector<LayoutRef>`.

### SIMENG: `hefesto/sim/engine/`
`class Engine` is the specified interface: `load`, `run`, `pause`, `resume`, `cancel`, `vectors` and callbacks. `WorkerEngine` implements it over `platform::WorkerHost`. `JobManager` is `submit(Deck, JobOptions) -> JobId`, with states, events and the queue.

### SIMD: `src/simd/`
- `adapter/ngspice_adapter.{hpp,cpp}` is the only file that includes `sharedspice.h`.
- `ipc/frames.{hpp,cpp}` defines the frames. It is shared with `sim/engine` as a header-only codec in `hefesto/sim/engine/frames.hpp`.

### PLATFORM: `hefesto/platform/worker_host.hpp`
`WorkerSpec {program, args, env}`, `WorkerHost::supported()`, `WorkerHost::start(const WorkerSpec&) -> diag::Outcome<std::unique_ptr<WorkerChannel>>` and `WorkerChannel::{send, on_frame, on_exit, terminate}`.

### SIMDECK: `hefesto/sim/deck/`
`prepare(const Testbench&, const DeckContext&) -> diag::Outcome<PreparedDeck>`, where `PreparedDeck` holds the lines, the `NameMap`, the descriptor and the diagnostics. `ModelDescriptor::load(pdk)`.

### SIMRES, SIMPROBE: `hefesto/sim/result/`, `hefesto/sim/probe/`
- `ResultStore` with `write_raw`, `read_raw`, `export_csv` and `measure_waveform`.
- `node_name(const LayoutSelection&) -> std::string`, which implements irsim-interface "Hierarchical node names".
- `probe(const Job&, const ProbeTarget&) -> diag::Outcome<TraceId>`.
- `OverlayFeed`.

### SIMADV, SIMMEAS: `hefesto/sim/runset/`, `hefesto/sim/meas/`
- `RunSet::generate(const RunSetSpec&) -> std::vector<Point>` and `execute(RunSet&, JobManager&)`.
- `sample_seed(base, k)`.
- `evaluate_measurements(const ResultStore&, const MeasSet&) -> MeasTable` and `statistics(const MeasTable&, const Specs&) -> StatsReport`.

### MAPS: `hefesto/netlist/name_map.hpp`
`NameMap {nets, devices}` (sorted). `netlist::spice::render_with_map(..., Settings) -> diag::Outcome<RenderedNetlist {text, names}>`. `schem::NetlistResult::names`.

## Risks / Trade-offs

- **R1. M7 and M8 slip.** Mitigation: R1–R3 need neither (D1), and their sources are SPICE files and decks. The layout and schematic sources come last.
- **R2. Spec gaps under the LVS clean room.** Mitigation: goldens first (G2). Spec issues go to the LVS spec team, with a turnaround target of one working day.
- **R3. Paraphrase in the algorithm specs.** Mitigation: they are reviewed first (D2). The NG-ALG and NG-RED goldens pin the behaviour independently of the wording.
- **R4. Simulation tolerance tuning.** Mitigation: `-ffp-contract=off`, then the SIM-XP cases re-tune the tolerances (Q-SIM-10). A case that needs a looser tolerance gets an approved deviation, never a silent change.
- **R5. Legal review changes Q-SIM-20.** Mitigation: the worker boundary stays. Only the link type of `hefesto-simd` would change, and the editor is unaffected.
- **R6. IPC bandwidth for large results.** Mitigation: chunked frames now. Shared memory behind the same frames if `sim-bench` shows transfer dominating.
- **R7. Reference-run surprises (D-22).** If ngspice keeps the first `.param`, the sky130 `both` mode and the sweep overrides move to Q-SIMADV-3 (b) and the Q-SIMADV-9 fallback. Both are drafted.
- **R8. Desktop Beta scope.** Mitigation: the Beta checklist is fixed in R0 (Q-SIM-27), so the exit has no open-ended items.

## Migration Plan

No existing user data changes. Projects gain a `sim/` result folder and testbench files (`simulation-command` "Testbench persistence"). Projects without them open as before. Desktop installs gain `hefesto-simd` and `libngspice-hefesto`. If either is missing, simulation is unavailable and every other feature works.

## Open Questions

None is decided. Each one's recommended option is (a).

1. **How spec changes are recorded.**
   - (a) *Recommended:* as this change's `specs/` deltas, applied to the living specs when G0 passes and validated with `openspec validate --all --strict`.
   - (b) Follow the M7 and M8 precedent: edit the living specs directly after approval and set `skip_specs: true`.
   - (c) Both: deltas for review, then a direct edit.
2. **When M9 implementation starts relative to M7 and M8.**
   - (a) *Recommended:* R0 and R1 as soon as the plan is approved. R2–R3 once G0–G2 pass, in parallel with M7 and M8 implementation. R4 after their exits (G3, G4).
   - (b) Start everything after M8 exits.
   - (c) Start R1 only, and plan the rest again after M7 exits.
3. **Desktop scope.**
   - (a) *Recommended:* DESIGN draws the LVS browser and waveform boards before SHELL, covering the run-set, corner and Monte Carlo views.
   - (b) Minimal panels with no boards.
   - (c) LVS browser and viewer only, with the run-set views in M10.
4. **Who owns the M7 and M8 additive APIs (`render_with_map`, `NetlistResult::names`).**
   - (a) *Recommended:* a MAPS workstream staffed by the M7 and M8 owners after each exit, reviewed by LVSCMD and SIMDECK.
   - (b) M9's LVSCMD and SIMDECK edit `netlist/spice` and `schem/net` directly.
   - (c) Add them to M7 and M8 before their exits.

## Decisions this design depends on

| Decision (all pending) | Recommended option | Design sections it shapes |
|---|---|---|
| Q-LVS-30 LVS clean room | (a) M8 two-team process | D1, D2, D3, D19 |
| Q-SIM-20, D-28 (desktop) | (a) shared `libngspice-hefesto` loaded only by `hefesto-simd` | D2, D9 |
| Q-SIM-21 worker IPC | (a) `WorkerHost`, framed pipes | D9, D10, D18 |
| Q-SIM-22 in-process desktop | (a) developer builds only | D2, D9 |
| Q-SIM-26 D-28 timing | (a) desktop in M9, iPad before M14 | D2 |
| Q-SIM-11, D-25 fault recovery | replace the worker on desktop | D9 |
| Q-SIM-7, Q-SIM-23 ngspice pin, reference build | first CMake release plus patches; patched reference with OpenVAF | D9, D19 |
| Q-LVS-31, -32, -33 | (a) | D4, D7, D16, D24 |
| Q-LVS-34, Q-SIMADV-13 threading, pool | (a) | D8, D10, D14 |
| Q-LVS-35, Q-SIM-28 performance | (a) | D21 |
| Q-LVS-36, Q-SIM-25 limits | configurable, the proposed defaults | D18 |
| Q-SIM-24 node naming | (a) naming forward, no `getnode` | D13 |
| Q-SIM-27 Beta sign-off | (a) three PDK flows, CI plus user | D19, D25 |
| Q-SCHEM-39 SCHEM-LVS-01 | (a) defer to M9 | D17, D19 |
| D-22, D-26, D-27 | reference runs | D11, D14, D19 |
| D-23 `mm_ok` | (a) simulation preparation adds it | D11 |
| D-24 `measured` retention | add it, default once measured | D14 |
| The 34 blocking Q-LVS, Q-SIM and Q-SIMADV entries | as `m9-decision-table.md` | D6–D16 |
