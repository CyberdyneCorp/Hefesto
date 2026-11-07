# Open Questions — SIM (embedded ngspice simulation)

Capabilities: `simulation-engine-embedding`, `simulation-netlist-preparation`, `simulation-analyses-and-results`, `simulation-ui-and-crossprobe`, `simulation-command`. Product decision: `docs/decisions/product.md#P-7`.

These capabilities are `native` and have no Magic counterpart. "Evidence" therefore points at the ngspice source: SourceForge `ngspice` master @ `7aa47164`, development after ngspice-47 (2026-08-11). That tree was cloned read-only for this research; it was not built or run.

### Q-SIM-1: [platform] LGPL components statically linked into the iPadOS app
- Kind: platform
- ngspice behavior: ngspice is Modified BSD, but `src/maths/KLU` is LGPLv2, `src/frontend/numparam` is LGPLv2 or newer, `src/osdi` is MPL 2.0, and `src/xspice/icm/table` is GPLv2 or newer. numparam is part of the core (`.param` expressions), so it cannot be switched off. iPadOS forbids loading dynamic libraries that are not embedded and signed, and App Store distribution complicates the LGPL relinking requirement.
- Evidence: COPYING:1-45
- Options: (a) ship static, with object files and relink instructions offered on request (the usual LGPL static-linking compliance); (b) embed KLU and numparam as a signed dynamic framework inside the app bundle (allowed on iOS); (c) build without KLU (Sparse 1.3 solver only) and ask upstream to relicense numparam; (d) legal review before deciding.
- Recommendation: (b) for KLU and numparam as one embedded framework, if legal review agrees. Otherwise (a). Exclude the `table` code models (GPL) in any case, as the spec already requires.
- Resolution: platform — draft following D-28's recommendation: legal review first, then KLU and numparam as one embedded, signed dynamic framework, or static linking with object files and relink instructions on request if the review says so; the GPL `table` code models are excluded in every case; confirm in D-28 before M9 (D-28, D-5 draft, 2026-11-07).

### Q-SIM-2: [platform] OSDI / Verilog-A models on iPadOS
- Kind: platform
- ngspice behavior: OSDI models are shared objects compiled by OpenVAF. ngspice loads them with `dlopen(path, RTLD_NOW|RTLD_LOCAL)` and looks up the generic symbols `OSDI_DESCRIPTORS`, `OSDI_NUM_DESCRIPTORS`, `OSDI_VERSION_MAJOR/MINOR` and `OSDI_DESCRIPTOR_SIZE`. It accepts OSDI 0.3 (original OpenVAF) and 0.4 (OpenVAF-reloaded; 0.5 is experimental). iPadOS App Store builds cannot load code the user supplies, OpenVAF (Rust + LLVM) cannot run on device, and every module exports the same symbol names, so several modules cannot be linked statically as they are.
- Evidence: src/osdi/osdiregistry.c:32, src/osdi/osdiregistry.c:340-470, src/osdi/osdi.h:20-21
- Options: (a) a Hefesto patch adding a static OSDI registry: compile each module at build time with symbol prefixing (for example `objcopy --prefix-symbols`, or an OpenVAF option if one exists) and register the descriptors through a table; (b) upstream such a static-registration API to ngspice; (c) no OSDI on iPad, which drops IHP sg13g2 support there.
- Recommendation: (a) now, (b) in parallel. Decide the built-in module list (PSP 103.x, `r3_cmc`, `mosvar`, HICUM L2, and possibly BSIM-CMG and EKV) and check each model's license for redistribution in binary form.
- Resolution: platform — a Hefesto patch adds a static OSDI registry: each module is compiled at build time with prefixed symbols and registered through a table; the same API is proposed upstream; the built-in module list is the one of `simulation-engine-embedding`, each model's license checked for binary redistribution (P-7, P-13, D-5, approved 2026-11-07, 2026-11-07).

