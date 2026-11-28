# Open Questions — SIM (embedded ngspice simulation)

Capabilities: `simulation-engine-embedding`, `simulation-netlist-preparation`, `simulation-analyses-and-results`, `simulation-ui-and-crossprobe`, `simulation-command`. Product decision: `docs/decisions/product.md#P-7`.

These capabilities are `native` and have no Magic counterpart. "Evidence" therefore points at the ngspice source. The research used SourceForge `ngspice` master @ `7aa47164`, development after ngspice-47 (2026-08-11); that tree was cloned read-only, and was not built or run. On 2026-11-22 (task 1.4 of `openspec/changes/m9-lvs-simulation`) every ngspice line reference below was re-checked against the pinned release ngspice-47, tag commit `a80f6e3e` (Q-SIM-7), read through the SourceForge raw file view and not cloned, and now refers to that tree, except where an entry says otherwise. The list of traces checked is in `docs/decisions/m9-decision-table.md`.

### Q-SIM-1: [platform] LGPL components statically linked into the iPadOS app
- Kind: platform
- ngspice behavior: ngspice is Modified BSD, but `src/maths/KLU` is LGPLv2, `src/frontend/numparam` is LGPLv2 or newer, `src/osdi` is MPL 2.0, and `src/xspice/icm/table` is GPLv2 or newer. numparam is part of the core (`.param` expressions), so it cannot be switched off. iPadOS forbids loading dynamic libraries that are not embedded and signed, and App Store distribution complicates the LGPL relinking requirement.
- Evidence: COPYING:1-45
- Options: (a) ship static, with object files and relink instructions offered on request (the usual LGPL static-linking compliance); (b) embed KLU and numparam as a signed dynamic framework inside the app bundle (allowed on iOS); (c) build without KLU (Sparse 1.3 solver only) and ask upstream to relicense numparam; (d) legal review before deciding.
- Recommendation: (b) for KLU and numparam as one embedded framework, if legal review agrees. Otherwise (a). Exclude the `table` code models (GPL) in any case, as the spec already requires.
- Status: approved 2026-11-22 (the 2026-11-07 draft confirmed through Q-SIM-26 (a); recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- Resolution: platform — D-28's recommendation, with the timing of Q-SIM-26 (a): legal review first, then on iPadOS KLU and numparam as one embedded, signed dynamic framework, or static linking with object files and relink instructions on request if the review says so; that iPadOS choice is settled by D-28 before M14. The desktop build follows Q-SIM-20 (a), reviewed before the Desktop Beta. The GPL `table` code models are excluded in every case (D-28, D-5 draft 2026-11-07). Approved by the user 2026-11-22.

### Q-SIM-2: [platform] OSDI / Verilog-A models on iPadOS
- Kind: platform
- Status: approved 2026-11-07 (P-7, P-13, D-5); symbol-prefix amendment approved 2026-11-27 (user decision, recommended option; `docs/decisions/m9-decision-table.md` R3-3)
- ngspice behavior: OSDI models are shared objects compiled by OpenVAF. ngspice loads them with `dlopen(path, RTLD_NOW|RTLD_LOCAL)` and looks up the generic symbols `OSDI_DESCRIPTORS`, `OSDI_NUM_DESCRIPTORS`, `OSDI_VERSION_MAJOR/MINOR` and `OSDI_DESCRIPTOR_SIZE`. It accepts OSDI 0.3 (original OpenVAF) and 0.4 (OpenVAF-reloaded; 0.5 is experimental). iPadOS App Store builds cannot load code the user supplies, OpenVAF (Rust + LLVM) cannot run on device, and every module exports the same symbol names, so several modules cannot be linked statically as they are.
- Evidence: src/osdi/osdiregistry.c:31, src/osdi/osdiregistry.c:339-451, src/osdi/osdi.h:20-21
- Options: (a) a Hefesto patch adding a static OSDI registry: compile each module at build time with symbol prefixing (for example `objcopy --prefix-symbols`, or an OpenVAF option if one exists) and register the descriptors through a table; (b) upstream such a static-registration API to ngspice; (c) no OSDI on iPad, which drops IHP sg13g2 support there.
- Recommendation: (a) now, (b) in parallel. Decide the built-in module list (PSP 103.x, `r3_cmc`, `mosvar`, HICUM L2, and possibly BSIM-CMG and EKV) and check each model's license for redistribution in binary form.
- Resolution: platform — a Hefesto patch adds a static OSDI registry: each module is compiled at build time with prefixed symbols and registered through a table; the same API is proposed upstream; the built-in module list is the one of `simulation-engine-embedding`, each model's license checked for binary redistribution (P-7, P-13, D-5, approved 2026-11-07, 2026-11-07).
- Resolution (amendment, approved 2026-11-27): the prefixed symbols are generated per module at build time: each OpenVAF module's `OSDI_DESCRIPTORS`, `OSDI_NUM_DESCRIPTORS`, `OSDI_VERSION_MAJOR`, `OSDI_VERSION_MINOR` and `OSDI_DESCRIPTOR_SIZE` get a prefix derived from the module name, and the module is registered under its basename in the static table of patch `0001-static-osdi-registry.patch`, on every platform; a built-in module is never loaded from a `.osdi` file. `simulation-engine-embedding` "OSDI compact models", scenario "Statically registered modules with per-module symbol prefixes"; NGBUILD task 5.2. Approved by the user 2026-11-27.

### Q-SIM-3: [platform] XSPICE code models without dlopen
- Kind: platform
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: XSPICE code-model libraries (`analog.cm`, `digital.cm`, `spice2poly.cm`, `xtradev.cm`, `xtraevt.cm`, `table.cm`) are shared objects loaded with `dlopen` by the `codemodel` command, which is normally issued from `spinit`. Upstream has no option to link them statically.
- Evidence: src/spicelib/devices/dev.c:290-420, configure.ac:141-143, src/xspice/icm/GNUmakefile.in:9-10
- Options: (a) a Hefesto patch that links the `.cm` sources statically and registers them at `ngSpice_Init`; (b) leave XSPICE out on iPad (no `spice2poly`, which breaks `E`/`G` POLY sources used by some PDK models); (c) upstream.
- Recommendation: (a). Verify that no supported PDK model needs `table.cm` (GPL).
- Resolution: platform — option (a): a Hefesto patch links the XSPICE code-model sources statically and registers them at `ngSpice_Init`, on every platform; the GPL `table` code models are never built; task 3.3's PDK runs confirm that no supported PDK model needs `table.cm`. Approved by the user 2026-11-22.

### Q-SIM-4: [platform] Crash containment on iPadOS
- Kind: platform
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: unrecoverable errors are handled by `longjmp` and `ControlledExit`. A real crash (SIGSEGV or an abort inside a model) ends the host process. On iPadOS the host process is the editor, because no helper process can be spawned.
- Evidence: src/sharedspice.c:2132-2188, src/sharedspice.c:914
- Options: (a) in-process with a mandatory autosave before each run (current spec); (b) run ngspice in an app extension, if a suitable extension point exists (probably not for general compute); (c) an optional remote engine (a desktop or server `hefesto-simd` over the network); (d) a WebAssembly build of ngspice in a sandboxed runtime. That is in-process but memory-isolated, roughly 2–5× slower, and has no OSDI unless the models are compiled to wasm.
- Recommendation: (a) as baseline, with (c) as an option later. Evaluate (d) if crash reports show ngspice crashes are common.
- Resolution: platform — option (a): on iPadOS the engine runs in-process, with a mandatory autosave before each run (M14); a remote engine (option (c)) may be added later. Approved by the user 2026-11-22.

### Q-SIM-5: Single instance per process; parallelism
- Kind: platform
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: all simulator state is global (`ft_curckt`, the `fl_running` and `fl_exited` thread flags, status statics). Upstream's way to run in parallel is to load separately named copies of the shared library, using the `ident` argument of `ngSpice_Init_Sync`.
- Evidence: src/sharedspice.c:390-470, src/include/ngspice/sharedspice.h:383-398
- Options: (a) worker processes on desktop and serial execution on iPad (current spec); (b) `dlmopen` namespaces on Linux and renamed dylib copies on macOS; (c) refactor ngspice to an instance struct (large upstream work).
- Recommendation: (a). Review the default worker count (`min(4, cores − 1)`) against memory use with large RC netlists.
- Resolution: platform — option (a): worker processes on desktop and serial runs on iPadOS; the pool size is the formula of Q-SIMADV-13. Approved by the user 2026-11-22.

### Q-SIM-6: ngspice lowercases names; layout names are case-sensitive
- Kind: ambiguity
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: the netlist parser lowercases node, instance and model names. Vector names come back lowercase. Magic node and label names are case-sensitive, so `A` and `a` are different nets in layout and the same net in ngspice. Device-current vector names (`@m.xdut.xm1[id]`) depend on how the PDK wraps devices in subcircuits. For sky130, devices are `X` instances of subcircuit wrappers, and the primitive path inside the wrapper must be confirmed.
- Evidence: src/frontend/inpcom.c (case folding during input preparation), src/frontend/rawfile.c:160-200
- Options: replicate (lowercase everywhere and let nets merge silently, which is wrong) | rename on collision (current spec: `<name>__c<k>`) | refuse to simulate.
- Recommendation: rename on collision, as specified. Capture golden cases SIM-NET-03 and SIM-UI-04 to confirm vector naming of nested subcircuit nodes and of device currents.
- Resolution: fix — as specified: a name that collides with another after ngspice's lowercasing is renamed `<name>__c<k>`; golden cases SIM-NET-03 and SIM-UI-04 confirm the vector naming of nested subcircuit nodes and device currents. Approved by the user 2026-11-22.

### Q-SIM-7: ngspice version pin and the new CMake build
- Kind: ambiguity
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: the master tree contains a CMake build (`CMakeLists.txt`, `cmake/Ngspice*.cmake`, with `NGSPICE_BUILD_SHARED`). The latest release (ngspice-47, August 2026) may still be autotools-only. There is no iOS toolchain support, and there is no option to build a static `libngspice`; the CMake option builds a shared one.
- Evidence: configure.ac:178-180, src/Makefile.am:19-32, src/Makefile.am:498-616, NEWS:1 (ngspice-47 builds with autotools only; `CMakeLists.txt:150-260` and `cmake/NgspiceOptions.cmake:86-91`, cited before the re-check, exist only in master `7aa47164`)
- Options: pin ngspice-47 and write Hefesto's own CMake wrapper | pin a master commit that has the CMake build | wait for ngspice-48.
- Recommendation: pin the first release that has the CMake build, and carry Hefesto patches for the static archive, the static code-model and OSDI registries, and iOS. Offer these upstream.
- Checked 2026-11-22 (M9 plan): the ngspice-47 tag (`a80f6e3e95d51534905b1f23410a951802666656`) has no `cmake/` directory and no top-level `CMakeLists.txt`; it builds with autotools only. No released ngspice has the CMake build yet, so the first option (ngspice-47 with Hefesto's own CMake wrapper) is the one the recommendation leads to (`docs/decisions/m9-decision-table.md`).
- Resolution: other — pin the release ngspice-47 (tag commit `a80f6e3e95d51534905b1f23410a951802666656`) with a Hefesto-written CMake build (`third_party/ngspice/CMakeLists.txt`), since that tag has no `cmake/` tree and no released ngspice ships the upstream CMake build; carry the Hefesto patches (static archive or shared library per Q-SIM-20, static code-model and OSDI registries, portable seeding, iOS) and offer them upstream; moving to the first release with the upstream CMake build is a later, separate decision. The golden capture pin and roadmap principle 2 move to the same release (Q-SIM-23). Approved by the user 2026-11-22.

### Q-SIM-8: PDK simulation model descriptors need verification
- Kind: doc-mismatch
- Status: approved 2026-11-22 as a conditional verify rule (design D1 of `openspec/changes/m9-lvs-simulation`; `docs/decisions/m9-decision-table.md`); `Verified` 2026-11-26 by task 3.3 (M9 R2)
- ngspice behavior: not applicable. The descriptor contents come from open_pdks and the IHP repository. The values in the spec are from memory:
  - sky130: `sky130.lib.spice` sections `tt ff ss sf fs`, plus `.spiceinit` with `set ngbehavior=hsa` and `set ng_nomodcheck`
  - gf180mcu: `design.ngspice`, plus `sm141064.ngspice` sections `typical ff ss fs sf`
  - sg13g2: `cornerMOSlv.lib` (`mos_tt` …), `cornerMOShv.lib`, `cornerHBT.lib`, `cornerRES.lib` and `cornerCAP.lib`, plus `.spiceinit` loading `psp103_nqs.osdi`, `r3_cmc.osdi` and `mosvar.osdi`
- Evidence: none checked locally; no PDK is installed on the research machine.
- Options: verify against open_pdks and IHP-Open-PDK at the pinned versions; generate descriptors automatically from `libs.tech/ngspice/.spiceinit` and the corner files.
- Recommendation: verify (cross-cutting X-6 already requires the open_pdks sources). Also confirm the section names for HBT, RES and CAP in sg13g2, and whether sky130 still needs `ng_nomodcheck`.
- Resolution: verify — the descriptors are those this entry lists, with the sky130 model root `libs.tech/combined` (Q-SIMADV-2), each checked by the D-26 reference run against the pinned PDK sets: (a) if the run confirms a value (file name, section name, `.spiceinit` command, OSDI module, including the sg13g2 HBT, RES and CAP sections), it stays; otherwise (b) it is corrected to what the pinned PDK files define, in `simulation-netlist-preparation`. sky130 `ng_nomodcheck`: (a) if the run without it fails on `combined`, the descriptor sets it; otherwise (b) the descriptor does not set it. A result that neither branch covers goes back to the user before gate G2-SIM. Approved by the user 2026-11-22.
- Verified 2026-11-26: descriptors (a), sky130 `ng_nomodcheck` (b). golden-capture run 45859754101 (mode `sim-refruns`, image `ngspice-47-hefesto2`, harness `tools/golden/refruns/run_refruns.py`): every listed model line and section loads cleanly against the pinned sets: sky130 `combined` `tt ff ss sf fs`; gf180mcu `design.ngspice` and `sm141064.ngspice` `typical ff ss fs sf`; sg13g2 `cornerMOSlv.lib` and `cornerMOShv.lib` `mos_tt mos_ff mos_ss mos_sf mos_fs`, `cornerHBT.lib` `hbt_typ hbt_bcs hbt_wcs`, `cornerRES.lib` `res_typ res_bcs res_wcs`, `cornerCAP.lib` `cap_typ cap_bcs cap_wcs`. The pinned sg13g2 `.spiceinit` loads six OSDI modules (`psp103`, `psp103_nqs`, `r3_cmc`, `mosvar`, `cap_cmomi`, `cap_cmomf`), which is the set `simulation-netlist-preparation` already lists since Q-SIMADV-5; the three-module list in this entry is superseded. sky130 on `combined` without `ng_nomodcheck` loads and runs with no warning (run `sky130-d26-without`), so the descriptor does not set it (`simulation-netlist-preparation` table amended).

### Q-SIM-9: `.control` sections and the `shell` command
- Kind: platform
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: the `.control` language is a full interpreter. It includes `shell` (runs a host command through `system()`), `cd`, `load`, `source`, `write` and `codemodel`/`osdi` (load code). In shared mode, `.control` commands run on the caller's thread right after the circuit is parsed.
- Evidence: src/frontend/commands.c:187-459, src/sharedspice.c:24-40
- Options: strip `.control` by default and block dangerous commands (current spec) | run `.control` fully on desktop only | add a Hefesto-side `.meas`/script replacement.
- Recommendation: as specified. Review App Store guideline 2.5.2 (interpreted code) for the remaining `.control` subset on iPad, and consider allowing only `let`, `meas`, `print` and `alter`.
- Resolution: platform — as specified: `.control` sections are stripped by default and dangerous commands are blocked on every platform; the iPadOS subset (`let`, `meas`, `print`, `alter`) is reviewed against App Store guideline 2.5.2 in M14. Approved by the user 2026-11-22.

### Q-SIM-10: Cross-platform numeric reproducibility
- Kind: ambiguity
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: results depend on floating-point evaluation (FMA contraction on arm64 compared with x86_64, libm differences between glibc and Apple libm) and on the time-step control path. Small differences can change accepted time steps and, through them, waveforms near sharp edges.
- Evidence: not applicable (numerical behavior)
- Options: tolerance-based comparison (current spec: 1e-3 relative plus an absolute floor) | build with `-ffp-contract=off` and a bundled libm for bitwise equality.
- Recommendation: build with `-ffp-contract=off` on all platforms and keep tolerance-based golden comparison. Re-tune the tolerances from the golden cases SIM-XP-*.
- Resolution: platform — ngspice and Hefesto are built with `-ffp-contract=off` on every platform, and golden comparison stays tolerance-based; the tolerances are re-tuned from the SIM-XP golden cases. Approved by the user 2026-11-22.

### Q-SIM-11: Recovery after an ngspice error
- Kind: quirk
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: after an error, `shared_exit` does a `longjmp` back into `ngSpice_Command` or `ngSpice_Circ`, or calls `pthread_exit` on the background thread, and tells the caller to "reset or detach" the library. A statically linked library cannot be detached, and it is unclear whether `ngSpice_Reset` (`totalreset`) restores every global after an arbitrary error.
- Evidence: src/sharedspice.c:2132-2188, src/sharedspice.c:1471-1477, src/sharedspice.c:2529-2540
- Options: trust `ngSpice_Reset` (current spec, with a second fault making the engine `unavailable`) | always restart the worker after any fault on desktop.
- Recommendation: on desktop, always replace the worker after a fault (cheap and safe). On iPad, use `ngSpice_Reset` as specified, and fuzz-test reset after error decks (SIM-ENG-07).
- Resolution: platform — on desktop the worker is always replaced after any fault and `ngSpice_Reset` is never used; on iPadOS the engine recovers with `ngSpice_Reset`, covered by fuzz tests of reset after error decks (D-25). Approved by the user 2026-11-22.

### Q-SIM-12: [platform] OpenMP and performance on Apple platforms
- Kind: platform
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: OpenMP parallelises device loading (`USE_OMP`). Apple clang ships no OpenMP runtime, and none is available on iPadOS.
- Evidence: configure.ac:1320-1330 (OpenMP on by default in the autotools build)
- Options: OpenMP off on Apple (current spec allows it only on Linux, and optionally on macOS through a bundled libomp) | bundle LLVM libomp on macOS.
- Recommendation: off on iPadOS. Measure on macOS before bundling libomp.
- Resolution: platform — OpenMP is off on every platform in M9 and always off on iPadOS; bundling libomp on macOS is considered only after a measurement. Approved by the user 2026-11-22.

### Q-SIM-13: [platform] `$VAR` and host paths inside PDK model files
- Kind: platform
- ngspice behavior: `.include`/`.lib` names that start with `$VAR` (one or two variables) are expanded with `getenv`. Relative paths are tried against the including file's directory, then against the `sourcepath` list. On iPadOS there are no meaningful environment variables, and PDK files live in the sandbox.
- Evidence: src/frontend/inpcom.c:2016-2106, src/frontend/inpcom.c:2136-2290
- Options: Hefesto resolves and rewrites all paths before ngspice sees them (current spec) | set environment variables in-process with `setenv` before loading.
- Recommendation: rewrite, as specified. It is deterministic and does not depend on process-global environment state.
- Resolution: platform — Hefesto resolves and rewrites `.include`/`.lib` paths against the PDK store before ngspice reads the deck; no environment variables are consulted (X-4, hefesto-files-and-pdks, 2026-11-07).

### Q-SIM-14: Progress reporting tracks only two thread identities
- Kind: quirk
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: progress tracking remembers at most two thread ids. A third thread (a new `bg_run` thread after earlier runs) takes over slot 2. Status strings are throttled to 150 ms, and are suppressed when unchanged. The `tran` status reports `--ready--` once `time > tstop − tmax`.
- Evidence: src/sharedspice.c:1953-2108
- Options: rely on status strings only | combine them with the streamed time point (current spec does this for `tran`).
- Recommendation: as specified. Check in golden case SIM-RUN-03 that progress keeps working on the third and later runs in one engine.
- Resolution: other — as specified: progress combines ngspice's status strings with the streamed time point; golden case SIM-RUN-03 checks that progress keeps working on the third and later runs in one engine. Approved by the user 2026-11-22.

### Q-SIM-15: Callback threads and print thread latency
- Kind: quirk
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: with `low_latency` defined (the default), output text is sent synchronously from the calling thread. Otherwise a separate print thread batches output. Callbacks can therefore arrive on the caller's thread, the background thread, or the print thread.
- Evidence: src/sharedspice.c:18-22, src/sharedspice.c:1099-1109
- Options: none needed. The spec requires thread-agnostic, non-blocking handlers that copy their data.
- Recommendation: keep the default `low_latency` build.
- Resolution: other — keep ngspice's default `low_latency` build; callback handlers copy their data and never block, whatever thread calls them. Approved by the user 2026-11-22.

### Q-SIM-16: Default result memory budgets
- Kind: ambiguity
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22); interim-value amendment approved 2026-11-27 (user decision, R3-6)
- ngspice behavior: ngspice grows vectors without limit, and memory is reported only for information.
- Evidence: src/sharedspice.c (usage reporting)
- Options: 512 MiB on iPad and 4 GiB on desktop (current spec) | a fraction of physical memory.
- Recommendation: define the budget relative to the iPad memory bound of capability `hefesto-performance` (3 GB in total, shared with layout and GPU). Revisit after profiling.
- Resolution: platform — the default result budgets stay 512 MiB on iPadOS and 4 GiB on desktop; they are revisited after the `sim-bench` profiles. Approved by the user 2026-11-22.
- Resolution (amendment, approved 2026-11-27): the desktop default stays 4 GiB until Q-SIM-25 decides it (see there); iPadOS stays 512 MiB. Approved by the user 2026-11-27.

### Q-SIM-17: Raw file header fields are not reproducible
- Kind: quirk
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: raw files contain `Date:` (wall clock) and `Command: ngspice-<ver>, Build <date>`, and the binary data is host byte order (`fwrite` of `double`).
- Evidence: src/frontend/rawfile.c:114-130, src/frontend/rawfile.c:224-258
- Options: golden comparisons ignore `Date:` and `Command:` lines (recommended) | Hefesto writes a fixed date in test mode.
- Recommendation: ignore those lines when comparing. Hefesto always writes little-endian data, which is native on all supported targets.
- Resolution: fix — golden comparisons of raw files ignore the `Date:` and `Command:` header lines; Hefesto always writes little-endian binary data. Approved by the user 2026-11-22.

### Q-SIM-18: [platform] IRSIM replacement on iPadOS
- Kind: platform
- ngspice behavior: XSPICE digital code models and event-driven nodes (`ngGet_Evt_NodeInfo`, `ngSpice_Init_Evt`) could provide switch- or logic-level simulation in-process. IRSIM has no in-process path on iPad (Q-LEGACY-71).
- Evidence: src/include/ngspice/sharedspice.h:185-210, src/include/ngspice/sharedspice.h:370-405
- Options: no switch-level simulation on iPad | embed IRSIM as a library (it is not designed for that) | map `.sim` to an XSPICE digital netlist.
- Recommendation: defer. Decide together with Q-LEGACY-71.
- Resolution: platform — no switch-level simulation on iPadOS; IRSIM is desktop-only and embedded ngspice provides simulation on every platform (same answer as Q-LEGACY-71) (X-4, P-13, 2026-11-07).

### Q-SIM-19: [platform] Background execution on iPadOS
- Kind: platform
- Status: approved 2026-11-22 (recommendation in `docs/decisions/m9-decision-table.md`, approved by the user 2026-11-22)
- ngspice behavior: not applicable (an iPadOS lifecycle question). iPadOS suspends apps in the background, so a long simulation cannot keep running when the user switches apps.
- Evidence: not applicable
- Options: pause on background (current spec) | request a background-processing task (time-limited and not guaranteed) | a remote engine.
- Recommendation: pause and resume, as specified. Show an in-app notice when a run is paused.
- Resolution: platform — as specified: on iPadOS a running simulation is paused when the app goes to the background and resumed when it returns, with an in-app notice (M14). Approved by the user 2026-11-22.

### Q-SIM-20: [platform] How `hefesto-simd` links ngspice on desktop
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan, `openspec/changes/m9-lvs-simulation` design D2; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`)
- ngspice behavior: ngspice's core is under the Modified BSD licence. KLU is LGPLv2, numparam is LGPLv2 or newer, and `src/osdi` is MPL-2.0. On desktop, `simulation-engine-embedding` runs ngspice in the separate worker executable `hefesto-simd`, which talks to the core over IPC. The requirement "ngspice embedded as a library" also asks for a static archive "on every platform". Under the M9 rule, an external tool keeps its licence boundary as a subprocess or an optional plugin.
- Evidence: COPYING:1-45; `openspec/specs/simulation-engine-embedding/spec.md` (Requirements "ngspice embedded as a library" and "Crash containment on desktop"); `docs/decisions/audit/DECISIONS.md` D-28
- Options:
  - (a) On desktop, build ngspice as one shared library, `libngspice-hefesto`, from the pinned source and the Hefesto patches. The engine adapter is a separate plugin, `hefesto-ngspice-adapter`, which links the library; in release builds only `hefesto-simd` loads it, through the install-relative runpath. The editor, the CLI and the core never link ngspice or the plugin. A user can replace the library: on macOS `hefesto-simd` carries the disable-library-validation entitlement (and is sandboxed), and the build documentation describes how to sign a rebuilt library. The linking goes through D-28's legal review before the Desktop Beta. iPadOS stays with D-28.
  - (b) Link ngspice statically into `hefesto-simd`, and offer its object files and relink instructions on request.
  - (c) Link it statically into the editor process (rejected by the licence rule).
- Recommendation: (a). The process boundary and a library the user can actually replace are intended to meet the LGPL obligations without a relink kit; D-28 recommends a legal review before linking is settled, so that position is confirmed by the review before the Desktop Beta, not assumed. Without the entitlement, a notarized hardened-runtime worker would refuse a user-signed library, and the library would not be replaceable in practice. The option also keeps the code-model and OSDI registries the same as in the static build. The spec's "static archive on every platform" becomes "as D-28 settles on iPadOS, a shared library behind the adapter plugin on desktop". MPL-2.0 requires the source of every covered file in the executable (the whole OSDI tree as built), not only the modified files.
- Resolution: platform — option (a): on Linux and macOS ngspice is built as one shared library, `libngspice-hefesto`, linked only by the separate engine adapter plugin `hefesto-ngspice-adapter`; in release builds only the sandboxed worker `hefesto-simd` loads that plugin, through an install-relative run path; the editor, the CLI and the core never link ngspice or the plugin, and a CI check enforces this. The user can replace the library: on macOS `hefesto-simd` alone disables library validation, and the build documentation describes how to sign a rebuilt library. The desktop linking goes through D-28's legal review before the Desktop Beta; the review can change the link type without touching the editor. iPadOS follows D-28. Approved by the user 2026-11-22.

### Q-SIM-21: [platform] Worker IPC transport and the platform contract
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`)
- ngspice behavior: n/a. This is a Hefesto host question.
  - `platform::ProcessRunner` offers only a blocking `run(request) -> {exit_code, stdout, stderr}` (`src/core/include/hefesto/platform/process_runner.hpp`).
  - `hefesto-simd` needs a long-lived, bidirectional and streaming channel that carries deck lines, commands, events and float64 vectors. It must also be restarted after a crash.
