# Design: M9 LVS, simulation and advanced analog simulation

> Drafted 2026-11-22 against origin/main 072f50c8. **Approved 2026-11-22.** The user approved every decision this design cites with the recommended option of `docs/decisions/m9-decision-table.md` (G0, task 1.1); the four verify rows are approved as conditional rules (D1). The spec deltas were synced into `openspec/specs/` at G0 (task 1.2, Open Question 1 (a)), so the change no longer carries a `specs/` folder.

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
- **The `getnode` hierarchical naming that probes use (Q-SIM-24).** M9 brings the naming function forward from M12 without the fixed buffers of `SimGetNodeName`: `strcpy`/`strcat` into `static nodename[256]` and `buff[256]` overflow on long paths or labels (`sim/SimExtract.c:734-767`). `SimTxtorLabel`'s `name[30]` (`:357-366`) cannot overflow: `@=%c%d,%d` with two 32-bit integers is at most 26 characters plus the terminator.
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
| G0 decisions | The M9 decision table is approved and the approved answers are recorded as `Resolution:` lines and in `audit/DECISIONS.md`. The four **verify** rows (Q-SIM-8, Q-SIMADV-3, -4, -9) get conditional `Resolution:` lines that name the option for each outcome of their reference run ("verify: (a) if the D-22 run shows the later `.param` taking effect, otherwise (b)"); their `Verified` lines are added at G2-SIM. This change's spec deltas are applied to the living specs. `check_milestone_readiness.py --milestone M9` reports "ready". | every other task |
| G1 LVS clean room | CLEANROOM tasks 2.1–2.4 are done: roles recorded, paraphrase review of the eight `lvs-*` specs and `LVS.md`, `check_cleanroom.py` covering netgen. | LVS golden inputs (task 3.5) and every LVS implementation task |
| G2-LVS golden | The LVS cases are captured (roadmap principle 2), and the spec team has recorded the LVS texts that depend on captures. | the LVS implementation workstreams |
| G2-SIM golden | The SIM and SIMADV cases are captured. The reference runs of D-22, D-26 and D-27 have added the `Verified` lines of the four verify rows (task 3.3), and the spec team has recorded the simulation texts that depend on captures. | the simulation implementation workstreams |
| G3 M7 | M7 has exited, and `render_with_map` (Q-LVS-33) has landed. | LVSCMD layout source, SIMDECK layout source and parasitic modes, SIMPROBE layout probes |
| G4 M8 | M8 has exited, and `schem::NetlistResult::names` has landed. | SCHEMSIM, LVSCMD schematic source, SIMDECK schematic source |
| Exit | D25 | M9 done, Desktop Beta |

A verify row is approved at G0 as a rule ("option X if the run shows Y"), not as a guess. If a reference run shows an outcome that no branch of the approved rule covers, the result goes back to the user before G2-SIM passes.

Rounds. Workstreams edit only what D3 assigns them. The LVS and simulation streams are gated separately, so neither waits for the other's capture or clean room.

| Round | LVS stream | Simulation stream | Shared |
|---|---|---|---|
| R0 | — | — | the decisions (tasks section 1); needs nothing |
| R1 | CLEANROOM (G0); then LVS golden inputs and capture (after G1) | NGBUILD → GOLDEN capture image → reference runs → SIM/SIMADV inputs and capture (the G2-SIM critical path, below) | PDK data sets, GOLDEN comparators, SCAFFOLD, PLATFORM `WorkerHost` (G0) |
| R2 | LVSREAD, LVSSETUP, LVSALG (G1, G2-LVS) | SIMD and SIMENG; SIMDECK for raw and SPICE-file sources; SIMRES (G2-SIM) | — |
| R3 | LVSRED, LVSPROP, LVSHIER, LVSREP (LVS R2) | SIMCMD; SIMADV and SIMMEAS on raw and SPICE-file testbenches (simulation R2) | DESIGN boards |
| R4a | MAPS → LVSCMD (G3, G4) | MAPS → SIMDECK layout and schematic sources (G3, G4) | SECURITY |
| R4b | — | SIMPROBE (after SIMDECK 13.4); SCHEMSIM 16.1–16.2 (after SIMDECK 13.4) | SCHEMSIM 16.3 (after LVSCMD 12); SHELL (after LVSCMD 12.3 and SIMPROBE 14.2) |
| R5 | | | GOLDEN SCHEM-SIM/LVS and the Beta end-to-end cases, PERF (R4b) |
| R6 | | | INTEGRATION exit (R5) |

