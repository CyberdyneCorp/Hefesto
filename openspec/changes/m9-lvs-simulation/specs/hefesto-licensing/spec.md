## MODIFIED Requirements

### Requirement: Two-team clean-room process for specs written from GPL source
Where a Hefesto spec was written by reading GPL source, the spec work and the implementation SHALL be done by two separate teams. (User decision 2026-11-13, judgement items J-1 and J-3 of `docs/decisions/m8-decision-table.md`.) Today this covers:
- the xschem-compatible schematic capabilities (`schematic-file-format`, `schematic-symbols-and-libraries`, `schematic-editing`, `schematic-netlisting`, `schematic-simulation-integration`, `schematic-command`), whose specs and open-question entries (`docs/decisions/open-questions/SCHEM.md`) were written from xschem's source;
- the netgen-compatible LVS capabilities (`lvs-netlist-input`, `lvs-setup-file`, `lvs-comparison-algorithm`, `lvs-device-reduction`, `lvs-property-matching`, `lvs-hierarchy`, `lvs-reports`, `lvs-command`), whose specs and open-question entries (`docs/decisions/open-questions/LVS.md`) were written from netgen's source (Q-LVS-30).

The process SHALL be:
- **Separate roles.** A contributor or agent session that read the GPL source of a tool SHALL NOT implement a capability whose spec it wrote or that cites that tool. An implementer SHALL NOT read that GPL source. A session that read netgen source SHALL NOT work on the LVS command or on the schematic-driven LVS parts of `schematic-simulation-integration`, which implement LVS behaviour.
- **No source references followed.** Implementers SHALL work from the spec text only. They SHALL NOT follow the `Evidence:` lines of open-question entries or the `(xschem ref: ...)` and `(netgen ref: ...)` traces of the specs, which exist for traceability only.
- **No source tree reachable.** No xschem or netgen source tree, clone, archive or code view SHALL be present on a machine, or in the context, of an implementation session for that tool's capabilities. Running the xschem or netgen binary in the golden capture runner is allowed: its outputs are black-box behaviour. Reading a tool's public user documentation is allowed.
- **Spec review for paraphrase.** Spec text SHALL describe behaviour, not code. Before an implementation workstream starts on a requirement, its text SHALL be reviewed for literal code paraphrase (source variable or function names that are not part of the file format or user interface, the structure of a GPL awk or Tcl program, pseudo-code that mirrors C), and any such text SHALL be rewritten as behaviour.
- **Hefesto-authored golden inputs.** Golden-case inputs SHALL be written by Hefesto contributors or taken from permissively licensed data such as the Apache-2.0 PDK libraries. Files from the GPL tool's own distribution (for example xschem's `xschem_library` or `tests` files, or netgen's test netlists) SHALL NOT be committed as inputs or expected outputs.
- **Interface data.** The interface facts of the clean-room system symbol library (capability `schematic-symbols-and-libraries`, Requirement "System library": file names, pin names, order and positions, and the netlisting properties `type`, `format`, `template`, `global`) have no public source other than xschem's GPL `devices/*.sym` files. They SHALL be extracted from those files by the spec side only, as a table in that spec, and implementers SHALL take them from that table. Symbol graphics SHALL be drawn independently.
- **Counsel review.** Legal counsel SHALL review this process once for each tool it covers, including the interface table above, before the Desktop 1.0 release. The review SHALL NOT block M8 or M9.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-9, docs/decisions/product.md#P-12) (See Q-LVS-30.)

#### Scenario: Implementer never sees xschem source
- **GIVEN** an agent or contributor session assigned to implement `schematic-netlisting`
- **WHEN** the session starts
- **THEN** no xschem source tree SHALL be reachable from it, and its instructions SHALL forbid following `Evidence:` paths and `(xschem ref: ...)` traces

#### Scenario: Implementer never sees netgen source
- **GIVEN** an agent or contributor session assigned to implement `lvs-comparison-algorithm`
- **WHEN** the session starts
- **THEN** no netgen source tree SHALL be reachable from it, and its instructions SHALL forbid following `Evidence:` paths and `(netgen ref: ...)` traces