- Evidence: `src/core/include/hefesto/platform/process_runner.hpp`; `openspec/specs/simulation-engine-embedding/spec.md` (Requirement "Crash containment on desktop"); `openspec/specs/hefesto-architecture/spec.md` (Requirement "Platform services abstraction")
- Options:
  - (a) A new platform service `platform::WorkerHost`:
    - it starts a worker and returns a `WorkerChannel` with `send(frame)`, a receive callback, `terminate()` and an exit callback;
    - frames are length-prefixed, little-endian binary messages over the worker's stdin and stdout pipes;
    - a version handshake comes first, and heartbeats follow;
    - hosts implement it in `src/cli` and the desktop shell, and the iPadOS host reports it unsupported.
  - (b) A Unix-domain socket per worker.
  - (c) Shared memory for vectors, plus a pipe for control.
- Recommendation: (a). Pipes exist on both desktop hosts, need no file-system names or permissions, and close by themselves when the worker dies. Shared memory (c) can be added later behind the same frames if profiling shows that vector transfer dominates.
- Resolution: platform — option (a): a new platform service `platform::WorkerHost` starts a worker and returns a `WorkerChannel` (`send(frame)`, receive callback, `terminate()`, exit callback); frames are length-prefixed little-endian binary messages over the worker's stdin and stdout pipes, with a version handshake first and heartbeats after; `src/cli` and the desktop shell implement it, and the iPadOS host reports it unsupported. Approved by the user 2026-11-22.