**R1 order.**
- Simulation, the G2-SIM critical path: NGBUILD 5.1 (pin and patch series) → GOLDEN 3.2 (capture image built from that series) → 3.3 (reference runs) and 3.6 (SIM and SIMADV inputs) → 3.7 (SIM and SIMADV capture) → 3.8.
- LVS: CLEANROOM 2.1–2.2 → GOLDEN 3.5 (LVS inputs, by the LVS implementation team) → 3.7 (LVS capture) → 3.8. 2.3–2.4 finish before G1 but do not hold up 3.5.
- SCAFFOLD 4.2 writes the area-contract headers except the PLATFORM-owned `platform/worker_host.hpp` and the SIMD-owned frame codec `sim/engine/frames.hpp`, which PLATFORM 5.3 and SIMD 5.4 write.

R1–R3 need neither M7 nor M8, because the LVS readers take SPICE files and the simulation engine takes decks. This is why design Open Question 2 recommends starting them as soon as G0 passes, without waiting for M7 or M8 to exit.

### D2. Licence boundary (Q-LVS-30, Q-SIM-20, -22, -26, D-28)

**netgen (GPL).**
- netgen is never linked, vendored, shipped or started by the product.
- Its only use is as the golden reference and the performance baseline. The capture and benchmark jobs run it as a subprocess in the pinned container (`tools/golden/goldenlib/refrunners.py`: `netgen -batch source <script>`).
- The `lvs-command` requirement "Execution model" (no fork or exec, no external netgen) stays as it is.
- The approved `hefesto-licensing` requirement "Two-team clean-room process for specs written from GPL source" already applies: it covers every Hefesto spec "written by reading GPL source", and the `lvs-*` specs were. Q-LVS-30 decides only how it is applied to netgen. A MODIFIED delta adds the eight `lvs-*` capabilities, `LVS.md` and the `(netgen ref: ...)` traces to the requirement's list:
  - **Spec team.** It owns the `lvs-*` specs, `LVS.md` and the texts that depend on captures. It may read netgen source and run netgen.
  - **Implementation team.** It writes `src/core/*/lvs/`, the tests and the golden inputs from the specs only. It never opens netgen's repository or the capture image's `/opt/src/netgen`, and it never follows `Evidence:` lines or `(netgen ref: ...)` traces.
  - **Roster rule.** A session that read netgen source is on the LVS spec team for the whole milestone. It does not work on LVSCMD or on SCHEMSIM 16.3 (schematic-driven LVS), which implement LVS behaviour.
  - **Allowed observation.** Running the netgen binary in the capture runner, and reading the public reference manual, are allowed for both teams.
  - **Paraphrase review.** The spec team reviews the eight specs and `LVS.md` for paraphrase before G1. The algorithm specs (`lvs-comparison-algorithm`, `lvs-device-reduction`) come first, because they describe internal procedures.
  - **`check_cleanroom.py`.** It gains netgen's GPL header, `netgen ref:` traces under `src/` and `tests/`, and the `provenance` rule for `tests/golden/LVS/`.
  - **Counsel.** Counsel review of the LVS process joins the counsel review the requirement already demands before Desktop 1.0 (the M13 item that exists for J-1).
- `THIRD_PARTY_NOTICES.md` does not list netgen, because nothing of it is distributed. `hefesto-licensing` gains the requirement "Process boundary for external engines", which says so explicitly.

**ngspice on desktop (Q-SIM-20 (a)).**
- NGBUILD builds the pinned ngspice (Q-SIM-7) with the Hefesto patch series as one shared library, `libngspice-hefesto`, with the feature table of `simulation-engine-embedding`:
  - KLU, OSDI and XSPICE on, with the code models registered statically;
  - `table`, FFTW3, CIDER, `d_cosim` and readline off.
