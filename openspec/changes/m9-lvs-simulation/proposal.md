# Proposal: M9 LVS, simulation and advanced analog simulation (Desktop Beta)

> **Status: plan drafted 2026-11-22, with nothing approved yet.** The user approved planning M9 on 2026-11-22. The decisions the plan depends on are open:
> - the 34 entries reported by `check_milestone_readiness.py --milestone M9`;
> - audit decisions D-22 to D-28;
> - 18 entries this plan raises;
> - design Open Questions 1–4.
>
> `docs/decisions/m9-decision-table.md` lists each one with a recommended option. Gate G0 (design D1) passes only when the user has approved them and the readiness check reports "ready".

## Why

Desktop Beta is the first release that carries a complete analog flow: schematic → simulation → layout → DRC → GDS → extraction → LVS → post-layout simulation, on Linux and macOS (`docs/roadmap.md`, M9). Earlier milestones built every step except two. Hefesto cannot compare a layout against its schematic, and it cannot simulate. Product decisions P-5 (built-in LVS), P-7 (embedded ngspice) and P-19 (advanced analog simulation) define those two steps. The roadmap assigns them to M9 as 17 capabilities with 184 requirements.

What exists today (origin/main at 072f50c8):
- **Specs.** There are eight `lvs-*` capabilities (87 requirements) and nine `simulation-*` capabilities (97 requirements). The LVS specs were written from netgen 1.5.327 @ `93dcda14`, and their open questions cite netgen source. The simulation specs were written from ngspice @ `7aa47164`. `simulation-optimization-hooks` belongs to M11.
- **Open decisions.** These entries are unresolved:
  - 13 Q-LVS, 15 Q-SIM and 6 Q-SIMADV entries;
  - the draft-only Q-SIM-1;
  - the remaining part of Q-SIMADV-5 (`mm_ok`);
  - audit decisions D-22 to D-28, each marked "blocks M9".
  
  The roadmap and the audit disagree on when D-28 (how ngspice's LGPL and MPL parts are linked) must be settled: the roadmap says "before M14", the audit says "blocks M9".
- **Core.** There is no LVS or simulation code. The pieces M9 builds on exist as stubs:
  - The M7 entry points (`extract::ensure_extracted`, `netlist::spice::render`, `extresist::run`) are stubs that return `CORE-NOTIMPL-008`.
  - M8's `schem::netlist` is in progress, and `schem::build_deck` is a stub that returns `CORE-NOTIMPL-007`.
  - Neither returns the name map from netlist names back to layout or schematic objects that cross-probing needs.
  - `platform::ProcessRunner` can only run a process to completion; it cannot host a long-lived worker.
  - The `netgen-compat` setting exists, with default ON.
  - The scaffold code `CORE-NOTIMPL-009` is free.
- **Desktop.** The top bar paints a disabled "LVS not run" chip and a disabled Simulate button with M9 tooltips (`view_models.cpp:180`, `:419`; `main_window.cpp:284`). The "Waveforms" tab still says M8 (`main_window.cpp:264-265`). There are no LVS or waveform boards in `docs/design/mockups/`.
- **Golden.** `tests/golden/LVS/CASES.md` (77 cases), `SIM/CASES.md` (53) and `SIMADV/CASES.md` (45, of which 5 are M11) are inventories only; nothing is captured yet. The capture image builds netgen and stock ngspice, but:
  - it has no Hefesto patches, no OpenVAF and no PDK model data;
  - `tools/golden/compare/` has no `comp.out`, JSON or raw-file comparator.
  
  SCHEM-SIM-01/02 are deferred to M9. SCHEM-LVS-01 is still marked `deferred = "M8"`, but it needs LVS.
- **PDK data.** Only the `sky130-xschem` set carries `libs.tech/netgen`. No ngspice model tree, gf180mcu or IHP netgen setup file, or IHP OSDI source is fetched.

## What Changes

- **Decisions first (R0).** The user decides every row of the M9 decision table. The approved answers become `Resolution:` lines, and the spec changes they imply are applied as this change's spec deltas (design Open Question 1).
- **LVS clean room (CLEANROOM).** The two-team process of `hefesto-licensing` is extended to netgen (Q-LVS-30):
  - roles are assigned;
  - the eight `lvs-*` specs and `LVS.md` get a paraphrase review;
  - `check_cleanroom.py` learns netgen's GPL header and the `netgen ref:` traces;
  - golden inputs are Hefesto-authored.
- **Reference data and capture (PDK, GOLDEN).** This work comes before any implementation:
  - New PDK data sets: the netgen setup files of gf180mcu and IHP, and the ngspice model trees of the three PDKs (sky130 `libs.tech/combined`, gf180mcu and IHP `libs.tech/ngspice`, the IHP Verilog-A sources).
  - The capture image builds the patched reference ngspice and adds OpenVAF (Q-SIM-23).
  - New comparators for `comp.out`, `comp.json`, raw files and measurement tables.
  - The LVS, SIM and SIMADV cases are captured.
  - D-22, D-26 and D-27 are settled with reference runs.
- **Engine build and worker (NGBUILD, SIMD, PLATFORM).**
  - ngspice is pinned (Q-SIM-7) and built by a CMake wrapper with the Hefesto patch series.
  - The resulting `libngspice-hefesto` is loaded only by `hefesto-simd` on desktop (Q-SIM-20).
  - The engine adapter in `hefesto-simd` is the only code that includes `sharedspice.h`.
  - A new platform service `platform::WorkerHost` carries framed IPC with heartbeats and replaces the worker after any fault (Q-SIM-21, D-25).
- **LVS core (LVSREAD, LVSSETUP, LVSALG, LVSRED, LVSPROP, LVSHIER, LVSREP).** These implement the netlist readers, the setup-file commands in the X-3 sandbox, the comparison algorithm, reduction, property matching, the hierarchical flow and the netgen-compatible reports. Every quirk sits behind one `netgen-compat` branch point, and every crash or undefined behaviour is fixed in both modes.
- **LVS command (LVSCMD).**
  - `lvs` and the `netgen` namespace.
  - The layout source through `ensure_extracted` and `render_with_map` (Q-LVS-32, Q-LVS-33).
  - The schematic source through `schem::netlist`.
  - The result model, the result browser, cross-probing and headless LVS.
- **Simulation front end (SIMDECK, SIMRES, SIMPROBE, SIMCMD).**
  - Circuit sources: layout, schematic and raw.
  - Parasitic modes, the net name map, testbench composition, model descriptors in the PDK store and deck sanitisation.
  - Jobs, results, raw and CSV files.
  - Probing, with the layout node naming brought forward from `irsim-interface` (Q-SIM-24).
  - Operating-point annotation, the 3D overlay feed for M10, and the `sim` command.
- **Advanced analog simulation (SIMADV, SIMMEAS).** Run sets, sweeps and design of experiments, the corner matrix, Monte Carlo and mismatch with portable seeds, measurements, specs, yield, Cpk, histograms and correlation. Run sets use a memory-aware worker pool (Q-SIMADV-13).
- **Schematic integration, M9 part (SCHEMSIM, implementation team under M8's clean room).** The parts of `schematic-simulation-integration` that m8 design D13 deferred:
  - running a testbench and the full launchers;
  - operating-point annotation and embedded graphs;
  - waveform and layout cross-probing;
  - schematic-driven LVS;
  - raw interchange with xschem;
  - `xschem simulate` and `xschem raw`;
  - the exact outputs of `to_eng`, `tabulate` and `ngspice::`.
- **Desktop (DESIGN, SHELL).**
  - New boards for the LVS result browser and the waveform viewer, including the run-set, corner and Monte Carlo views (design Open Question 3).
  - The LVS chip and the Simulate button are enabled, and the Waveforms tab is relabelled.
  - Cross-probing works on the canvas.
- **Security (SECURITY).** A review covers three areas: setup-file Tcl in the sandbox, deck sanitisation (`.control`, `shell`, includes), and how the core parses frames from a worker that might be hostile.
- **Exit (PERF, INTEGRATION).**
  - LVS and simulation performance gates (Q-LVS-35, Q-SIM-28) and fuzz targets.
  - Requirement coverage.
  - The Desktop Beta flow on three PDKs, with the user's sign-off (Q-SIM-27).
  - Third-party notices and the M9 exit check.

### Non-goals

- Linking or shipping netgen, or reading its source in the implementation team. netgen is a golden reference only, run as a subprocess in the capture image.
- The iPadOS simulation engine build: in-process static or framework linking (D-28 iPad part), background behaviour and the iPad viewer. M9 keeps the adapter boundary that iPadOS needs; the build itself is M14.
- `simulation-optimization-hooks` and the five SIMADV-OPT cases (M11).
- The netlist window, IRSIM (`rsim`, `startrsim`, `simcmd`) and the `getnode` command (M12; UI-CLIENT-002 stays). M9 brings forward only the hierarchical node naming that probes need (Q-SIM-24).
- Xyce, Spectre or any other simulator. The xschem procedure `sim_is_xyce` keeps returning 0.
- `readnet ext` in the GUI (Q-LVS-1).

## Capabilities

### New Capabilities

None.

### Modified Capabilities

M9 implements the existing requirements of its 17 capabilities. Unlike M7 and M8, which applied approved decisions to the living specs before their change was created (`skip_specs: true`), this change carries spec deltas, because M9's decisions are not approved yet. Design Open Question 1 asks whether to keep that approach.

The deltas under `specs/` record the changes the recommended answers imply:
- `lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports`, `lvs-command`: limits (Q-LVS-36), in-memory sources, determinism and threading (Q-LVS-34), the IHP setup file (Q-LVS-31), incremental extraction and the name map (Q-LVS-32, -33), layout references in results, and the LVS status chip.
- `simulation-engine-embedding`: the desktop shared library and worker (Q-SIM-20, -21, -22, D-25) and the IPC limits (Q-SIM-25).
- `simulation-netlist-preparation`: `mm_ok=1` for extracted sg13g2 devices (D-23) and deck limits.
- `simulation-analyses-and-results`: chunked result transfer and raw golden comparison (Q-SIM-17).
- `simulation-ui-and-crossprobe`: probe naming with no length limit (Q-SIM-24) and the Simulate button.
- `simulation-command`: schematic launchers.
- `simulation-sweeps`: the `measured` retention mode (D-24) and point-index result order.
- `simulation-corners`: corners on the worker pool.
- `simulation-monte-carlo`: samples that do not depend on which worker ran them, and the sample cap.
- `simulation-measurements-and-statistics`: the `.meas` cross-check in debug builds (Q-SIMADV-10).
- `hefesto-licensing`: the process boundary for external engines.
- `hefesto-architecture`: the long-lived worker processes service.
- `hefesto-performance`: LVS and simulation throughput.

M9 also implements the M9 part of `schematic-simulation-integration` (m8 design D13) and the `netgen-compat` parts of `hefesto-magic-compat-mode`.

## Impact

- **New code (core):**
  - `src/core/{include/hefesto,src}/lvs/{net,read,setup,cmp,reduce,prop,hier,report,cmd}/`
  - `src/core/{include/hefesto,src}/sim/{engine,deck,result,probe,cmd,runset,meas}/`
  - `src/core/include/hefesto/platform/worker_host.hpp`
  - additions to `schem/sim/` and `schem/cmd/`
  - `tests/unit/{lvs,sim}/`
  - `tests/fuzz/{fuzz_lvs_spice,fuzz_lvs_verilog,fuzz_lvs_setup,fuzz_sim_raw,fuzz_sim_frames,fuzz_sim_meas}.cpp`
- **New code (worker and build):** `src/simd/` (the `hefesto-simd` executable and engine adapter), `third_party/ngspice/` (CMake wrapper, patch series, build manifest), and the `WorkerHost` hosts in `src/cli/` and the desktop shell.
- **New code (desktop):** `src/shell/desktop/src/{lvs_*,waveform_*,runset_*}.cpp`.
- **Shared changes:**
  - M7's `netlist::spice` gains `render_with_map`, and `schem::NetlistResult` gains `NameMap` (Q-LVS-33; additive, after M7 and M8 exit).
  - `EditorState` gains `lvs::Session` and `sim::Session`.
  - `diag` gains the `lvs` and `sim` areas and `CORE-NOTIMPL-009`.
  - The PDK manager gains the simulation model descriptor store.
- **Tooling:**
  - `fetch_pdk.py` gets the netgen and ngspice data sets.
  - The capture image gets the patched ngspice and OpenVAF.
  - New comparators: `tools/golden/compare/{lvs_canonical,raw_tolerance,meas_table}.py`.
  - `check_cleanroom.py` covers netgen.
  - New benchmark jobs: `lvs-bench`, `sim-bench`.
- **Golden:**
  - 77 LVS, 53 SIM and 40 SIMADV cases, plus SCHEM-SIM-01/02 and SCHEM-LVS-01.
  - Three Desktop Beta end-to-end cases.
- **Docs:**
  - `docs/diagnostics/codes/{lvs,sim}.md`;
  - `docs/design/m9-ui-conformance.md`;
  - `docs/contributing/cleanroom.md` (netgen);
  - `docs/building.md` (ngspice, OpenVAF, data sets);
  - `THIRD_PARTY_NOTICES.md` (ngspice parts, OSDI models);
  - the roadmap status and the CHANGELOG.

## Dependencies

| Needs | From |
|---|---|
| Decision table approved, readiness "ready" | gate G0 (R0) |
| LVS clean-room roles and paraphrase review | CLEANROOM, before any LVS implementation (gate G1) |
| LVS, SIM, SIMADV goldens captured (roadmap principle 2) | GOLDEN, section 3 (gates G2-LVS and G2-SIM) |
| `ensure_extracted`, `render` (+ `render_with_map`), `extresist::run` working | M7 exit, plus the additive API of Q-LVS-33 (gate G3) |
| `schem::netlist` (+ `NameMap`), `schem::build_deck` working | M8 exit (gate G4) |
| ngspice desktop linking decided | Q-SIM-20 / D-28 desktop part (R0) |
| iPadOS linking of LGPL and MPL parts | D-28 iPad part, before M14 (Q-SIM-26) |
| Results feed for 3D overlays | consumed by M10's P3 current-density overlay (`openspec/changes/m10-3d-view/design.md` P3, task 15.4) |