### Q-SIM-22: In-process simulation on desktop
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`); enforcement-gap amendment approved 2026-11-25 (user decision of 2026-11-25)
- ngspice behavior: n/a. The `simulation-engine-embedding` requirement "Crash containment on desktop" says "A setting MAY select in-process execution on desktop, for debugging". Under Q-SIM-20 (a), the editor never links ngspice.
- Evidence: `openspec/specs/simulation-engine-embedding/spec.md` (Requirement "Crash containment on desktop")
- Options: (a) developer builds only: the developer desktop host loads the same `hefesto-ngspice-adapter` plugin that `hefesto-simd` loads (Q-SIM-20 (a)), so no adapter code and no `sharedspice.h` include is compiled into the editor, and release builds neither offer the setting nor load the plugin; (b) keep it as a user setting in release builds; (c) drop it on desktop.
- Recommendation: (a). Developers keep a debuggable path that matches the iPadOS in-process engine, the licence boundary check stays "only the worker, or the developer host, loads the adapter plugin", and users cannot lose unsaved layout work to an engine crash.
- Resolution: platform — option (a): in-process simulation on desktop exists only in developer builds, which load the same `hefesto-ngspice-adapter` plugin that `hefesto-simd` loads; release builds neither offer the setting nor load the plugin into the editor. Approved by the user 2026-11-22.
- Resolution (amendment, approved 2026-11-25): the enforcement gap of `scripts/check_engine_boundary.py` recorded under m9 task 4.3 (no tool checks yet that release builds do not offer the setting; the plugin rule finds only the literal plugin name) is accepted until SIMD task 5.4's release-configuration CI leg (`HEFESTO_DEVELOPER_BUILD` OFF, `HEFESTO_BUILD_NGSPICE=ON`, `hefesto-simd` built) runs the check without `--developer-build`. Approved by the user 2026-11-25.

### Q-SIM-23: Reference ngspice build for the SIM and SIMADV captures
- Kind: doc-mismatch
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`)
- ngspice behavior: n/a. The question is about Hefesto's capture setup.
  - The golden container builds stock ngspice `7aa47164` (`pre-master-48`) with `--enable-cider` and applies no Hefesto patches. It has no OpenVAF.
  - `tests/golden/SIM/CASES.md` requires "same source and patch level" as the engine.
  - `tests/golden/SIMADV/CASES.md` requires "the same portable-seed patch".
  - The IHP OSDI models need OpenVAF to compile.