### Q-SIM-3: [platform] XSPICE code models without dlopen
- Kind: platform
- ngspice behavior: XSPICE code-model libraries (`analog.cm`, `digital.cm`, `spice2poly.cm`, `xtradev.cm`, `xtraevt.cm`, `table.cm`) are shared objects loaded with `dlopen` by the `codemodel` command, which is normally issued from `spinit`. Upstream has no option to link them statically.
- Evidence: src/spicelib/devices/dev.c:290-420, cmake/NgspiceOptions.cmake:21-22
- Options: (a) a Hefesto patch that links the `.cm` sources statically and registers them at `ngSpice_Init`; (b) leave XSPICE out on iPad (no `spice2poly`, which breaks `E`/`G` POLY sources used by some PDK models); (c) upstream.
- Recommendation: (a). Verify that no supported PDK model needs `table.cm` (GPL).

### Q-SIM-4: [platform] Crash containment on iPadOS
- Kind: platform
- ngspice behavior: unrecoverable errors are handled by `longjmp` and `ControlledExit`. A real crash (SIGSEGV or an abort inside a model) ends the host process. On iPadOS the host process is the editor, because no helper process can be spawned.
- Evidence: src/sharedspice.c:2134-2190, src/sharedspice.c:914
- Options: (a) in-process with a mandatory autosave before each run (current spec); (b) run ngspice in an app extension, if a suitable extension point exists (probably not for general compute); (c) an optional remote engine (a desktop or server `hefesto-simd` over the network); (d) a WebAssembly build of ngspice in a sandboxed runtime. That is in-process but memory-isolated, roughly 2–5× slower, and has no OSDI unless the models are compiled to wasm.
- Recommendation: (a) as baseline, with (c) as an option later. Evaluate (d) if crash reports show ngspice crashes are common.

### Q-SIM-5: Single instance per process; parallelism
- Kind: platform
- ngspice behavior: all simulator state is global (`ft_curckt`, the `fl_running` and `fl_exited` thread flags, status statics). Upstream's way to run in parallel is to load separately named copies of the shared library, using the `ident` argument of `ngSpice_Init_Sync`.
- Evidence: src/sharedspice.c:390-470, src/include/ngspice/sharedspice.h:330-345
- Options: (a) worker processes on desktop and serial execution on iPad (current spec); (b) `dlmopen` namespaces on Linux and renamed dylib copies on macOS; (c) refactor ngspice to an instance struct (large upstream work).
- Recommendation: (a). Review the default worker count (`min(4, cores − 1)`) against memory use with large RC netlists.

### Q-SIM-6: ngspice lowercases names; layout names are case-sensitive
- Kind: ambiguity
- ngspice behavior: the netlist parser lowercases node, instance and model names. Vector names come back lowercase. Magic node and label names are case-sensitive, so `A` and `a` are different nets in layout and the same net in ngspice. Device-current vector names (`@m.xdut.xm1[id]`) depend on how the PDK wraps devices in subcircuits. For sky130, devices are `X` instances of subcircuit wrappers, and the primitive path inside the wrapper must be confirmed.
- Evidence: src/frontend/inpcom.c (case folding during input preparation), src/frontend/rawfile.c:160-200
- Options: replicate (lowercase everywhere and let nets merge silently, which is wrong) | rename on collision (current spec: `<name>__c<k>`) | refuse to simulate.
- Recommendation: rename on collision, as specified. Capture golden cases SIM-NET-03 and SIM-UI-04 to confirm vector naming of nested subcircuit nodes and of device currents.

### Q-SIM-7: ngspice version pin and the new CMake build
- Kind: ambiguity
- ngspice behavior: the master tree contains a CMake build (`CMakeLists.txt`, `cmake/Ngspice*.cmake`, with `NGSPICE_BUILD_SHARED`). The latest release (ngspice-47, August 2026) may still be autotools-only. There is no iOS toolchain support, and there is no option to build a static `libngspice`; the CMake option builds a shared one.
- Evidence: CMakeLists.txt:150-260, cmake/NgspiceOptions.cmake:86-91, NEWS:1
- Options: pin ngspice-47 and write Hefesto's own CMake wrapper | pin a master commit that has the CMake build | wait for ngspice-48.
- Recommendation: pin the first release that has the CMake build, and carry Hefesto patches for the static archive, the static code-model and OSDI registries, and iOS. Offer these upstream.

