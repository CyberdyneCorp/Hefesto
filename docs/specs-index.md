# Hefesto Specification Index

Normative specs live in `openspec/specs/`; conventions in `docs/spec-conventions.md`; delivery plan in `docs/roadmap.md`.
Each area has an open-questions log (`docs/decisions/open-questions/<AREA>.md`) and a golden-case inventory (`tests/golden/<AREA>/CASES.md`).

**Totals:** 169 capabilities, 1981 requirements, 2964 scenarios, 838 open questions.

## Hefesto product (native)

Decisions: [product.md](decisions/product.md) · [cross-cutting.md](decisions/cross-cutting.md) · [roadmap](roadmap.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`hefesto-3d-visualization`](../openspec/specs/hefesto-3d-visualization/spec.md) | 5 | 6 | Defines Hefesto's 3D view of a layout's process stack. The baseline is parity with Magic's 3D window (capability `view-3d`). Beyond that, Hefesto adds interactive inspection, glTF/… |
| [`hefesto-app-ux`](../openspec/specs/hefesto-app-ux/spec.md) | 5 | 6 | Defines the information architecture of Hefesto's modern GUI: the main window, panels, browsers and multi-window behavior on desktop and iPad. Magic's Tk wrapper GUI is specified s… |
| [`hefesto-architecture`](../openspec/specs/hefesto-architecture/spec.md) | 6 | 7 | Defines how Hefesto is structured. A platform-independent C++20 core holds the layout database, all algorithms and the command API. Thin per-platform app shells provide windows, in… |
| [`hefesto-build-and-distribution`](../openspec/specs/hefesto-build-and-distribution/spec.md) | 4 | 5 | Defines how Hefesto is built with CMake, how third-party dependencies are managed, what CI verifies, and how Hefesto is packaged on each platform. |
| [`hefesto-collaboration-and-diff`](../openspec/specs/hefesto-collaboration-and-diff/spec.md) | 4 | 5 | Implements decision P-8. Hefesto provides: - structural and visual layout diff - a workflow that works with git: deterministic text formats and a merge helper - document sync throu… |
| [`hefesto-design-system`](../openspec/specs/hefesto-design-system/spec.md) | 7 | 9 | Defines Hefesto's visual design system, "forge" (decision P-23): - design tokens: color, typography, spacing, radius, elevation and motion - the icon style - the component patterns… |
| [`hefesto-diagnostics`](../openspec/specs/hefesto-diagnostics/spec.md) | 4 | 8 | Defines Hefesto's structured diagnostics model. Magic reports problems as free text, and the specs treat Magic's message wording as `informative`. Hefesto reports problems as struc… |
| [`hefesto-files-and-pdks`](../openspec/specs/hefesto-files-and-pdks/spec.md) | 3 | 5 | Defines how Hefesto provides Magic's file-system behavior (cell search paths, PDK discovery, locking, crash backups) on all platforms. This includes the iPadOS app sandbox, which h… |
| [`hefesto-input-and-touch`](../openspec/specs/hefesto-input-and-touch/spec.md) | 3 | 3 | Defines how mouse, keyboard, trackpad, touch and Apple Pencil input map onto Magic's tools, macros and commands. Every interaction ends up as a command of the single command API (s… |
| [`hefesto-ipad-feature-scope`](../openspec/specs/hefesto-ipad-feature-scope/spec.md) | 4 | 4 | Defines which features the first iPadOS release includes and which stay desktop-only (decision P-13). This list is authoritative: a capability not listed as desktop-only here SHALL… |
| [`hefesto-licensing`](../openspec/specs/hefesto-licensing/spec.md) | 5 | 8 | Defines Hefesto's licensing obligations. Hefesto's specifications are derived from several sources: - Magic, under a UC Berkeley permissive notice - netgen, under the GPL - KLayout… |
| [`hefesto-mag-extensions`](../openspec/specs/hefesto-mag-extensions/spec.md) | 3 | 3 | Defines how Hefesto stores data that Magic does not support in Magic's file formats without breaking Magic's ability to read them. Such data includes 3D view settings, annotations,… |
| [`hefesto-magic-compat-mode`](../openspec/specs/hefesto-magic-compat-mode/spec.md) | 5 | 7 | Implements decision X-2, generalized by decision D-2 to every reference tool that Hefesto must match. Hefesto always fixes the reference tools' crashes, undefined behavior and data… |
| [`hefesto-onboarding-and-help`](../openspec/specs/hefesto-onboarding-and-help/spec.md) | 4 | 4 | Defines how new users get started and how every user finds help (decision P-16). This covers: - a new-project wizard - project templates and example designs per PDK - interactive t… |
| [`hefesto-output-determinism`](../openspec/specs/hefesto-output-determinism/spec.md) | 5 | 8 | Implements decision X-1 (as amended on 2026-11-06). Hefesto reproduces Magic's ordering wherever that ordering is a deterministic function of the input: Magic's canonical tile deco… |
| [`hefesto-pdk-manager`](../openspec/specs/hefesto-pdk-manager/spec.md) | 19 | 29 | Defines the in-app PDK manager. On every platform, users browse, download, install, update, pin and remove open PDKs from inside Hefesto. On iPadOS this is the only way to get a PD… |
| [`hefesto-performance`](../openspec/specs/hefesto-performance/spec.md) | 4 | 4 | Sets measurable performance targets for interactive editing, rendering and batch operations. Each target has a reference workload, so CI benchmarks can catch regressions. The numbe… |
| [`hefesto-platform-targets`](../openspec/specs/hefesto-platform-targets/spec.md) | 4 | 6 | Defines what Hefesto is built from and where it runs: the language standard, the supported compilers, and the supported operating systems and architectures. Every other `hefesto-*`… |
| [`hefesto-product-operations`](../openspec/specs/hefesto-product-operations/spec.md) | 5 | 5 | Defines how Hefesto runs as a shipped product (decision P-22): - opt-in crash reporting and usage telemetry that never collect design data - signed desktop updates with stable and … |
| [`hefesto-rendering`](../openspec/specs/hefesto-rendering/spec.md) | 4 | 4 | Defines Hefesto's GPU rendering for the 2D layout view. One internal rendering interface sits over two backends. On the desktop (Linux and macOS) the backend is Vulkan, through Mol… |
| [`hefesto-scripting`](../openspec/specs/hefesto-scripting/spec.md) | 4 | 5 | Defines how Hefesto runs scripts and PDK integration code. Open PDKs drive Magic through Tcl startup files and the `toolkit.tcl` PCell generators (capabilities `scripting-api`, `sc… |
| [`hefesto-security`](../openspec/specs/hefesto-security/spec.md) | 4 | 4 | Defines Hefesto's security posture. Hefesto treats every opened file as untrusted data and confines script execution to a sandbox. It also requires user consent before a file trigg… |
| [`hefesto-settings`](../openspec/specs/hefesto-settings/spec.md) | 5 | 7 | Defines how Hefesto replaces Magic's `.magicrc` and environment-variable configuration with layered settings. The layers are application defaults, user preferences, PDK startup dat… |
| [`hefesto-testing-strategy`](../openspec/specs/hefesto-testing-strategy/spec.md) | 6 | 6 | Defines how Hefesto proves that it meets its specifications. Testing has these layers: - unit tests - golden compatibility tests against Magic, netgen and ngspice, using the canoni… |

## Technology files (TECH)

Open questions: [32](decisions/open-questions/TECH.md) · Golden cases: [CASES.md](../tests/golden/TECH/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`tech-command`](../openspec/specs/tech-command/spec.md) | 9 | 16 | Defines the `tech` command. It queries the current technology (name, file name, version, internal scale, planes, layers and DRC values), loads a new technology, and changes which l… |
| [`tech-compose-and-connect`](../openspec/specs/tech-compose-and-connect/spec.md) | 17 | 30 | Defines the `compose` and `connect` sections of a technology file and the tables they produce: - the per-plane **paint** and **erase** result tables, which define what any type bec… |
| [`tech-contacts-and-stacking`](../openspec/specs/tech-contacts-and-stacking/spec.md) | 8 | 13 | Defines the `contact` section of a technology file (alias `images`). It covers how a type is declared a contact, its residues and the planes it touches, the `stackable` statement, … |
| [`tech-file-format`](../openspec/specs/tech-file-format/spec.md) | 13 | 30 | Defines the lexical and structural format of a Magic-compatible technology (`.tech`) file: how lines are read and split into tokens, comments, line continuation, quoting, the `incl… |
| [`tech-layers-and-planes`](../openspec/specs/tech-layers-and-planes/spec.md) | 11 | 23 | Defines how planes and tile types are declared in the `planes`, `types` and `aliases` sections of a technology file. It covers the built-in planes and types, how numbers and names … |
| [`tech-loading-and-scaling`](../openspec/specs/tech-loading-and-scaling/spec.md) | 13 | 19 | Defines how a technology file is found, loaded, partly reloaded and selected at startup, and what a load does to layout already in memory. It also defines the internal-grid scale m… |
| [`tech-styles-section`](../openspec/specs/tech-styles-section/spec.md) | 4 | 6 | Defines the `styles` section of a technology file. It covers the `styletype` statement, which selects the family of display-style and color-map files, and the mapping of each tile … |

## Cell files (.mag) (CELL)

Open questions: [44](decisions/open-questions/CELL.md) · Golden cases: [CASES.md](../tests/golden/CELL/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`cell-file-format`](../openspec/specs/cell-file-format/spec.md) | 24 | 51 | Defines Magic's native text cell format (`.mag`) for both reading and writing, so that Hefesto can read every `.mag` file Magic reads and write files that are byte-identical to the… |
| [`cell-io-safety`](../openspec/specs/cell-io-safety/spec.md) | 16 | 24 | Defines the safety rules around reading and writing cell files: |
| [`cell-library-resolution`](../openspec/specs/cell-library-resolution/spec.md) | 15 | 26 | Defines how Hefesto maps a cell name, or an instance's recorded path, to a cell file. It covers: |
| [`cell-timestamps`](../openspec/specs/cell-timestamps/spec.md) | 8 | 11 | Defines the timestamp carried by every cell definition and by every instance record in a parent. The hierarchical design-rule checker uses these timestamps to detect children that … |

## Layout data model (MODEL)

Open questions: [31](decisions/open-questions/MODEL.md) · Golden cases: [CASES.md](../tests/golden/MODEL/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`layout-connectivity`](../openspec/specs/layout-connectivity/spec.md) | 5 | 7 | Specifies the electrical connectivity relation of the layout database as observable semantics: which pieces of paint are connected within a plane, across planes through contacts, a… |
| [`layout-geometry-model`](../openspec/specs/layout-geometry-model/spec.md) | 9 | 21 | Defines the geometric foundation of the Hefesto layout database so that it is data-compatible with Magic VLSI: the integer coordinate space and its limits, rectangles and their pre… |
| [`layout-hierarchy`](../openspec/specs/layout-hierarchy/spec.md) | 13 | 20 | Specifies the hierarchical data model of a Magic-compatible layout: cell definitions and their namespace, cell instances (uses) with instance identifiers, transforms and arrays, bo… |
| [`layout-labels-ports`](../openspec/specs/layout-labels-ports/spec.md) | 11 | 15 | Specifies the label and port data model of a Magic-compatible layout database: label attributes (text, attached layer, rectangle, justification, outline font, size, rotation, offse… |
| [`layout-paint-semantics`](../openspec/specs/layout-paint-semantics/spec.md) | 11 | 19 | Specifies, as observable semantics, what geometry results when paint is added to, removed from, or overwritten in a cell: the per-plane result tables (paint, erase, write), the def… |
| [`layout-properties`](../openspec/specs/layout-properties/spec.md) | 6 | 10 | Specifies cell-definition properties: the typed key/value store attached to each cell definition, the value types and their coercions, how geometric property values follow scaling … |
| [`layout-undo`](../openspec/specs/layout-undo/spec.md) | 9 | 20 | Specifies the undo/redo model: the event log, how events are grouped into one undo step per user command, what database and editor changes are recorded (and what is not), how undo … |

## Design rule checking (DRC)

Open questions: [23](decisions/open-questions/DRC.md) · Golden cases: [CASES.md](../tests/golden/DRC/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`drc-cif-rules`](../openspec/specs/drc-cif-rules/spec.md) | 6 | 7 | Defines the `cifstyle`, `cifwidth`, `cifspacing`, `cifarea` and `cifmaxwidth` statements of the `drc` tech section, which check geometry of mask (CIF/GDS output) layers generated f… |
| [`drc-commands`](../openspec/specs/drc-commands/spec.md) | 15 | 19 | Defines the user-facing `drc` command of a Magic-compatible editor: option parsing, every option and its effect, printed output, and the values returned in scripting mode (`drc lis… |
| [`drc-engine`](../openspec/specs/drc-engine/spec.md) | 14 | 14 | Defines how Hefesto keeps DRC results up to date for a cell hierarchy in a Magic-compatible way: the per-cell check and error planes, how edits mark areas for rechecking, how the b… |
| [`drc-rule-language`](../openspec/specs/drc-rule-language/spec.md) | 22 | 29 | Defines the `drc` section of a Magic-compatible technology file: its grammar, the style and variant mechanism, the scale factor, and every rule keyword with its parameters. For eac… |
| [`drc-rule-semantics`](../openspec/specs/drc-rule-semantics/spec.md) | 13 | 20 | Defines the exact geometric check performed by each primitive DRC rule produced by capability `drc-rule-language`, when a *basic check* is run over a single flat cell (or a flatten… |

## Mask generation, GDS, CIF (MASK)

Open questions: [139](decisions/open-questions/MASK.md) · Golden cases: [CASES.md](../tests/golden/MASK/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`cif-format-io`](../openspec/specs/cif-format-io/spec.md) | 30 | 40 | Defines the Caltech Intermediate Form (CIF) as read and written by Hefesto in a Magic-compatible way: the lexical rules and grammar accepted on input, how symbols, calls, transform… |
| [`gds-read`](../openspec/specs/gds-read/spec.md) | 41 | 83 | Defines how Hefesto reads a GDSII stream file (the `gds read` / `calma read` operation) into layout cells. The goal is data compatibility with Magic 8.3.694 (commit `fc5daa9e`): th… |
| [`gds-write`](../openspec/specs/gds-write/spec.md) | 28 | 64 | Defines how Hefesto serializes a layout hierarchy to a GDSII stream file so that the output is byte-identical to Magic VLSI 8.3.694 (commit `fc5daa9e`) for the same technology file… |
| [`mask-hierarchical-generation`](../openspec/specs/mask-hierarchical-generation/spec.md) | 25 | 43 | Define how mask (CIF/GDS output) geometry is generated for a hierarchical layout so that Hefesto produces exactly the same per-cell output geometry as Magic. This covers: |
| [`mask-input-styles`](../openspec/specs/mask-input-styles/spec.md) | 26 | 41 | Defines the `cifinput` section of the technology file and the semantics Hefesto applies when converting mask-layer geometry read from CIF or GDSII files into Magic-compatible tile … |
| [`mask-output-operators`](../openspec/specs/mask-output-operators/spec.md) | 28 | 39 | Defines exactly how the mask layers of an output style (capability `mask-output-styles`) are computed from layout geometry: the geometric model, the layer evaluation pipeline for o… |
| [`mask-output-styles`](../openspec/specs/mask-output-styles/spec.md) | 19 | 33 | Defines the `cifoutput` section of the technology file: how mask output styles, style variants, mask layers (`layer`, `templayer`, `labellayer`), label-to-layer mappings, GDS layer… |

## OASIS (OASIS)

Open questions: [34](decisions/open-questions/OASIS.md) · Golden cases: [CASES.md](../tests/golden/OASIS/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`oasis-command`](../openspec/specs/oasis-command/spec.md) | 7 | 11 | Defines the `oasis` command: Hefesto's command-level API for reading and writing OASIS files. Its options mirror the Magic `gds` (`calma`) command, so that scripts written for GDS … |
| [`oasis-format`](../openspec/specs/oasis-format/spec.md) | 30 | 56 | Defines the OASIS (SEMI P39, version 1.0) byte-level format as Hefesto reads and writes it: the magic bytes, the START and END records, the primitive encodings (unsigned and signed… |
| [`oasis-read`](../openspec/specs/oasis-read/spec.md) | 25 | 41 | Defines how Hefesto reads an OASIS file (the `oasis read` operation, capability `oasis-command`) into layout cells. Magic has no OASIS reader. The governing rule is **GDS equivalen… |
| [`oasis-write`](../openspec/specs/oasis-write/spec.md) | 25 | 34 | Defines how Hefesto serializes a layout hierarchy to an OASIS file (the `oasis write` operation, capability `oasis-command`). Magic has no OASIS writer. The governing rule is **GDS… |

## LEF / DEF (LEFDEF)

Open questions: [102](decisions/open-questions/LEFDEF.md) · Golden cases: [CASES.md](../tests/golden/LEFDEF/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`def-read`](../openspec/specs/def-read/spec.md) | 30 | 51 | Defines how Hefesto reads a DEF (Design Exchange Format) file into a layout cell, compatibly with Magic's `def read` command. It covers the command and its options, the accepted DE… |
| [`def-write`](../openspec/specs/def-write/spec.md) | 25 | 46 | Defines the `def write` command and the DEF 5.7 file that it writes for one cell. The file contains the die area, generated via definitions, placed components, pins, non-default ru… |
| [`lef-read`](../openspec/specs/lef-read/spec.md) | 23 | 44 | Defines how a LEF file is read into the layout database by `lef read`: the shared LEF/DEF tokenizer and error reporting, unit conversion, LEF layer name resolution, the technology … |
| [`lef-tech-section`](../openspec/specs/lef-tech-section/spec.md) | 10 | 19 | Defines the `lef` section of the technology file: the table that maps LEF/DEF layer, cut and via names to magic tile types, the per-entry LEF class, the obstruction types, and the … |
| [`lef-write`](../openspec/specs/lef-write/spec.md) | 40 | 65 | Defines how Hefesto writes LEF (Library Exchange Format) abstract views of layout cells so that the result is byte-compatible with Magic VLSI 8.3.694. It covers the `lef write`, `l… |

## Circuit extraction (EXTRACT)

Open questions: [26](decisions/open-questions/EXTRACT.md) · Golden cases: [CASES.md](../tests/golden/EXTRACT/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`extract-capacitance`](../openspec/specs/extract-capacitance/spec.md) | 13 | 16 | Defines the exact numeric models the extractor uses to compute parasitic capacitance within one flattened area of layout: node capacitance to substrate (area, perimeter and corner … |
| [`extract-commands`](../openspec/specs/extract-commands/spec.md) | 12 | 13 | Defines the `extract` command family: which cells are extracted and when (incremental, all, single cell, parents), extraction options (`do`/`no`), their defaults (including the sta… |
| [`extract-devices`](../openspec/specs/extract-devices/spec.md) | 19 | 21 | Defines how the extractor recognizes devices in the paint of one cell, chooses the device record (model) among the candidates declared in the tech file, assigns gate, terminal and … |
| [`extract-file-format`](../openspec/specs/extract-file-format/spec.md) | 7 | 7 | Defines the `.ext` file written by the extractor for each cell: the complete grammar of every record the extractor writes, the exact number formatting, the order of records, and an… |
| [`extract-hierarchy`](../openspec/specs/extract-hierarchy/spec.md) | 12 | 12 | Defines hierarchical circuit extraction: the order in which cells are extracted, how interactions between a cell's own paint and its subcells (and between subcells, and between ele… |
| [`extract-nodes-and-labels`](../openspec/specs/extract-nodes-and-labels/spec.md) | 10 | 16 | Defines how the flat (single-cell) extractor partitions the paint of one cell into electrical nodes, how each node's reference point and generated name are computed, how labels are… |
| [`extract-resistance-lumped`](../openspec/specs/extract-resistance-lumped/spec.md) | 5 | 6 | Defines the lumped resistance estimate written for every node of a `.ext` file, the per-resistance-class area and perimeter values written with every node and merge record, and the… |
| [`extract-substrate`](../openspec/specs/extract-substrate/spec.md) | 8 | 9 | Defines how the extractor represents the substrate: the global substrate node of each cell, isolated substrate regions (e.g., p-wells inside deep n-wells), substrate shielding type… |
| [`extract-tech-section`](../openspec/specs/extract-tech-section/spec.md) | 22 | 29 | Defines the `extract` section of the technology file: extraction styles and variants, units and scale factors, the connectivity used for nodes, sheet resistance classes, every para… |

## Netlists and extresist (NETLIST)

Open questions: [48](decisions/open-questions/NETLIST.md) · Golden cases: [CASES.md](../tests/golden/NETLIST/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`ext-flattening-hierarchy`](../openspec/specs/ext-flattening-hierarchy/spec.md) | 11 | 14 | This capability defines how the hierarchical circuit model read by capability `ext-flattening-read` is flattened, fully or one level at a time, into flat nodes, devices, couplings … |
| [`ext-flattening-names`](../openspec/specs/ext-flattening-names/spec.md) | 7 | 14 | This capability defines hierarchical node names during netlisting: - their structure and canonical string form - instance and array-element naming - the preference order that picks… |
| [`ext-flattening-read`](../openspec/specs/ext-flattening-read/spec.md) | 11 | 21 | This capability defines how the netlisting front end reads a tree of `.ext` files (and their optional `.res.ext` overlays) into an in-memory hierarchical circuit model of cell defi… |
| [`extresist-command`](../openspec/specs/extresist-command/spec.md) | 6 | 8 | This capability defines the `extresist` command (full R-C extraction): its options and persistent settings, the inputs it reads, which nets it processes and the cutoff arithmetic, … |
| [`extresist-network`](../openspec/specs/extresist-network/spec.md) | 15 | 26 | This capability defines how the `extresist` command builds a resistor network for one net from layout geometry, and how it reduces that network. It covers: - tile fracturing; - bre… |
| [`extresist-output`](../openspec/specs/extresist-output/spec.md) | 5 | 6 | This capability defines the outputs of the `extresist` command (capability `extresist-command`): - the `.res.ext` overlay file: grammar, naming of new nodes, record order and numer… |
| [`netlist-extcheck`](../openspec/specs/netlist-extcheck/spec.md) | 2 | 3 | This capability defines `extcheck`, a standalone checker. It reads and flattens a `.ext` tree using capabilities `ext-flattening-read` and `ext-flattening-hierarchy`, reports globa… |
| [`netlist-sim`](../openspec/specs/netlist-sim/spec.md) | 7 | 10 | This capability defines the `.sim` netlist generator (Magic's `ext2sim` / `exttosim` command) and its companion `.al` (alias) and `.nodes` (label) files, in the MIT, LBL and SU dia… |
| [`netlist-spice-devices`](../openspec/specs/netlist-spice-devices/spec.md) | 10 | 15 | This capability defines the exact SPICE text for each extracted device: the instance name and counters, the terminal order per device class, geometry and parameter formatting under… |
| [`netlist-spice-hierarchy`](../openspec/specs/netlist-spice-hierarchy/spec.md) | 6 | 8 | This capability defines SPICE output with `hierarchy on`: one subcircuit per cell definition, the pre-pass that promotes nets to ports, which cells are omitted, stubbed or absorbed… |
| [`netlist-spice-options`](../openspec/specs/netlist-spice-options/spec.md) | 10 | 18 | This capability defines the SPICE netlist generator (Magic's `ext2spice` / `exttospice` command): its options, defaults and run flags, the output file, and the structure of a flat … |

## LVS (netgen-compatible) (LVS)

Open questions: [29](decisions/open-questions/LVS.md) · Golden cases: [CASES.md](../tests/golden/LVS/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`lvs-command`](../openspec/specs/lvs-command/spec.md) | 8 | 8 | Defines Hefesto's LVS entry points: - the netgen-compatible `lvs` command and its arguments - the lower-level netgen command set exposed to scripts - obtaining the layout netlist f… |
| [`lvs-comparison-algorithm`](../openspec/specs/lvs-comparison-algorithm/spec.md) | 13 | 14 | Defines how two flat views of a cell pair (circuit 1 and circuit 2) are compared for graph isomorphism. It covers: - the bipartite device/net graph - the initial partition - iterat… |
| [`lvs-device-reduction`](../openspec/specs/lvs-device-reduction/spec.md) | 8 | 10 | Defines how devices are reduced before and during LVS comparison: - combining parallel devices into one device with several property records - combining series devices into one dev… |
| [`lvs-hierarchy`](../openspec/specs/lvs-hierarchy/spec.md) | 9 | 12 | Defines hierarchical LVS: - building the bottom-up compare queue of matching cell pairs - the per-pair comparison flow of the `lvs` command - flattening unmatched or mismatched sub… |
| [`lvs-netlist-input`](../openspec/specs/lvs-netlist-input/spec.md) | 14 | 23 | Defines how the built-in layout-versus-schematic (LVS) engine reads the two netlists it compares. Decision P-5 requires the results to be compatible with netgen. This covers: - the… |
| [`lvs-property-matching`](../openspec/specs/lvs-property-matching/spec.md) | 9 | 12 | Defines how the properties of matched devices are compared: - which properties are compared (properties of interest) - value types and promotion between them - tolerance rules for … |
| [`lvs-reports`](../openspec/specs/lvs-reports/spec.md) | 11 | 12 | Defines the outputs of LVS: - which messages go to the comparison log (`comp.out`) and which go only to the console - the two-column side-by-side layout - the exact text of each re… |
| [`lvs-setup-file`](../openspec/specs/lvs-setup-file/spec.md) | 15 | 16 | Defines the LVS setup file and the commands that PDK setup files use to configure a comparison. Examples of such files are `sky130A_setup.tcl`, `gf180mcuD_setup.tcl` and `sg13g2_se… |

## Embedded simulation (ngspice) (SIM)

Open questions: [19](decisions/open-questions/SIM.md) · Golden cases: [CASES.md](../tests/golden/SIM/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`simulation-analyses-and-results`](../openspec/specs/simulation-analyses-and-results/spec.md) | 11 | 16 | Defines which analyses Hefesto runs through the embedded ngspice engine (capability `simulation-engine-embedding`), and the job lifecycle around them: states, progress reporting, c… |
| [`simulation-command`](../openspec/specs/simulation-command/spec.md) | 7 | 13 | Defines the `sim` command: the command-API surface (capability `hefesto-architecture`, Requirement "Single command API") through which the GUI, touch gestures, scripts and the head… |
| [`simulation-corners`](../openspec/specs/simulation-corners/spec.md) | 8 | 10 | Defines corner analysis in Hefesto (decision P-19): - the corner definitions and corner sets held by each PDK's simulation model descriptor in the PDK store - the bundled corner ta… |
| [`simulation-engine-embedding`](../openspec/specs/simulation-engine-embedding/spec.md) | 15 | 27 | Defines how Hefesto embeds the ngspice circuit simulator as a library so that SPICE simulation works on Linux, macOS and iPadOS (decision P-7). It covers how ngspice is built and l… |
| [`simulation-measurements-and-statistics`](../openspec/specs/simulation-measurements-and-statistics/spec.md) | 11 | 17 | Defines how Hefesto turns simulation results into numbers and verdicts, and how it summarises many runs (decision P-19): - named measurements on stored vectors, extending the wavef… |
| [`simulation-monte-carlo`](../openspec/specs/simulation-monte-carlo/spec.md) | 10 | 17 | Defines Monte Carlo analysis in Hefesto (decision P-19): - the Monte Carlo modes (process, mismatch, both) and how each maps onto a PDK's own statistical model sections and switche… |
| [`simulation-netlist-preparation`](../openspec/specs/simulation-netlist-preparation/spec.md) | 14 | 24 | Defines how Hefesto builds the SPICE deck that the embedded ngspice engine runs (capability `simulation-engine-embedding`). The deck is built from: - a circuit under test (DUT): ex… |
| [`simulation-optimization-hooks`](../openspec/specs/simulation-optimization-hooks/spec.md) | 7 | 8 | Defines an optional evaluation interface through which optimisation loops drive simulation (decision P-19). The main client is the AI agent (decision P-10): its sizing and post-lay… |
| [`simulation-sweeps`](../openspec/specs/simulation-sweeps/spec.md) | 9 | 17 | Defines how Hefesto runs one testbench many times with different values (decision P-19): - run sets, the batch object shared by sweeps, corners (capability `simulation-corners`) an… |
| [`simulation-ui-and-crossprobe`](../openspec/specs/simulation-ui-and-crossprobe/spec.md) | 12 | 14 | Defines the waveform viewer and the links between simulation results and layout: - probing layout nets and devices to show their vectors - highlighting the layout net of a waveform… |

## AI layout agent (AGENT)

Open questions: [6](decisions/open-questions/AGENT.md) · Golden cases: [CASES.md](../tests/golden/AGENT/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`agent-architecture`](../openspec/specs/agent-architecture/spec.md) | 5 | 5 | Defines how Hefesto's AI layout agent is structured. The agent is a planner driven by an AI model. It can act on a layout only by calling tools, and every tool maps to the command … |
| [`agent-device-and-cell-generation`](../openspec/specs/agent-device-and-cell-generation/spec.md) | 3 | 4 | Lets the agent build devices and cells from a description or from a SPICE subcircuit. Examples: "nfet W=2 µm L=0.15 µm with 4 fingers", "a 1 pF MIM capacitor", "an inverter cell ma… |
| [`agent-drc-lvs-fix-loop`](../openspec/specs/agent-drc-lvs-fix-loop/spec.md) | 3 | 4 | Lets the agent repair layout by iterating. It reads DRC errors and LVS mismatches, proposes edits, re-runs the checks, and repeats until the checks are clean or a limit is reached.… |
| [`agent-evaluation`](../openspec/specs/agent-evaluation/spec.md) | 3 | 3 | Defines how agent quality is measured and kept from regressing. A benchmark suite of layout tasks is scored by Hefesto's own verification gates. The cases are listed in `tests/gold… |
| [`agent-model-providers`](../openspec/specs/agent-model-providers/spec.md) | 4 | 4 | Defines the pluggable interface through which the layout agent reaches an AI model, and the confidentiality rules for layout data. Layouts and PDK data are often confidential, and … |
| [`agent-natural-language-commands`](../openspec/specs/agent-natural-language-commands/spec.md) | 2 | 3 | Lets users describe layout operations in natural language, for example "make a 2 µm wide metal2 strap from pin A to pin B". The agent translates the request into a reviewable seque… |
| [`agent-safety-and-approval`](../openspec/specs/agent-safety-and-approval/spec.md) | 5 | 5 | Defines how the layout agent operates under human approval. The agent proposes changes, the user reviews them as a visual and structural diff, and only an explicit accept applies t… |
| [`agent-schematic-driven-layout`](../openspec/specs/agent-schematic-driven-layout/spec.md) | 4 | 4 | Lets the agent turn a schematic netlist into a verified layout. It generates the devices (capability `agent-device-and-cell-generation`), places them, routes the nets, and closes t… |

## Schematic capture (xschem-compatible) (SCHEM)

Open questions: [29](decisions/open-questions/SCHEM.md) · Golden cases: [CASES.md](../tests/golden/SCHEM/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`schematic-command`](../openspec/specs/schematic-command/spec.md) | 6 | 8 | Defines the `schematic` command: the command-API surface (capability `hefesto-architecture`, Requirement "Single command API") through which the GUI, touch gestures, key bindings, … |
| [`schematic-editing`](../openspec/specs/schematic-editing/spec.md) | 19 | 21 | Defines the editing model of Hefesto's schematic and symbol editor (decision P-12): the document and object model, snapping, selection, placing and changing wires, instances, label… |
| [`schematic-file-format`](../openspec/specs/schematic-file-format/spec.md) | 22 | 29 | Defines the on-disk format of schematic (`.sch`) and symbol (`.sym`) files used by Hefesto's built-in schematic editor (decision P-12). Hefesto SHALL read and write these files so … |
| [`schematic-netlisting`](../openspec/specs/schematic-netlisting/spec.md) | 24 | 35 | Defines how Hefesto turns a schematic hierarchy into a SPICE netlist that is identical to the one xschem produces (decision P-12). The netlist feeds embedded simulation (capabiliti… |
| [`schematic-simulation-integration`](../openspec/specs/schematic-simulation-integration/spec.md) | 11 | 15 | Defines how schematics drive simulation and LVS in Hefesto (decisions P-12, P-7 and P-5). This covers: - testbench schematics and their code blocks (`.include`, `.lib`, `.param`, `… |
| [`schematic-symbols-and-libraries`](../openspec/specs/schematic-symbols-and-libraries/spec.md) | 19 | 24 | Defines what a schematic symbol means and how symbols are found. Decision P-12 requires the xschem symbol libraries of the sky130, gf180mcu and IHP sg13g2 PDKs to work in Hefesto u… |

## Tapeout readiness (TAPEOUT)

Open questions: [17](decisions/open-questions/TAPEOUT.md) · Golden cases: [CASES.md](../tests/golden/TAPEOUT/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`tapeout-fill-and-density`](../openspec/specs/tapeout-fill-and-density/spec.md) | 13 | 17 | Defines how Hefesto generates metal, poly and diffusion fill and checks layout density before tapeout: - per-PDK fill recipes - density windows and limits - the reports |
| [`tapeout-seal-ring`](../openspec/specs/tapeout-seal-ring/spec.md) | 9 | 12 | Defines how Hefesto generates the die seal ring for each supported PDK and places it around a chip. Where the PDK ships a Magic-based generator, Hefesto reproduces its geometry exa… |
| [`tapeout-shuttle-packages`](../openspec/specs/tapeout-shuttle-packages/spec.md) | 9 | 10 | Defines how Hefesto exports a design as a submission package for an open shuttle program. A package contains: - the files the program requires, under the required names and folder … |
| [`tapeout-signoff-checklist`](../openspec/specs/tapeout-signoff-checklist/spec.md) | 15 | 20 | Defines Hefesto's sign-off checklist. It is an ordered, reproducible set of checks that runs on the exact GDS to be submitted. Its result tells the user whether the design is ready… |

## Digital flow (desktop-only) (DIGITAL)

Open questions: [10](decisions/open-questions/DIGITAL.md) · Golden cases: [CASES.md](../tests/golden/DIGITAL/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`digital-flow-block-import`](../openspec/specs/digital-flow-block-import/spec.md) | 11 | 16 | Defines how Hefesto imports hardened digital blocks (macros) produced by the open digital flow (LibreLane, formerly OpenLane 2, built on OpenROAD) so that they can be viewed, place… |
| [`digital-flow-mixed-signal`](../openspec/specs/digital-flow-mixed-signal/spec.md) | 10 | 13 | Defines how hardened digital blocks (capability `digital-flow-block-import`) are used inside analog and top-level layouts in Hefesto: placing block instances, connecting their supp… |
| [`digital-flow-results`](../openspec/specs/digital-flow-results/spec.md) | 13 | 16 | Defines how Hefesto presents the results of a LibreLane run: the run summary, the metrics, the logs, the sign-off reports (DRC, LVS, antenna) mapped onto layout locations, the fina… |
| [`digital-flow-run`](../openspec/specs/digital-flow-run/spec.md) | 16 | 21 | Defines how the Hefesto desktop app configures and launches the open digital flow (LibreLane, built on Yosys, OpenROAD, Magic, KLayout and Netgen) as an external process. It covers… |

## Plugin API (PLUGIN)

Open questions: [8](decisions/open-questions/PLUGIN.md) · Golden cases: [CASES.md](../tests/golden/PLUGIN/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`plugin-api`](../openspec/specs/plugin-api/spec.md) | 17 | 24 | Defines what Hefesto plugins can do and the binary contract they use (decision P-20). A plugin extends Hefesto through a fixed set of extension points: commands in the single comma… |
| [`plugin-packaging-and-distribution`](../openspec/specs/plugin-packaging-and-distribution/spec.md) | 12 | 15 | Defines how Hefesto plugins are packaged, described, signed, found, installed, updated and removed (decision P-20). It covers: - the `.hfplugin` package and its `plugin.json` manif… |
| [`plugin-sdk`](../openspec/specs/plugin-sdk/spec.md) | 8 | 8 | Defines the Hefesto plugin SDK (decision P-20). The SDK contains: - the C ABI header and the C++20 wrapper - the build integration and the packaging and signing tool - the required… |
| [`plugin-security-and-platforms`](../openspec/specs/plugin-security-and-platforms/spec.md) | 10 | 11 | Defines the security model for Hefesto plugins and their availability on each platform (decision P-20). It covers: - capability-based permissions - how permissions are enforced for… |

## Reliability (EM / IR drop) (RELIABILITY)

Open questions: [12](decisions/open-questions/RELIABILITY.md) · Golden cases: [CASES.md](../tests/golden/RELIABILITY/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`reliability-em-check`](../openspec/specs/reliability-em-check/spec.md) | 10 | 13 | Defines the electromigration (EM) check (decision P-21). For every wire segment, via and contact of the selected nets, Hefesto computes current metrics (average, absolute average, … |
| [`reliability-em-rules`](../openspec/specs/reliability-em-rules/spec.md) | 9 | 15 | Defines how electromigration (EM) current limits are represented as PDK data in the PDK store, how a limit is looked up for one wire segment, via or contact, and which limits Hefes… |
| [`reliability-ir-drop`](../openspec/specs/reliability-ir-drop/spec.md) | 12 | 14 | Defines static and dynamic IR-drop analysis of power and ground nets (decision P-21). The resistive grid is the resistance network that capability `extresist-network` builds, with … |
| [`reliability-visualization`](../openspec/specs/reliability-visualization/spec.md) | 8 | 10 | Defines how Hefesto presents IR-drop and electromigration results (decision P-21). It covers: - 2D heat maps over the layout - 3D overlays through capability `hefesto-3d-visualizat… |

## Editing commands (CMDEDIT)

Open questions: [38](decisions/open-questions/CMDEDIT.md) · Golden cases: [CASES.md](../tests/golden/CMDEDIT/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`cmd-box`](../openspec/specs/cmd-box/spec.md) | 15 | 26 | Defines the **box**: the single rectangular editing cursor that marks the area for paint, erase, fill, selection, cell placement and most other editing commands. This capability co… |
| [`cmd-coordinates-and-units`](../openspec/specs/cmd-coordinates-and-units/spec.md) | 14 | 37 | Defines how editing commands parse distances and coordinates (numbers, unit suffixes, simple expressions), how keyword arguments and direction names are matched, how the `units`, `… |
| [`cmd-hierarchy`](../openspec/specs/cmd-hierarchy/spec.md) | 19 | 25 | Defines the commands that load, create, place, navigate, expand, flatten, save and manage cells and cell instances: `load`, `xload`, `edit`, `down`, `getcell`/`get`, `dump`, `expan… |
| [`cmd-labels-properties`](../openspec/specs/cmd-labels-properties/spec.md) | 10 | 13 | Defines the commands that create and edit labels and ports, query and set cell properties, locate labels and nodes, and draw non-mask annotation elements: `label`, `setlabel`, `por… |
| [`cmd-misc-edit`](../openspec/specs/cmd-misc-edit/spec.md) | 9 | 11 | Defines the remaining layout-window commands that are not owned by another capability: `xor`, `random`, `shell`, the command surface of `straighten`, and the build-specific command… |
| [`cmd-paint-erase`](../openspec/specs/cmd-paint-erase/spec.md) | 13 | 26 | Defines the commands that change mask paint in the edit cell: `paint`, `erase`, `fill`, `corner`, `contact`, `polygon`, `splitpaint`, `spliterase` and `drop`. It also defines the l… |
| [`cmd-selection`](../openspec/specs/cmd-selection/spec.md) | 21 | 30 | Defines the selection model and the commands that create, inspect and operate on it: `select` (every option and qualifier, point-selection cycling, chunk/region/net growth, `short`… |

## UI, display, startup (UI)

Open questions: [40](decisions/open-questions/UI.md) · Golden cases: [CASES.md](../tests/golden/UI/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`app-startup`](../openspec/specs/app-startup/spec.md) | 13 | 19 | Defines how the application starts and ends: - command-line options and file arguments - installation-root and search-path resolution - the order of startup files - default technol… |
| [`display-style-files`](../openspec/specs/display-style-files/spec.md) | 15 | 23 | Defines the file formats that give layers and user-interface elements their look: display-style files (`.dstyle`, all format versions), color-map files (`.cmap`), glyph files (`.gl… |
| [`ui-color-window`](../openspec/specs/ui-color-window/spec.md) | 6 | 7 | Defines the `color` window client. It edits one entry of the session color map interactively, using RGB and HSV bars, and loads or saves color-map files. The file format is in `dis… |
| [`ui-command-dispatch`](../openspec/specs/ui-command-dispatch/spec.md) | 17 | 29 | Defines how user actions become commands and reach the code that runs them. This covers: - the window-client model and per-client command tables - command lookup, prefix rules and … |
| [`ui-feedback-and-highlights`](../openspec/specs/ui-feedback-and-highlights/spec.md) | 6 | 8 | Defines the highlight layer drawn over layout windows, and the feedback-area facility that subsystems and users use to mark areas with explanatory text. This covers: - the order of… |
| [`ui-tools-and-input`](../openspec/specs/ui-tools-and-input/spec.md) | 8 | 13 | Defines pointer tools and the default input bindings: - the tool registry and the `tool` command - button semantics of the `box`, `wiring`, `netlist` and `rsim` tools, and the Tcl … |
| [`ui-windows-and-views`](../openspec/specs/ui-windows-and-views/spec.md) | 24 | 33 | Defines windows and what is drawn in them: - the window model (ids, limits, stacking order, chrome flags) - the window commands `openwindow`, `closewindow`, `specialopen`, `over`, … |
| [`ui-wizard-and-debug`](../openspec/specs/ui-wizard-and-debug/spec.md) | 4 | 4 | Documents Magic's wizard commands. These are the commands whose names begin with `*`. They exist for developers: debugging, statistics and internal dumps. They are listed here for … |

## Scripting, PCells, Tcl tools (SCRIPT)

Open questions: [46](decisions/open-questions/SCRIPT.md) · Golden cases: [CASES.md](../tests/golden/SCRIPT/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`pcell-toolkit`](../openspec/specs/pcell-toolkit/spec.md) | 18 | 31 | Specifies the "gencell" framework that Magic provides in `toolkit.tcl`. Every open PDK (sky130, gf180mcu, IHP sg13g2) builds its device generators ("PCells") on this framework. The… |
| [`readspice`](../openspec/specs/readspice/spec.md) | 5 | 9 | Specifies the `readspice <netlist>` command. It is defined by `readspice.tcl` and loaded at startup when present. The command annotates already-loaded layout cells with the port or… |
| [`scripting-api`](../openspec/specs/scripting-api/spec.md) | 20 | 40 | Defines the scripting surface that Hefesto exposes to scripts. This is the surface that Magic exposes to Tcl when it is built as the `tclmagic` extension. It covers command binding… |
| [`scripting-auxiliary-tools`](../openspec/specs/scripting-auxiliary-tools/spec.md) | 8 | 9 | Documents the auxiliary Tcl scripts and launchers that are installed with Magic: - the socket command server; - the BSI menu helpers; - the reference-library stripper; - the netlis… |
| [`scripting-pdk-integration`](../openspec/specs/scripting-pdk-integration/spec.md) | 13 | 22 | Specifies what open-PDK support files expect from the layout editor and what the editor does on their behalf. This covers sky130A/B, gf180mcuA-D, IHP sg13g2 and other PDKs generate… |
| [`ui-box-stack-and-measure`](../openspec/specs/ui-box-stack-and-measure/spec.md) | 9 | 13 | Specifies the script-level helper commands that Magic defines in `tools.tcl` and that layout-generating scripts use heavily, PDK device generators in particular: - the cursor-box s… |
| [`ui-gui-managers`](../openspec/specs/ui-gui-managers/spec.md) | 18 | 18 | Specifies Magic's Tcl/Tk GUI layer (`wrapper.tcl` and the scripts it sources). It covers: - the wrapper layout window, its menus, title-bar captions and the hook points that PDK sc… |

## Antenna, mask hints, fonts, 3D, plot (MISC)

Open questions: [33](decisions/open-questions/MISC.md) · Golden cases: [CASES.md](../tests/golden/MISC/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`antenna-check`](../openspec/specs/antenna-check/spec.md) | 9 | 19 | Defines the process-antenna (plasma-induced gate damage) check: the extract-section technology keywords that declare antenna rules (`antenna`, `model`, `tiedown`), the exact antenn… |
| [`font-labels`](../openspec/specs/font-labels/spec.md) | 10 | 14 | Defines rendered ("font") labels: labels drawn with a vector outline font at a given size, rotation and offset, as opposed to default labels drawn with a fixed screen font. Covers … |
| [`mask-hints`](../openspec/specs/mask-hints/spec.md) | 8 | 13 | Defines mask hints: per-cell geometric annotations stored as cell properties whose key starts with `MASKHINTS_`. A mask hint is a set of rectangles (a Manhattan region) in the cell… |
| [`plot-output`](../openspec/specs/plot-output/spec.md) | 14 | 18 | Defines hard-copy/plot output of layout: the `plot` command and its subcommands (`postscript`, `pnm`, `svg`, `versatec`, `parameters`, `help`, and the optional `gremlin`/`pixels`),… |
| [`view-3d`](../openspec/specs/view-3d/spec.md) | 8 | 13 | Defines the 3D rendering view of a layout ("wind3d" window client): how it is opened and what it renders, its command set and key bindings, the cifoutput-section `render` statement… |

## Legacy: routers, plow, wiring, netlist window, IRSIM (LEGACY)

Open questions: [60](decisions/open-questions/LEGACY.md) · Golden cases: [CASES.md](../tests/golden/LEGACY/CASES.md)

| Capability | Req | Scen | Purpose |
|---|---|---|---|
| [`irsim-interface`](../openspec/specs/irsim-interface/spec.md) | 9 | 11 | Defines how Hefesto names electrical nodes under selected paint (`getnode`) consistently with Magic's extractor naming, and the interface Magic offers to the switch-level simulator… |
| [`netlist-window`](../openspec/specs/netlist-window/spec.md) | 10 | 11 | Defines Magic's netlist facility used by the routers and by interactive netlist editing: the `.net` file format (exact grammar, reading and writing), the in-memory netlist model (n… |
| [`openaccess`](../openspec/specs/openaccess/spec.md) | 2 | 2 | Records the existence of Magic's experimental OpenAccess (OA) integration, compiled only with both the Tcl wrapper and the `OPENACCESS` build option. It lets cell definitions flagg… |
| [`plow`](../openspec/specs/plow/spec.md) | 27 | 33 | Defines Magic's plowing facility: the `plow` and `straighten` commands, the `plowing` technology section, the plowing rule tables derived from the DRC section, and the complete edg… |
| [`router-channel`](../openspec/specs/router-channel/spec.md) | 37 | 50 | Defines the netlist-driven channel router (`route`, `channels`): the `router` technology section, the routing grid, channel decomposition of empty space, obstacle and hazard maps, … |
| [`router-gate-array`](../openspec/specs/router-gate-array/spec.md) | 11 | 13 | Defines the gate-array router driven by the `garoute` command: user-defined routing channels (normal and river-routing), automatic channel-definition generation, channel initializa… |
| [`router-maze`](../openspec/specs/router-maze/spec.md) | 20 | 34 | Defines Magic's maze router ("mzrouter"): the `mzrouter` technology section (styles, route layers, route contacts, spacings, search parameters, widths), the derivation of widths an… |
| [`router-maze-iroute`](../openspec/specs/router-maze-iroute/spec.md) | 10 | 12 | Defines the `iroute` command: point-to-point routing with the maze router (capability `router-maze`), start/destination specification, endpoint layer selection, result codes, paint… |
| [`router-net2ir`](../openspec/specs/router-net2ir/spec.md) | 3 | 4 | Defines `net2ir`, a standalone converter shipped with Magic that turns a saved feedback file (label locations produced by the netlist `find` command followed by `feedback save`) pl… |
| [`scheme-interpreter`](../openspec/specs/scheme-interpreter/spec.md) | 3 | 3 | Documents Magic's optional built-in mini-Scheme interpreter (selected at build time with `--with-interpreter=scheme`, mutually exclusive with the Tcl interpreter). It is legacy: cu… |
| [`wiring-tool`](../openspec/specs/wiring-tool/spec.md) | 8 | 9 | Defines Magic's wiring interface: the `wiring` technology section (contact definitions used for layer switches), the `wire` command and all its subcommands (type selection, legs, c… |