#### Scenario: Spec author does not implement
- **GIVEN** a contributor who wrote a requirement of `schematic-netlisting` by reading xschem's source
- **WHEN** a pull request implementing that capability lists the contributor as its author
- **THEN** the review checklist SHALL reject it

#### Scenario: netgen reader stays off the LVS command
- **GIVEN** an agent session that read netgen's source while reviewing `lvs-hierarchy`
- **WHEN** it is assigned a task of the LVS command or of schematic-driven LVS
- **THEN** the assignment SHALL be refused under the roster rule

#### Scenario: GPL golden input refused
- **WHEN** a pull request adds a golden case whose input is a file from xschem's or netgen's own distribution
- **THEN** the review checklist SHALL reject it and ask for a Hefesto-authored input

#### Scenario: Counsel review before Desktop 1.0
- **WHEN** the Desktop 1.0 release checklist runs
- **THEN** it SHALL fail unless the counsel review of the two-team process for xschem and for netgen, and of the system-library interface table, is recorded

## ADDED Requirements

### Requirement: Process boundary for external engines
Hefesto SHALL keep each external engine's licence boundary visible in its build:
- **netgen.** netgen SHALL NOT be linked, vendored, distributed or started by any Hefesto product binary. It MAY run only as a subprocess in the golden-capture and benchmark containers, as a reference. netgen SHALL therefore not appear in the third-party notices.
- **ngspice on Linux and macOS.** ngspice SHALL be built as the shared library `libngspice-hefesto` and linked only by the engine adapter plugin `hefesto-ngspice-adapter`. In release builds only the worker executable `hefesto-simd` SHALL load that plugin; a developer build MAY also load it into the editor for debugging (capability `simulation-engine-embedding`). The editor, the headless command-line tool and the core library SHALL NOT link ngspice or the plugin.
- **Replaceable library.** A user SHALL be able to replace `libngspice-hefesto` with one rebuilt from the published source. On macOS, `hefesto-simd` SHALL therefore not enforce library validation, and the build documentation SHALL describe how to sign a rebuilt library so that it loads.
- **Legal review.** Before the Desktop Beta release, the desktop linking SHALL be confirmed by the legal review that decision D-28 calls for. The review's outcome SHALL be recorded with the release.
- **ngspice on iPadOS.** It SHALL be linked as decision D-28 settles.
- **Build manifest.** It SHALL record ngspice's source, patches and configure options. The source archive of each release SHALL include the ngspice sources it was built from, including the source of every MPL-2.0 file that is built (the OSDI tree), modified or not.

A CI check SHALL fail when an ngspice symbol appears in a binary other than `hefesto-ngspice-adapter` and `libngspice-hefesto`, when a release binary other than `hefesto-simd` references the adapter plugin, or when the ngspice API header is included outside the engine adapter.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-5, docs/decisions/product.md#P-7) (See Q-SIM-20, Q-SIM-22, Q-SIM-26, Q-LVS-30, D-28.)

#### Scenario: Engine boundary check
- **GIVEN** a change that calls `ngSpice_Command` from a file in the core library
- **WHEN** CI runs
- **THEN** the engine-boundary check SHALL fail and name the file

#### Scenario: Notices of a desktop release
- **WHEN** the third-party notices of a Linux release are generated
- **THEN** they SHALL list ngspice with KLU and its SuiteSparse parts (AMD, COLAMD, BTF, SuiteSparse_config), numparam, OSDI, sparse and the XSPICE code models, and SHALL NOT list netgen

#### Scenario: Rebuilt library on macOS
- **GIVEN** a notarized macOS install, and a `libngspice-hefesto` rebuilt by the user from the release's source archive and signed as the build documentation describes
- **WHEN** the user puts it in place of the shipped library and runs a simulation
- **THEN** `hefesto-simd` SHALL load it and the simulation SHALL run
