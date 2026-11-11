# Hefesto iPad Feature Scope

## Purpose
Defines which features the first iPadOS release includes and which stay desktop-only (decision P-13). This list is authoritative: a capability not listed as desktop-only here SHALL be available on iPadOS. The excluded features fall into three groups: those that need external processes, those that are legacy, and those whose workflows suit the desktop better.

## Requirements

### Requirement: Included feature set
The first iPadOS release SHALL provide the following, with the same results as on the desktop:
- **Layout and DRC:** viewing and editing layout (with touch and Apple Pencil), technology loading, continuous and on-demand DRC, PDK device generators (PCells), and the wiring tool.
- **Scripting:** the embedded, sandboxed Tcl interpreter and PDK startup scripts (`hefesto-scripting`), subject to its App Store fallback to curated packs.
- **Schematics and simulation:** schematic editing and netlisting (`schematic-*` capabilities), and ngspice simulation with waveforms and cross-probing (`simulation-*`).
- **Extraction and LVS:** extraction and netlist generation (`extract-*`, `ext-flattening-*`, `netlist-spice-*`, `netlist-sim`, `extresist-*`), LVS (`lvs-*`), and the 3D view with overlays (`hefesto-3d-visualization`, `view-3d`).
- **File I/O:** reading and writing GDS, OASIS, CIF, LEF and DEF, and the tapeout capabilities (`tapeout-*`). An exception is sign-off steps that require KLayout rule decks: on iPadOS they are not run, and the design is not marked sign-off ready until they pass on a desktop (decision P-18).
- **Interactive routing:** the maze router and `iroute` (`router-maze`, `router-maze-iroute`).
- **Agent:** the AI layout agent (`agent-*`), with cloud, on-prem and offline providers.
- **Advanced simulation and reliability:** sweeps, corners, Monte Carlo, measurements and statistics, and the optimization hooks (`simulation-sweeps`, `simulation-corners`, `simulation-monte-carlo`, `simulation-measurements-and-statistics`, `simulation-optimization-hooks`; in-process, one simulation at a time), and EM/IR analysis with overlays (`reliability-*`), within the iPadOS memory budgets those capabilities state.
- **Plugins:** built-in plugins and sandboxed Tcl script plugins only (`plugin-*`). Native plugins are not loaded on iPadOS, and user-installed script plugins depend on App Review (capability `plugin-security-and-platforms`).
- **PDKs:** the PDK manager (`hefesto-pdk-manager`), which is the only way to install PDKs on iPadOS.
- **Digital blocks already in a project:** placing, checking, extracting and running LVS on imported hardened blocks (`digital-flow-mixed-signal`), and browsing stored flow results read-only (`digital-flow-results`).
- **Supporting capabilities:** plotting (`plot-output`), structural and visual diff, document sync, onboarding and help, settings, and App Store updates and opt-in diagnostics (`hefesto-product-operations`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-13, docs/decisions/product.md#P-17, docs/decisions/product.md#P-19, docs/decisions/product.md#P-20, docs/decisions/product.md#P-21)

#### Scenario: Full analog flow on iPad
- **GIVEN** an iPad with sky130A installed through the PDK manager
- **WHEN** a user draws a schematic, lays out the cell, runs DRC and LVS, extracts it, and simulates it with ngspice
- **THEN** every step SHALL complete on the iPad with the same results as on the desktop, under the comparison policy of `hefesto-output-determinism`

### Requirement: Desktop-only features
These features SHALL be desktop-only in the first iPadOS release:
- running the digital flow (`digital-flow-run`), which runs external tools, and importing or updating hardened blocks (`digital-flow-block-import`)
- the channel and gate-array routers and `net2ir` (`router-channel`, `router-gate-array`, `router-net2ir`)
- plowing (`plow`)
- the netlist window (`netlist-window`)
- IRSIM (`irsim-interface`)
- the Scheme interpreter (`scheme-interpreter`) and OpenAccess (`openaccess`), if they are ever implemented. Both capabilities are informative and Hefesto does not implement them on any platform (roadmap M12), so they are listed here only to keep them off iPadOS.
- git integration (the git parts of `hefesto-collaboration-and-diff`)

The `shell` command is not on this list: it runs no process on any platform (capability `cmd-misc-edit` "Shell command").

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-13)

#### Scenario: Desktop-only command on iPad
- **WHEN** a user or script runs `plow east` on iPadOS
- **THEN** the command SHALL fail with a diagnostic stating that plowing is available only in the desktop app, and the layout SHALL be unchanged

### Requirement: Graceful handling of desktop-only features
On iPadOS, the user interface SHALL NOT show controls for desktop-only features, or SHALL show them disabled with an explanation. Projects and files that use desktop-only features SHALL still open and edit on iPadOS. Examples are a digital-flow run directory and netlist-window `.net` files. Their data SHALL be preserved unchanged when saved.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-13)

#### Scenario: Project with digital-flow runs
- **GIVEN** a project created on desktop that contains LibreLane run results and a hardened macro
- **WHEN** it is opened on iPad
- **THEN** the macro SHALL be viewable and placeable, the run results SHALL be browsable read-only, and saving SHALL keep the run data unchanged

### Requirement: Scope changes are tracked
Moving a feature into or out of the iPadOS scope SHALL be recorded as a product decision and reflected in this capability. The roadmap SHALL list, for each iPadOS release, which desktop-only features it adds.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-13)

#### Scenario: Adding plow to iPad
- **WHEN** the team decides to support plowing on iPad
- **THEN** a product decision SHALL record it, `plow` SHALL be removed from the desktop-only list, and the roadmap SHALL name the release that adds it