- Evidence: `tools/golden/container/Containerfile:116-125`; `tools/golden/pins.toml:46-52`; `tests/golden/SIM/CASES.md:4`; `tests/golden/SIMADV/CASES.md:4`; `openspec/specs/simulation-engine-embedding/spec.md` (Requirements "ngspice build configuration and license hygiene" and "OSDI compact models")
- Options:
  - (a) The capture image builds ngspice from the engine's own pin (Q-SIM-7), with the Hefesto patch series and the spec's feature table (CIDER off). It adds a pinned OpenVAF to compile the IHP OSDI modules. The stock build is kept only for the existing `--with-ngshared` smoke test.
  - (b) Capture with stock ngspice, and record deviations for every seeded or OSDI case.
  - (c) Capture with Hefesto's own `hefesto-simd`.
- Recommendation: (a). The reference must be ngspice with the same source and patches as the engine. If Hefesto's engine were used (c), the captures would compare Hefesto with itself.
- Resolution: other — option (a): the capture image builds ngspice from the engine's pin (ngspice-47, `a80f6e3e`, Q-SIM-7) with the Hefesto patch series and the spec's feature table (CIDER off), as both the executable and `--with-ngshared`, and adds a pinned OpenVAF for the IHP OSDI modules; the stock build is kept only for the existing `--with-ngshared` smoke test; `tools/golden/pins.toml` moves from `7aa47164` to `a80f6e3e` (task 3.2), and roadmap principle 2 names ngspice-47 `a80f6e3e`. Approved by the user 2026-11-22.