- The engine adapter is its own plugin, `hefesto-ngspice-adapter`: a shared module built from `src/simd/adapter/`, the only code that includes `sharedspice.h`. It links `libngspice-hefesto` and exports only Hefesto's engine interface as a C ABI with no ngspice types. Both libraries are found through an install-relative runpath (`$ORIGIN` on Linux, `@loader_path` on macOS).
- `hefesto-simd` is a small executable: the IPC server and the heartbeat thread. It loads the adapter plugin at start-up and has no ngspice symbol itself.
- The in-process engine (Q-SIM-22 (a)) exists only in developer builds. The developer desktop host loads the same adapter plugin behind the same engine interface, so no adapter code is compiled into the editor. Release builds compile the in-process host out and never load the plugin into the editor.
- The editor, `hefesto-cli` and the core library never link ngspice or the adapter plugin. A CI step (`scripts/check_engine_boundary.py`) fails if `nm` finds an `ngSpice_` symbol in any binary other than `hefesto-ngspice-adapter` and `libngspice-hefesto`, if a release binary other than `hefesto-simd` references the adapter plugin, or if a file outside `src/simd/adapter/` includes `sharedspice.h`.
- **Replaceability.** The build manifest records the source, the patches and the configure line, so a user can rebuild `libngspice-hefesto`. On macOS a notarized, hardened-runtime `hefesto-simd` with library validation would refuse a library the user signed, so `hefesto-simd` (and only it) carries the `com.apple.security.cs.disable-library-validation` entitlement, and `docs/building.md` documents ad-hoc signing of a rebuilt library. The worker's sandbox (D9) bounds what that entitlement exposes.
- **Legal review.** D-28 recommends a legal review before the linking is settled. The desktop linking goes through that review before the Desktop Beta (task 20.1). The plan states the intended LGPL position (a replaceable shared library loaded by a separate process); the review confirms it or changes the link type (risk R5).
- **MPL-2.0.** The source archive of each release carries the source of every MPL-covered file that is built, the whole `src/osdi` tree as built and not only modified files, as MPL-2.0 section 3.2 requires for the executable form.

**ngspice on iPadOS.**
- There is no process creation on iPadOS, so the engine must be in-process. Whether it is statically linked with a relink offer or loaded as an embedded signed framework is D-28's iPad part.
- Q-SIM-26 (a) records when each part of D-28 is settled. The desktop part (Q-SIM-20) is decided in R0 and reviewed before the Desktop Beta. The iPad part is decided before M14, as D-28 already requires, after the same legal review.
- M9 builds no iPad engine. The engine interface and the adapter are the same, so M14 adds only an `InProcessEngine` host and the iPadOS link type.

**Third-party notices.** INTEGRATION adds rows to `THIRD_PARTY_NOTICES.md`, and `check_third_party_notices.py` verifies them:
- under ngspice: KLU and the SuiteSparse parts it brings (AMD, COLAMD, BTF, SuiteSparse_config), each with the licence its files carry in the pinned tree; numparam; OSDI; sparse (which already has a row); the XSPICE code-model libraries compiled in;
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
| `hefesto-simd`, the `hefesto-ngspice-adapter` plugin, IPC server, heartbeat, worker sandbox profiles, frame codec | `src/simd/`, `sim/engine/frames.hpp` | SIMD | either |
| `WorkerHost` contract and hosts | `platform/worker_host.hpp`, `src/cli/`, desktop shell | PLATFORM | either |
| Area directories, `sources.cmake`, `codes.inc`, the stub headers of "Area contracts" except `platform/worker_host.hpp` and `sim/engine/frames.hpp`, `check_engine_boundary.py` | `lvs/`, `sim/` skeletons, `scripts/check_engine_boundary.py` | SCAFFOLD (R1 only; each header then passes to the owner named in this table) | either |
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
  - `nets`: netlist name → `{layout_name, hier_path, layout point | schematic wire}`;
  - `devices`: netlist name → `{use_path, location | schematic instance}`.
  
  It is ordered (a sorted vector) for deterministic iteration, with a second sorted index over `layout_name`. LVS and simulation both read it, and neither recomputes netlist names.