### Q-SIM-8: PDK simulation model descriptors need verification
- Kind: doc-mismatch
- ngspice behavior: not applicable. The descriptor contents come from open_pdks and the IHP repository. The values in the spec are from memory:
  - sky130: `sky130.lib.spice` sections `tt ff ss sf fs`, plus `.spiceinit` with `set ngbehavior=hsa` and `set ng_nomodcheck`
  - gf180mcu: `design.ngspice`, plus `sm141064.ngspice` sections `typical ff ss fs sf`
  - sg13g2: `cornerMOSlv.lib` (`mos_tt` …), `cornerMOShv.lib`, `cornerHBT.lib`, `cornerRES.lib` and `cornerCAP.lib`, plus `.spiceinit` loading `psp103_nqs.osdi`, `r3_cmc.osdi` and `mosvar.osdi`
- Evidence: none checked locally; no PDK is installed on the research machine.
- Options: verify against open_pdks and IHP-Open-PDK at the pinned versions; generate descriptors automatically from `libs.tech/ngspice/.spiceinit` and the corner files.
- Recommendation: verify (cross-cutting X-6 already requires the open_pdks sources). Also confirm the section names for HBT, RES and CAP in sg13g2, and whether sky130 still needs `ng_nomodcheck`.

### Q-SIM-9: `.control` sections and the `shell` command
- Kind: platform
- ngspice behavior: the `.control` language is a full interpreter. It includes `shell` (runs a host command through `system()`), `cd`, `load`, `source`, `write` and `codemodel`/`osdi` (load code). In shared mode, `.control` commands run on the caller's thread right after the circuit is parsed.
- Evidence: src/frontend/commands.c:188-520, src/sharedspice.c:24-40
- Options: strip `.control` by default and block dangerous commands (current spec) | run `.control` fully on desktop only | add a Hefesto-side `.meas`/script replacement.
- Recommendation: as specified. Review App Store guideline 2.5.2 (interpreted code) for the remaining `.control` subset on iPad, and consider allowing only `let`, `meas`, `print` and `alter`.

### Q-SIM-10: Cross-platform numeric reproducibility
- Kind: ambiguity
- ngspice behavior: results depend on floating-point evaluation (FMA contraction on arm64 compared with x86_64, libm differences between glibc and Apple libm) and on the time-step control path. Small differences can change accepted time steps and, through them, waveforms near sharp edges.
- Evidence: not applicable (numerical behavior)
- Options: tolerance-based comparison (current spec: 1e-3 relative plus an absolute floor) | build with `-ffp-contract=off` and a bundled libm for bitwise equality.
- Recommendation: build with `-ffp-contract=off` on all platforms and keep tolerance-based golden comparison. Re-tune the tolerances from the golden cases SIM-XP-*.

### Q-SIM-11: Recovery after an ngspice error
- Kind: quirk
- ngspice behavior: after an error, `shared_exit` does a `longjmp` back into `ngSpice_Command` or `ngSpice_Circ`, or calls `pthread_exit` on the background thread, and tells the caller to "reset or detach" the library. A statically linked library cannot be detached, and it is unclear whether `ngSpice_Reset` (`totalreset`) restores every global after an arbitrary error.
- Evidence: src/sharedspice.c:2134-2190, src/sharedspice.c:1473-1479, src/sharedspice.c:2534-2545
- Options: trust `ngSpice_Reset` (current spec, with a second fault making the engine `unavailable`) | always restart the worker after any fault on desktop.
- Recommendation: on desktop, always replace the worker after a fault (cheap and safe). On iPad, use `ngSpice_Reset` as specified, and fuzz-test reset after error decks (SIM-ENG-07).

### Q-SIM-12: [platform] OpenMP and performance on Apple platforms
- Kind: platform
- ngspice behavior: OpenMP parallelises device loading (`USE_OMP`). Apple clang ships no OpenMP runtime, and none is available on iPadOS.
- Evidence: cmake/NgspiceOptions.cmake:53
- Options: OpenMP off on Apple (current spec allows it only on Linux, and optionally on macOS through a bundled libomp) | bundle LLVM libomp on macOS.
- Recommendation: off on iPadOS. Measure on macOS before bundling libomp.