### Q-SIM-24: Hierarchical node names for layout probes before M12
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`); naming and array-element amendment approved 2026-11-27 (user decision, recommended option; R3-1, R3-2)
- ngspice behavior: n/a. Background:
  - The `simulation-ui-and-crossprobe` requirement "Probing a layout net" names nets as `irsim-interface` "Hierarchical node names" does, which is Magic's `getnode` naming. `irsim-interface` is an M12 capability.
  - Magic's `SimGetNodeName` builds names with `strcpy` and `strcat` into the fixed buffers `static nodename[256]` and `buff[256]` (`sim/SimExtract.c:765-767`); a long hierarchical path or label overflows them. `SimTxtorLabel`'s `static name[30]` cannot overflow: it holds `@=%c%d,%d` with two 32-bit integers, at most 26 characters plus the terminator (`sim/SimExtract.c:357-366`).
  - The probe looks the `getnode`-style name up in the net-name map, whose entries come from the netlister. The two names can differ for the same node (`@=` terminal names, case-collision renames, a different preferred label), so the map needs a `layoutName` field computed with the same naming function, and a fallback by connectivity.
- Evidence: `openspec/specs/simulation-ui-and-crossprobe/spec.md` (Requirement "Probing a layout net"); `openspec/specs/simulation-netlist-preparation/spec.md` (Requirement "Net name map"); `openspec/specs/irsim-interface/spec.md`; Magic `sim/SimExtract.c:357-366`, `:734-767`; `sim/SimDBstuff.c:431`
- Options:
  - (a) Bring the naming function forward into M9, tagged to the `irsim-interface` requirement. Names have no length limit in both `magic-compat` modes (X-2 tier 1). The net-name map gets a `layoutName` computed with that function at each entry's layout point, and probes fall back to connectivity when the names differ (`simulation-ui-and-crossprobe` "Probing a layout net" and `simulation-netlist-preparation` "Net name map", deltas). The `getnode`, `rsim`, `startrsim` and `simcmd` commands stay in M12.
  - (b) Option (a), and also bring the `getnode` command forward.
  - (c) A Hefesto-only naming rule for probes.
- Recommendation: (a). Probing needs only the names. The command belongs with the rest of the IRSIM interface, which stays M12 (UI-CLIENT-002 is not retired).
- Resolution: platform — option (a): the hierarchical node-naming function is brought forward into M9, tagged to `irsim-interface` "Hierarchical node names", with no length limit in both `magic-compat` modes (X-2 tier 1); the net name map gets a `layoutName` field computed with that function at each entry's layout point, and a layout probe looks up `layoutName` first and falls back to connectivity when the names differ; `getnode`, `rsim`, `startrsim` and `simcmd` stay in M12. Approved by the user 2026-11-22.
- Resolution (amendment, approved 2026-11-27): the probe uses `getnode`'s normal naming (preferred label, else the extractor's generated name), never the positional `@=` names of `getnode fast`; a use-path array element outside the array's range fails with `SIM-PROBE-003` `no instance <element> in cell <cell>`; both in both `magic-compat` modes. `simulation-ui-and-crossprobe` "Probing a layout net", scenarios "Probe uses the normal node name" and "Array element outside the array". Approved by the user 2026-11-27.

### Q-SIM-25: Simulation input and transport limits
- Kind: bug
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`); job-log cap and interim result budget amendment approved 2026-11-27 (user decision, recommended option; R3-5, R3-6)
- ngspice behavior: ngspice bounds decks and results only by memory. The result budget of `simulation-analyses-and-results` (Q-SIM-16) covers vectors, but nothing bounds:
  - deck size;
  - include depth;
  - the IPC frames of Q-SIM-21;
  - the size of a run set;
  - the size of an imported raw file.
