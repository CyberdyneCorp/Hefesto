# Open Questions: DIGITAL area

These entries cover the capabilities `digital-flow-block-import`, `digital-flow-run`, `digital-flow-results` and `digital-flow-mixed-signal`, which implement decision P-17 (digital flow integration, desktop-only).

The reference is LibreLane, not Magic: `librelane/librelane` @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (version 3.0.17). The "Magic behavior" field below therefore describes **LibreLane behavior** (or the behavior of the tools it drives), and "Evidence" cites LibreLane sources.

### Q-DIGITAL-1: Digital flow on iPadOS
- Kind: platform
- Magic behavior: LibreLane is a Python program that runs Yosys, OpenROAD, Magic, KLayout and Netgen as subprocesses, natively, through Nix, or in a container. iPadOS has no `fork`/`exec`, no Python runtime for third-party packages and no container engine.
- Evidence: librelane/steps/step.py:1290-1370, librelane/__main__.py:380-410
- Options: platform alternative (no flow on iPad; blocks and run results usable read-only) | remote execution on a user-provided server | none
- Recommendation: follow P-13 and P-17. On iPadOS, running the flow, importing or updating blocks, and exporting macro packages are unavailable. Blocks stored in a project load, display, place, and take part in DRC, extraction and LVS. Run directories stored in a project are browsable read-only. Revisit remote execution only after the desktop flow ships.
- Resolution: platform — the digital flow is desktop-only; on iPadOS flow runs, block import/update and macro export are unavailable, while stored blocks load, place and take part in DRC, extraction and LVS, and stored run directories are browsable read-only (P-13, P-17, 2026-11-07).

### Q-DIGITAL-2: Who provisions the PDK for a run
- Kind: ambiguity
- Magic behavior: by default LibreLane uses Ciel to download and enable the PDK version pinned in `pdk_hashes.yaml`. With `--manual-pdk` it uses the PDK found under `--pdk-root` as is.
- Evidence: librelane/flows/cli.py:426-470, librelane/pdk_hashes.yaml:1-3
- Options: always `--manual-pdk` with Hefesto's PDK store | let LibreLane/Ciel manage PDKs in its own root | offer both
- Recommendation: always `--manual-pdk`, so the PDK manager (P-14) is the single source of PDKs and runs never download silently. Warn on a commit mismatch and record both commits.

### Q-DIGITAL-3: The run directory is not a stable interface
- Kind: ambiguity
- Magic behavior: LibreLane documents `final/` and the metrics, but step directory names, `state_out.json`, report file names and log names are internal and change between versions (for example LibreLane 3.0 renamed and removed several design formats).
- Evidence: librelane/flows/flow.py:720-735, librelane/state/design_format.py:70-110, librelane/steps/step.py:1146-1235
- Options: parse defensively and pin a tested version set | require a machine-readable progress/report interface upstream
- Recommendation: parse defensively, with the tested version set of `digital-flow-run`. Propose upstream a stable, versioned run manifest (step list, status, report paths).

### Q-DIGITAL-4: Magic DRC report counts rectangles, not errors
- Kind: quirk
- Magic behavior: LibreLane's `drc.magic.rpt` lists every rectangle returned by `drc listall why`, and its own footer says the count "Should be divided by 3 or 4". The metric `magic__drc_error__count` is the number of rectangles, so one geometric violation can count several times.
- Evidence: librelane/scripts/magic/drc.tcl:55-95, librelane/steps/magic.py:530-560
- Options: replicate (one entry per rectangle) | fix (merge touching rectangles of the same rule into one violation) | show both
- Recommendation: list one entry per distinct rectangle with a multiplicity for exact duplicates (as specified), keep the metric value unchanged for comparison, and consider an optional "merge adjacent" view.

### Q-DIGITAL-5: Blocks without a GDS view
- Kind: ambiguity
- Magic behavior: LibreLane requires both a GDS and a LEF for every macro and fails otherwise. Magic itself can write GDS from abstract LEF views, producing a layout with only pins and obstructions.
- Evidence: librelane/config/variable.py:172-181
- Options: refuse import without GDS | import abstract-only and refuse GDS/macro export of any layout containing it | allow export with a warning
- Recommendation: import abstract-only with a warning (useful for floorplanning), and refuse GDS export and macro export while such a block is present, as specified.

### Q-DIGITAL-6: Bus-bit notation across views
- Kind: ambiguity
- Magic behavior: LEF, DEF and Verilog use `name[i]` (DEF and Verilog may escape the name), Magic labels and netgen often use `name<i>` or `name[i]`, and Magic's LEF annotation swaps `[]` and `<>` once when looking up a pin.
- Evidence: librelane/steps/magic.py:234-270; capability `lef-read`, Requirement "Annotation of existing cells"
- Options: replicate the single swap | normalize all bus notation to one canonical form internally
- Recommendation: replicate the LEF-annotation rule for compatibility, and compare names across views by canonical `name[i]` form in the consistency checks.

### Q-DIGITAL-7: LibreLane's Magic steps and Hefesto
- Kind: ambiguity
- Magic behavior: LibreLane runs Magic headless for stream-out, LEF writing, DRC, SPICE extraction and antenna checks, using Tcl scripts and environment variables.
- Evidence: librelane/steps/magic.py:175-183, librelane/scripts/magic/
- Options: leave Magic in the flow | offer `hefesto-cli` as a drop-in replacement for those steps (through capability `hefesto-magic-compat-mode`) | contribute a Hefesto step plug-in to LibreLane
- Recommendation: out of scope for the first release. Track it once `hefesto-cli` passes the golden compatibility tests for GDS, LEF, DRC and extraction.

### Q-DIGITAL-8: Preserving configuration formatting
- Kind: ambiguity
- Magic behavior: LibreLane accepts JSON, YAML and legacy Tcl configuration files with preprocessor expressions and `pdk::`/`scl::` conditional sections; it does not rewrite them.
- Evidence: librelane/config/config.py:113-125, librelane/config/preprocessor.py:223-300
- Options: full-fidelity editing (keep whitespace and comments) | key-order and value preservation with normalized formatting | read-only Tcl
- Recommendation: as specified — preserve key order, untouched values and YAML comments; normalize JSON indentation to four spaces; keep Tcl read-only.

### Q-DIGITAL-9: Bundling the flow
- Kind: ambiguity
- Magic behavior: LibreLane is Apache-2.0; it depends on tools under other licenses (OpenROAD BSD-3-Clause, Yosys ISC, Magic, Netgen and KLayout GPL) and is distributed mainly through Nix and containers.
- Evidence: License, flake.nix, nix/
- Options: bundle a pinned flow | drive a user-installed flow | offer a guided installer that runs the official Nix or container install
- Recommendation: do not bundle (licensing, size, update cadence). Offer a guided installer later, recorded as its own decision.

### Q-DIGITAL-10: Timing views and static timing analysis
- Kind: ambiguity
- Magic behavior: LibreLane uses LIB, SPEF and SDF views of macros for hierarchical STA in OpenSTA; Magic does no timing analysis.
- Evidence: librelane/config/variable.py:107-160
- Options: keep timing views as attachments only | integrate OpenSTA as an external process | implement STA
- Recommendation: attachments only for now, as specified; revisit with a product decision if mixed-signal timing sign-off is requested.