- **Two namings, one node.** The netlister chooses the netlist name (`ext2spice`'s choice, after `global` merges, case-collision renames and generated `<cell>_<x>_<y>#` names). Probes compute the `getnode`-style hierarchical name of the selected node (Q-SIM-24), which can differ: `@=` transistor-terminal names, a different preferred label, a different case. The two are reconciled by connectivity, never by comparing strings:
  - when SIMDECK builds the simulation net-name map, it fills `layout_name` by calling SIMPROBE's `node_name` at the entry's layout point, so both names describe the same node by construction;
  - a probe computes `node_name` of the selection and looks it up in the `layout_name` index;
  - if that fails (for example a node whose map entry was taken at a point the naming function names differently), the probe traces the selected node's connectivity in the extracted hierarchy and takes the map entry whose layout point lies in that node. Only when no entry's point lies in the node does the probe report the net as not netlisted.
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
- `third_party/ngspice/` holds the pin (Q-SIM-7) and the patch series. The ngspice-47 tag (`a80f6e3e`, checked 2026-11-22) has no `cmake/` directory and no top-level `CMakeLists.txt`; the CMake build exists only on master after 47. The pin is therefore ngspice-47 built by a Hefesto-written CMake wrapper, moving to the first release that ships the upstream CMake build once one exists. The `(ngspice ref: ...)` traces taken from `7aa47164` are re-checked against the pin by the SIM spec team (task 1.4). The patch series:
  - the static OSDI registry;
  - static code-model registration;
  - portable seeding (`simulation-monte-carlo` "Portable random numbers");
  - the deterministic initialisation hooks.
- It also holds a CMake wrapper with the spec's feature table, `-ffp-contract=off` on every platform (Q-SIM-10), OpenMP off (Q-SIM-12; it is measured on macOS before any change), and a generated manifest (`ngspice-manifest.json`: version `<release>-hefesto<n>`, patches, licences).
- IHP OSDI modules are compiled from the PDK's Verilog-A sources with a pinned OpenVAF at build time and registered statically (`simulation-engine-embedding` "OSDI compact models").

**Worker.**
- `hefesto-simd` runs one engine instance and serves one job at a time.
- **Reuse between jobs.** A healthy worker is reused for the next job, including the points of a run set. After a job's results are copied out, the adapter sends `remcirc` and `destroy all` (`simulation-engine-embedding` "Single engine instance per address space"). Before the next load it sends the PDK initialisation commands and `setseed <s>` again ("Deterministic engine initialisation"). The worker is replaced only after a fault. This is the single mechanism behind run-set determinism (D10, D14), and the 1-against-4-worker tests prove that no state leaks from one job to the next. Starting a process per point is not used: it would cost more than the 10 % overhead gate allows on a 1 000-point sweep (D21).
- **Heartbeat thread.** A dedicated thread in `hefesto-simd` writes the `beat` frame every 2 s. It does not touch the engine and is not blocked by it, so `ngSpice_Circ` loading a large deck (which runs on the calling thread and can take longer than 10 s) or a long analysis never trips the timeout. The beat proves that the process is alive and its frame writer works; a stuck analysis is handled by cancellation (2 s for `bg_halt`, then a fault).
- **Confinement.** The worker parses untrusted decks with ngspice's C parser, so it runs with less than the user's rights:
  - macOS: a sandbox profile applied at start (`sandbox_init` with a Hefesto profile, or the App Sandbox entitlement for the bundled worker) with no network, read-only access to the PDK store and the installed libraries, and write access only to the job directory;
  - Linux: `prctl(PR_SET_NO_NEW_PRIVS)`, a seccomp filter that denies network sockets and process creation, and a Landlock ruleset with the same file rules where the kernel supports it (otherwise the seccomp filter alone, recorded in the job log);
  - the core passes the job directory and the PDK store path in the `WorkerSpec`; the worker applies the confinement before it loads the adapter plugin.
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

- Results never depend on thread count. The LVS reports do not, and the stored simulation results do not either: each point has its own seed, and each point starts from the reuse sequence of D9 (`remcirc`, `destroy all`, the initialisation commands, `setseed`) in a reused worker. No `ngSpice_Reset` is used on desktop.
- The determinism tests run every run-set case with 1 worker and with 4 workers and compare the stored results bit for bit. With one worker, every point follows a different earlier history than with four, so the test catches state that leaks between jobs.

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
- A layout probe resolves a net through the name map's `layout_name` index, then by connectivity to the map's layout points (D4 "Two namings, one node"; `simulation-ui-and-crossprobe` "Probing a layout net", delta). A test probes a node whose netlist name and `getnode` name differ (a case-collision rename and an unlabelled node named by a transistor terminal).
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

**Layout references.** When circuit 1 is a layout, every net and device in the structured result carries the layout reference from the name map ("Layout object references in structured results", delta). Adding the references changes nothing in `comp.out`, the `-list` result or `comp.json`; those otherwise follow Q-LVS-2, -13 and -19.

**The `lvs` command.**
- The netgen form and `lvs -layout`.
- A `.sch` file on either side is netlisted in memory in LVS mode through `schem::netlist` (G4).
- Setup and inputs are resolved per Q-LVS-31.
- Results go to the result browser and cross-probing, and the command works headless.
- The `netgen` helper only names `lvs` in its diagnostic (D-34).

**Incremental layout side.** `ensure_extracted` treats a `.ext` file as stale when its timestamp differs from the cell's (Magic's check, `extTimestampMisMatch`, `extract/ExtMain.c:968-990`) and also when its `tech` or `style` header line differs from the current technology and extract style, which Magic does not check. The extra rule is raised as an open item against M7's `ensure_extracted` (D24) and covered by the `lvs-command` scenario "Extract style changed".