- Evidence: `openspec/specs/simulation-analyses-and-results/spec.md` (Requirement "Saved vectors and memory budget"); `openspec/specs/simulation-netlist-preparation/spec.md` (Requirement "Deck validation and sanitisation"); `openspec/specs/simulation-sweeps/spec.md`, `simulation-monte-carlo/spec.md`
- Options:
  - (a) Configurable limits, each with its own diagnostic:
    - prepared deck 256 MiB;
    - include depth 32;
    - IPC frame 64 MiB (vectors are sent in chunks);
    - worker start-up 10 s, then a heartbeat every 2 s, with a timeout of 10 s;
    - 100 000 points per run set and 100 000 Monte Carlo samples;
    - raw import 4 GiB, and never more than the result budget once loaded.
  - (b) Memory-only limits.
  - (c) The values of (a) as fixed limits.
- Recommendation: (a). The values are far above every PDK testbench and golden case. Hostile or runaway inputs fail with a message, and a stuck worker is detected and replaced.
- Resolution: fix — option (a): configurable limits, each with its own diagnostic: prepared deck 256 MiB; include depth 32; IPC frame 64 MiB (vectors in chunks); worker start-up 10 s, then a heartbeat every 2 s with a 10 s timeout; 100 000 points per run set and 100 000 Monte Carlo samples; raw import 4 GiB and never more than the result budget once loaded. Approved by the user 2026-11-22.
- Resolution (amendment, approved 2026-11-27): the job log kept in memory is capped at 64 MiB of text, a setting that can be lowered and never raised past twice the default (design D18); lines past the cap are dropped and counted (`simulation-analyses-and-results` "Job lifecycle", scenario "Job log capped in memory"; also under the user's standing rule for crafted-input limits in both modes). Interim value: the result budget is 4 GiB on desktop (512 MiB on iPadOS) until this entry decides it (`simulation-analyses-and-results` "Saved vectors and memory budget", scenario "Default result budget"). Approved by the user 2026-11-27.

### Q-SIM-26: When D-28 must be settled
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`)
- ngspice behavior: n/a. D-28 is about how the LGPL and MPL parts are linked into the iPadOS app; its priority is "blocks M9 (must be settled before M14)", which matches the roadmap's M9 "Early de-risking" line ("must be decided before M14"). There is no conflict between the two. What is new is the desktop question that M9 raises (Q-SIM-20), because Desktop Beta ships `hefesto-simd`. The draft resolution of Q-SIM-1 says "confirm in D-28 before M9".
- Evidence: `docs/roadmap.md` (M9 "Early de-risking" and the de-risking schedule); `docs/decisions/audit/DECISIONS.md` D-28; Q-SIM-1
- Options:
  - (a) Decide the desktop linking (Q-SIM-20) in R0 and put it through D-28's legal review before the Desktop Beta. The iPadOS choice (static with a relink offer, or an embedded framework) stays where D-28 puts it, before M14, after the same review. The roadmap and D-28 gain a sentence naming the desktop part.
  - (b) Decide the desktop and iPadOS linking together before M9 implementation starts.
  - (c) Decide everything before M14, and ship the Desktop Beta without a recorded desktop decision.
- Recommendation: (a). The desktop build is the one M9 ships, and its boundary does not depend on how the iPadOS question is answered.
- Resolution: other — option (a): the desktop linking (Q-SIM-20) is decided in R0 and goes through D-28's legal review before the Desktop Beta; the iPadOS choice stays where D-28 puts it, before M14, after the same review; the roadmap and D-28 name the desktop part. Approved by the user 2026-11-22.

### Q-SIM-27: Desktop Beta sign-off
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`); layout amendment approved 2026-11-25 (user decision of 2026-11-25)
- ngspice behavior: n/a. The roadmap defines Desktop Beta as "the complete flow (schematic → simulation → layout → DRC → GDS → extraction → LVS → post-layout simulation) on Linux and macOS". No checklist exists, unlike the Desktop Alpha checklist of M4 (`docs/roadmap.md`, M4 status, task 13.6 of m4-scripting).
- Evidence: `docs/roadmap.md` (M4 and M9 sections); `openspec/changes/m4-scripting/tasks.md` 13.6
- Options:
  - (a) A beta flow per PDK, checked by end-to-end tests in CI on Linux x86_64 and macOS arm64 and then signed off by the user on real pointers, like Desktop Alpha. The flows are sky130 `inv_1`, gf180mcu `gf180mcu_fd_sc_mcu7t5v0__inv_1` and IHP `sg13g2_inv_1`. Each flow runs these steps:
    1. an inverter schematic `beta_inv.sch` with the devices and sizes of the library inverter is drawn in Hefesto (by the flow's script in CI, by hand for the sign-off), placed in a shared testbench `beta_tb.sch` with a pulse input and a fixed FO4 load (four copies of the same inverter), and simulated (`tran`, `tpd` as the mean of the 50 %–50 % rise and fall delays);
    2. a layout cell `beta_inv` is drawn in Hefesto: an instance of the library inverter plus Hefesto-drawn metal1 pins and labels; it has no errors under Hefesto's DRC with the PDK's Magic tech file, and is written to GDS (KLayout sign-off DRC, P-18, is not part of the Beta flow);
    3. the layout is extracted;
    4. LVS of `beta_inv` against `beta_inv.sch` matches (for IHP, the PDK's `sg13g2_inv_1.sch` is also compared, as a cross-check);
    5. the post-layout simulation of `beta_inv` with parasitic mode `rc` runs in the same testbench, and its `tpd` is within 30 % of the schematic run's.
  
  The CI scripts and inputs are authored by GOLDEN from the specs; the user redraws both cells for the sign-off.
  - (b) sky130 only.
  - (c) CI only, with no user sign-off.
- Recommendation: (a). All three PDKs are first-class from M1 on, the inverters exist in every library, every roadmap step runs inside Hefesto, and the 30 % band with a fixed FO4 load catches a broken parasitic path without becoming a timing sign-off. An unloaded output would let parasitics alone move the delay by more than 30 %.
- Resolution: other — option (a): Desktop Beta is signed off on one flow per PDK (sky130 `inv_1`, gf180mcu `gf180mcu_fd_sc_mcu7t5v0__inv_1`, IHP `sg13g2_inv_1`) on Linux x86_64 and macOS arm64: an inverter schematic `beta_inv.sch` drawn in Hefesto and simulated in a shared testbench with a fixed FO4 load (`tran`, `tpd` the mean of the 50 %–50 % rise and fall delays); a layout cell `beta_inv` drawn in Hefesto, clean under Hefesto's DRC with the PDK's Magic tech file and written to GDS (KLayout sign-off is not part of Beta); extraction; an LVS match against `beta_inv.sch`; post-layout `rc` simulation in the same testbench with `tpd` within 30 % of the schematic run. Each flow is an end-to-end CI case, followed by the user's sign-off on real input. Approved by the user 2026-11-22.
- Resolution (amendment, approved 2026-11-25): the layout cell `beta_inv` is drawn entirely in Hefesto: its two transistors, with the devices and sizes of the library inverter, and its diffusion, poly, contacts and metal1, plus the metal1 pins and labels `A`, `Y`, power and ground; it holds no instance of the PDK inverter cell. This replaces "an instance of the library inverter" in option (a) step 2. SIMREF draws the cells under `tests/e2e/beta/<pdk>/` (m9 design D19 "Beta flow inputs", task 20.2). Approved by the user 2026-11-25.

### Q-SIM-28: Simulation performance gate
- Kind: platform
- Status: approved 2026-11-22 (proposed 2026-11-22 by the M9 plan; approved by the user 2026-11-22, `docs/decisions/m9-decision-table.md`)
- ngspice behavior: n/a. The solver time is ngspice's own. Hefesto adds deck preparation, IPC transfer and result storage. `hefesto-performance` sets no target for simulation, apart from the waveform rendering requirement of `simulation-ui-and-crossprobe`.
- Evidence: `openspec/specs/hefesto-performance/spec.md`; `openspec/specs/simulation-ui-and-crossprobe/spec.md` (Requirement "Waveform rendering performance")
- Options:
  - (a) Two gates:
    - Hefesto overhead, meaning the whole job time minus ngspice's own analysis time as reported by ngspice's `rusage` command (not the worker process's resource usage, which includes Hefesto's adapter and transfer time), is at most 10 % of ngspice's time on three reference decks: the sky130 inverter `tran`, a 1 000-device post-layout `rc` deck, and a 1 000-point sweep (including the worker reuse steps between points);
    - a 1 000-sample Monte Carlo of the inverter scales by at least 0.6 × the worker count for every `w` up to `min(4, physical_cores − 1)` of the measuring runner: up to 3 on the 4-vCPU Linux runner, 2 on the 3-vCPU macOS runner, and 4 on the M3 Pro bench machine or a larger runner.
  - (b) Wall time only, reported for information.
  - (c) No gate in M9.
- Recommendation: (a). It measures what Hefesto controls, it is stable across runners because it is a ratio, and it catches a slow IPC path.
- Resolution: platform — option (a): Hefesto overhead (job time minus ngspice's own `rusage` analysis time) at most 10 % of ngspice's time on the three reference decks; a 1 000-sample Monte Carlo of the inverter scales by at least 0.6 × `w` for every `w` up to `min(4, physical_cores − 1)` of the measuring runner. Approved by the user 2026-11-22.