### Q-SIM-13: [platform] `$VAR` and host paths inside PDK model files
- Kind: platform
- ngspice behavior: `.include`/`.lib` names that start with `$VAR` (one or two variables) are expanded with `getenv`. Relative paths are tried against the including file's directory, then against the `sourcepath` list. On iPadOS there are no meaningful environment variables, and PDK files live in the sandbox.
- Evidence: src/frontend/inpcom.c:2040-2130, src/frontend/inpcom.c:2160-2314
- Options: Hefesto resolves and rewrites all paths before ngspice sees them (current spec) | set environment variables in-process with `setenv` before loading.
- Recommendation: rewrite, as specified. It is deterministic and does not depend on process-global environment state.
- Resolution: platform — Hefesto resolves and rewrites `.include`/`.lib` paths against the PDK store before ngspice reads the deck; no environment variables are consulted (X-4, hefesto-files-and-pdks, 2026-11-07).

### Q-SIM-14: Progress reporting tracks only two thread identities
- Kind: quirk
- ngspice behavior: progress tracking remembers at most two thread ids. A third thread (a new `bg_run` thread after earlier runs) takes over slot 2. Status strings are throttled to 150 ms, and are suppressed when unchanged. The `tran` status reports `--ready--` once `time > tstop − tmax`.
- Evidence: src/sharedspice.c:1955-2110
- Options: rely on status strings only | combine them with the streamed time point (current spec does this for `tran`).
- Recommendation: as specified. Check in golden case SIM-RUN-03 that progress keeps working on the third and later runs in one engine.

### Q-SIM-15: Callback threads and print thread latency
- Kind: quirk
- ngspice behavior: with `low_latency` defined (the default), output text is sent synchronously from the calling thread. Otherwise a separate print thread batches output. Callbacks can therefore arrive on the caller's thread, the background thread, or the print thread.
- Evidence: src/sharedspice.c:18-22, src/sharedspice.c:1100-1110
- Options: none needed. The spec requires thread-agnostic, non-blocking handlers that copy their data.
- Recommendation: keep the default `low_latency` build.

### Q-SIM-16: Default result memory budgets
- Kind: ambiguity
- ngspice behavior: ngspice grows vectors without limit, and memory is reported only for information.
- Evidence: src/sharedspice.c (usage reporting)
- Options: 512 MiB on iPad and 4 GiB on desktop (current spec) | a fraction of physical memory.
- Recommendation: define the budget relative to the iPad memory bound of capability `hefesto-performance` (3 GB in total, shared with layout and GPU). Revisit after profiling.

### Q-SIM-17: Raw file header fields are not reproducible
- Kind: quirk
- ngspice behavior: raw files contain `Date:` (wall clock) and `Command: ngspice-<ver>, Build <date>`, and the binary data is host byte order (`fwrite` of `double`).
- Evidence: src/frontend/rawfile.c:114-130, src/frontend/rawfile.c:224-258
- Options: golden comparisons ignore `Date:` and `Command:` lines (recommended) | Hefesto writes a fixed date in test mode.
- Recommendation: ignore those lines when comparing. Hefesto always writes little-endian data, which is native on all supported targets.

### Q-SIM-18: [platform] IRSIM replacement on iPadOS
- Kind: platform
- ngspice behavior: XSPICE digital code models and event-driven nodes (`ngGet_Evt_NodeInfo`, `ngSpice_Init_Evt`) could provide switch- or logic-level simulation in-process. IRSIM has no in-process path on iPad (Q-LEGACY-71).
- Evidence: src/include/ngspice/sharedspice.h:185-210, src/include/ngspice/sharedspice.h:395-430
- Options: no switch-level simulation on iPad | embed IRSIM as a library (it is not designed for that) | map `.sim` to an XSPICE digital netlist.
- Recommendation: defer. Decide together with Q-LEGACY-71.
- Resolution: platform — no switch-level simulation on iPadOS; IRSIM is desktop-only and embedded ngspice provides simulation on every platform (same answer as Q-LEGACY-71) (X-4, P-13, 2026-11-07).

### Q-SIM-19: [platform] Background execution on iPadOS
- Kind: platform
- ngspice behavior: not applicable (an iPadOS lifecycle question). iPadOS suspends apps in the background, so a long simulation cannot keep running when the user switches apps.
- Evidence: not applicable
- Options: pause on background (current spec) | request a background-processing task (time-limited and not guaranteed) | a remote engine.
- Recommendation: pause and resume, as specified. Show an in-app notice when a run is paused.