**Status chip.** The top-bar chip shows `LVS not run`, `LVS match`, `LVS mismatch (<n>)`, `LVS error`, `LVS cancelled` or `LVS stale`. It becomes `LVS stale` when the layout or the schematic changed after the run ("LVS status chip", delta).

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

**Captures with results loaded.** M8 captured SCHEM-SIM-01's deck only, and SCHEM-SIM-02's texts "with no simulation loaded", because no data set then carried the PDK ngspice models (`tests/golden/SCHEM/CASES.md`, "As authored"; `deviations.toml` `[SCHEM-SIM-01]`, `[SCHEM-SIM-02]`). There is therefore nothing yet to pin the result-dependent outputs from. The M8 spec team (xschem clean room, spec side) re-captures them in R1, once the capture image has the patched ngspice and the `*-ngspice` data sets (task 3.9):
- SCHEM-SIM-01 and -02 again, now with xschem running the PDK models through ngspice, so that the raw vectors, the op values and the annotated texts are captured;
- new SCHEM-SIM cases for what `schematic-symbols-and-libraries` defers to "M9 (SCHEM-SIM-02)" but SCHEM-SIM-02 (sky130 `test_inv` only) does not exercise: the gf180mcu `*fet*.sym` gm/id texts; the `annotate_fet_params.sym` and `annotate_bip_params.sym` texts on the three PDKs; the `xschem raw <what>` queries and `xschem raw_clear` as `schematic-command` lists them (`value`, `values`, `index`, `points`, `datasets`, `loaded`, `rawfile`, `sim_type`, `vars`, `list`), plus `xschem raw_read`, and the sky130 `test_customplot.sch` text that combines `tabulate`, `to_eng` and `xschem raw`.

The exact outputs of `to_eng`, `tabulate` and the three `ngspice::` procedures are then pinned from those captures by the M8 spec team (`schematic-symbols-and-libraries`).

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
| Workers | `max(1, min(4, physical_cores − 1, ⌊B_total / B_job⌋))`; a user setting can only lower it | `sim/runset` (Q-SIMADV-13; `simulation-engine-embedding` "Multiple simulations and corner parallelism", delta) |

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
| SIM | 53 inventoried; 48 whole cases plus the desktop legs of SIM-ENG-01 and -13 in M9 | patched ngspice, Magic for the DUT netlists | SIM-XP compares the three platforms; iPadOS cases below |
| SIMADV | 40 in M9 (CO 8, MC 13, MEAS 9, SW 10); 38 whole cases plus the desktop legs of MC-04 | patched ngspice | SIMADV-OPT (5) is M11; iPadOS cases below |
| SCHEM | SCHEM-SIM-01, -02 re-captured with results loaded; new SCHEM-SIM cases for the gf180mcu `*fet*` texts, the `annotate_fet_params`/`annotate_bip_params` texts on three PDKs and the `xschem raw` queries (D17); SCHEM-LVS-01 under Q-SCHEM-39 (a) | xschem, ngspice, netgen | re-captured by the M8 spec team (task 3.9) |
| Beta | BETA-E2E-SKY130, -GF180, -IHP | Hefesto run (end-to-end), checked against the step goldens | Q-SIM-27 |

**Beta flow inputs (Q-SIM-27).** One flow per PDK, the same steps on each, so that every step of the roadmap's "schematic → simulation → layout → DRC → GDS → extraction → LVS → post-layout simulation" runs in Hefesto:

| | sky130 | gf180mcu | IHP sg13g2 |
|---|---|---|---|
| Schematic | `beta_inv.sch`, drawn in Hefesto by the flow's script (`tests/e2e/beta/sky130/`), one `nfet_01v8` and one `pfet_01v8_hvt` with the sizes of `sky130_fd_sc_hd__inv_1` | `beta_inv.sch`, drawn the same way, with the devices and sizes of `gf180mcu_fd_sc_mcu7t5v0__inv_1` | `beta_inv.sch`, drawn the same way, with the devices and sizes of `sg13g2_inv_1`; the PDK's own `sg13g2_inv_1.sch` is used only as a cross-check netlist |
| Layout | cell `beta_inv`, drawn in Hefesto by the script: an instance of the PDK inverter cell plus Hefesto-drawn metal1 pin shapes and labels `A`, `Y`, power and ground | as sky130, with `gf180mcu_fd_sc_mcu7t5v0__inv_1` | as sky130, with `sg13g2_inv_1` |
| DRC | Hefesto's DRC with the PDK's Magic tech file (M3); zero errors | the same | the same |
| Authors | GOLDEN, from the specs (no xschem or netgen source); the user redraws both by hand for the sign-off | the same | the same |

- **Shared testbench** `beta_tb.sch` (one per PDK, same topology): an ideal supply at the PDK's nominal core voltage, a pulse input with 20 ps rise and fall, and a fixed load of four copies of the same inverter (FO4). It measures `tpd` as the mean of the 50 %–50 % rise and fall delays from the input to the DUT output.
- **The 30 % band** compares `tpd` of the schematic run with `tpd` of the `rc` post-layout run of `beta_inv` in the same testbench. Both runs drive the same FO4 load, so the band measures the cell's own parasitics, not an unloaded output.
- KLayout sign-off DRC (P-18) is not part of the Beta flow; it belongs to tape-out.

**iPadOS cases.** M9 builds no iPad engine, so these cases (or their iPad legs) get `deferred = "M14"` in `tests/golden/deviations.toml` when they are authored (task 3.6), each with the reason "needs the iPadOS in-process engine (M14)":
- SIM: SIM-ENG-01 (iPad leg), SIM-ENG-07 (reset fuzz; D9 resets only on iPadOS), SIM-ENG-09, SIM-ENG-10, SIM-ENG-12, SIM-ENG-13 (iPad leg), SIM-RES-07;
- SIMADV: SIMADV-SW-07, SIMADV-MC-04 (iPad leg), SIMADV-MC-13.

Two desktop cases are re-authored to match D9:
- SIM-ENG-06 expects the next DIV job to succeed in a replaced worker (a different process id), not after a reset (D-25);
- SIM-ENG-04 submits A and B to a pool of one worker, whose single engine instance gives the same `queued` timeline; the in-process variant is covered on iPadOS in M14.

**Reference runs before G2-SIM** (verification decisions, X-6):
- D-22: `.param` precedence, with one deck per PDK;
- D-26: sky130 `ng_nomodcheck`;
- D-27: gf180mcu `hs` against `hsa`.

Their results are recorded in `SIMADV.md` and `SIM.md` as `Verified` lines under the conditional `Resolution:` lines approved at G0 (D1).

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
  - **Isolating the comparison** uses the method of the DRC, GDS and extraction gates: the count of a script that reads both netlists, evaluates the setup file and compares, minus the count of the same script without the comparison command. This holds for Hefesto and for netgen, so no `--toggle-collect` on netgen's internal function names is used, and PERF needs no knowledge of netgen's source.
  - **Time budget.** Before the gate is enabled, PERF measures the 1M-device netgen run under callgrind (roughly 20–50 × native time) and checks that the three workloads fit `perf.yml`'s job limit. If they do not, the 1M-device workload runs in the nightly benchmark job instead of on every pull request.
- **`sim-bench`.** Hefesto's overhead (the whole job time minus ngspice's analysis time) must be at most 10 % on the inverter `tran` deck, a 1 000-device `rc` deck and a 1 000-point sweep. The 1 000-point sweep includes the reuse sequence of D9 between points.
  - **Analysis time** is taken from ngspice's own `rusage` command (its total analysis time), which the adapter sends after each analysis and reports in the job's events. The worker's process `rusage` is not used, because it includes the adapter and IPC time that the gate measures.
  - **Monte Carlo scaling.** A 1 000-sample run must be at least 0.6 × `w` times faster with `w` workers than with one, for `w` up to `min(4, physical_cores − 1)` of the runner. The GitHub-hosted runners have 4 vCPUs (Linux) and 3 (macOS arm64), so they check `w` up to 3 and 2; the `w = 4` point is checked on the M3 Pro bench machine and on a larger runner named in `perf.yml` when one is available.
- **Waveform viewer.** The rendering requirement of `simulation-ui-and-crossprobe`, measured on the M3 Pro and the Linux runner.
- Baselines are recorded in `bench/lvs/` and `bench/sim/`. `hefesto-performance` gains the requirement "LVS and simulation throughput" (delta).

### D22. Security review (SECURITY; `docs/contributing/security.md`)

A reviewer outside both LVS teams checks five areas:
- **Setup files.** Tcl in the sandbox against `hefesto-security` "Script sandbox enforcement", using hostile setup files that try `exec`, `socket`, `open` outside the roots, `load`, `interp` and infinite loops.
- **Deck sanitisation.** `.control`, `shell`, `system`, includes outside the granted folders, and `.lib` path tricks.
- **The core's frame parser.** It is fed by `fuzz_sim_frames` and must handle a worker that sends malformed, oversize or slow frames.
- **Raw import.**
- **Worker confinement (D9).** A test deck that tries to open a network socket, write outside the job directory, write into the PDK store and spawn a process is run in the worker on macOS and Linux; each attempt must be denied by the OS. The review also checks that only `hefesto-simd` carries the library-validation entitlement (D2).

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
- **From M7 (m7 D18):** the entry points, plus any change to them as an open item. Two changes are planned now: Q-LVS-33 (`render_with_map`) and the stale-header rule of `ensure_extracted` (D16, Q-LVS-32).
- **From M1:** the LVS chip and the Simulate button.
- **From the audit:** D-22 to D-28.

Exit criteria:
- The LVS, SIM and SIMADV (M9 part) goldens pass under D19's comparators, except the iPadOS cases and legs that D19 defers to M14. SCHEM-SIM-01/02, the new SCHEM-SIM cases of D17 and SCHEM-LVS-01 pass.
- `check_requirement_coverage.py` covers every requirement of the 17 capabilities after the task 1.2 sync and the M9 part of `schematic-simulation-integration`; the iPadOS-only requirements of `simulation-engine-embedding` are listed as deferred to M14.
- Unit, property and fuzz tests are green. LVS and non-numeric output are byte-identical on the three platforms, and simulation results agree within tolerance (D20).
- Each `netgen-compat` branch point is tested in both modes.
- `check_engine_boundary.py`, `check_cleanroom.py` and `check_third_party_notices.py` pass, and the legal review of the desktop ngspice linking (D2) is recorded.
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
- `adapter/ngspice_adapter.{hpp,cpp}` is the only file that includes `sharedspice.h`. It builds the `hefesto-ngspice-adapter` plugin, whose exported C ABI (`hefesto_engine_v1`) mirrors `sim::Engine` with no ngspice types.
- `ipc/frames.{hpp,cpp}` defines the frames. It is shared with `sim/engine` as a header-only codec in `hefesto/sim/engine/frames.hpp`, which SIMD owns (SCAFFOLD does not write it).
- `sandbox/` holds the macOS profile and the Linux seccomp and Landlock set-up (D9).

### PLATFORM: `hefesto/platform/worker_host.hpp`
`WorkerSpec {program, args, env, job_dir, readonly_roots}`, `WorkerHost::supported()`, `WorkerHost::start(const WorkerSpec&) -> diag::Outcome<std::unique_ptr<WorkerChannel>>` and `WorkerChannel::{send, on_frame, on_exit, terminate}`.

### SIMDECK: `hefesto/sim/deck/`
`prepare(const Testbench&, const DeckContext&) -> diag::Outcome<PreparedDeck>`, where `PreparedDeck` holds the lines, the `NameMap`, the descriptor and the diagnostics. `ModelDescriptor::load(pdk)`.

### SIMRES, SIMPROBE: `hefesto/sim/result/`, `hefesto/sim/probe/`
- `ResultStore` with `write_raw`, `read_raw`, `export_csv` and `measure_waveform`.
- `node_name(const LayoutSelection&) -> std::string`, which implements irsim-interface "Hierarchical node names".
- `resolve_probe(const NameMap&, const LayoutSelection&) -> std::optional<NetEntry>`: the `layout_name` index, then connectivity to the map's points (D4).
- `probe(const Job&, const ProbeTarget&) -> diag::Outcome<TraceId>`.
- `OverlayFeed`.

### SIMADV, SIMMEAS: `hefesto/sim/runset/`, `hefesto/sim/meas/`
- `RunSet::generate(const RunSetSpec&) -> std::vector<Point>` and `execute(RunSet&, JobManager&)`.
- `sample_seed(base, k)`.
- `evaluate_measurements(const ResultStore&, const MeasSet&) -> MeasTable` and `statistics(const MeasTable&, const Specs&) -> StatsReport`.

### MAPS: `hefesto/netlist/name_map.hpp`
`NameMap {nets, devices}` (sorted), with `NetEntry {netlist_name, layout_name, hier_path, point | wire}` and a `layout_name` index. `netlist::spice::render_with_map(..., Settings) -> diag::Outcome<RenderedNetlist {text, names}>`. `schem::NetlistResult::names`.

## Risks / Trade-offs

- **R1. M7 and M8 slip.** Mitigation: R1–R3 need neither (D1), and their sources are SPICE files and decks. The layout and schematic sources come last.
- **R2. Spec gaps under the LVS clean room.** Mitigation: goldens first (G2-LVS). Spec issues go to the LVS spec team, with a turnaround target of one working day.
- **R3. Paraphrase in the algorithm specs.** Mitigation: they are reviewed first (D2). The NG-ALG and NG-RED goldens pin the behaviour independently of the wording.
- **R4. Simulation tolerance tuning.** Mitigation: `-ffp-contract=off`, then the SIM-XP cases re-tune the tolerances (Q-SIM-10). A case that needs a looser tolerance gets an approved deviation, never a silent change.
- **R5. Legal review changes Q-SIM-20.** The review runs before the Desktop Beta (task 20.1). Mitigation: the worker boundary stays. Only the link type of the adapter plugin and the library would change, and the editor is unaffected.
- **R6. IPC bandwidth for large results.** Mitigation: chunked frames now. Shared memory behind the same frames if `sim-bench` shows transfer dominating.
- **R7. Reference-run surprises (D-22).** If ngspice keeps the first `.param`, the sky130 `both` mode and the sweep overrides move to Q-SIMADV-3 (b) and the Q-SIMADV-9 fallback. Both are drafted.
- **R8. Desktop Beta scope.** Mitigation: the Beta checklist is fixed in R0 (Q-SIM-27), so the exit has no open-ended items.

## Migration Plan

No existing user data changes. Projects gain a `sim/` result folder and testbench files (`simulation-command` "Testbench persistence"). Projects without them open as before. Desktop installs gain `hefesto-simd` and `libngspice-hefesto`. If either is missing, simulation is unavailable and every other feature works.

## Open Questions

All four were answered (a) by the user on 2026-11-22 (G0, task 1.1):
- **OQ1: (a).** The spec changes were recorded as this change's `specs/` deltas and synced into the living specs at G0 (task 1.2). `openspec` 1.13 rejects `skip_specs: true` while delta files exist, so the synced delta files were removed from the change when `skip_specs: true` was set; the archive at exit (task 20.6) then applies nothing a second time.
- **OQ2: (a).** R0 and R1 start now; R2–R3 start once G0, G1 and each stream's G2 pass, in parallel with M7 and M8 implementation; R4 starts after their exits (G3, G4).
- **OQ3: (a).** DESIGN draws the LVS browser and waveform boards, including the run-set, corner and Monte Carlo views, before SHELL builds the screens.
- **OQ4: (a).** A MAPS workstream of the M7 and M8 owners adds `render_with_map` and `NetlistResult::names` after each exit, reviewed by LVSCMD and SIMDECK.

The options as drafted:

1. **How spec changes are recorded.**
   - (a) *Recommended:* as this change's `specs/` deltas, applied to the living specs when G0 passes and validated with `openspec validate --all --strict`.
   - (b) Follow the M7 and M8 precedent: edit the living specs directly after approval and set `skip_specs: true`.
   - (c) Both: deltas for review, then a direct edit.
2. **When M9 implementation starts relative to M7 and M8.**
   - (a) *Recommended:* R0 and R1 as soon as the plan is approved. R2–R3 once G0, G1 and each stream's G2 pass, in parallel with M7 and M8 implementation. R4 after their exits (G3, G4).
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

| Decision (all approved 2026-11-22) | Approved option | Design sections it shapes |
|---|---|---|
| Q-LVS-30 LVS clean room (how the approved two-team requirement applies to netgen) | (a) M8 two-team process | D1, D2, D3, D19 |
| Q-SIM-20, D-28 (desktop) | (a) shared `libngspice-hefesto` behind the `hefesto-ngspice-adapter` plugin, loaded only by `hefesto-simd` in release builds; legal review before Beta | D2, D9 |
| Q-SIM-21 worker IPC | (a) `WorkerHost`, framed pipes | D9, D10, D18 |
| Q-SIM-22 in-process desktop | (a) developer builds only, through the same adapter plugin | D2, D9 |
| Q-SIM-26 D-28 timing | (a) desktop decided in R0 and reviewed before Beta, iPad before M14 | D2 |
| Q-SIM-11, D-25 fault recovery | replace the worker on desktop | D9 |
| Q-SIM-7, Q-SIM-23 ngspice pin, reference build | ngspice-47 (no CMake build in that tag) with a Hefesto-written CMake wrapper plus patches; patched reference with OpenVAF | D9, D19 |
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
