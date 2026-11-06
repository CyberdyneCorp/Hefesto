# Glossary

These are the canonical terms used in the Hefesto specs (`openspec/specs/`). They come from the Terminology sections of the spec audit (`docs/decisions/audit/G1.md` … `G6.md`).

Rules:
- Use the canonical term in new and edited spec text.
- Do not rename command names, option names, file-format keywords, exact message strings, or terms quoted from Magic, netgen, xschem or ngspice. For example, the `.mag` keyword `use`, the option `extresist`, and the message `non-manhattan direction vector` stay as they are.
- A requirement *title* is renamed only together with every quoted reference to it.
- The **owner** is the capability that defines the concept. Other specs refer to it.

Changes applied mechanically during consolidation:
- `use identifier` → `instance identifier` (13 occurrences);
- `non-manhattan` → `non-Manhattan` in prose (requirement titles and quoted messages are unchanged);
- the bare "compatibility mode" in `cmd-labels-properties`, `mask-hints`, `cell-file-format`, `ext-flattening-hierarchy` and `netlist-spice-hierarchy` → "property compat mode" or "substrate compatibility mode";
- `extresist-command` used as a command or option name → `extresist`;
- "error color" → the `danger` color token.

The remaining unifications are tracked as decisions D-45 and D-46 in `docs/decisions/audit/DECISIONS.md`.

## Layout model and hierarchy

| Canonical term | Definition | Rejected variants | Owner |
|---|---|---|---|
| cell | A cell definition: named paint, labels, properties and instances. | cell def, definition (fine when the def/instance distinction matters) | `layout-hierarchy` |
| instance | A placement of a cell inside another cell, with a transform and optional array. | use, cell use (except the `.mag` keyword `use` and Magic quotations) | `layout-hierarchy` |
| instance identifier | The name of an instance that is unique within its parent, for example `inv_0`. Array elements append `[y,x]` or `[x]`. | use identifier, use id, instance name | `layout-hierarchy` ("Instance identifier assignment") |
| edit cell | The cell that receives edits. | current cell, target cell | `cmd-hierarchy` |
| read-only cell | A cell that cannot be edited in this session: locked by another process, without write permission, or a vendor view. | not editable, no-edit, locked cell (except for the lock itself) | `cell-io-safety` |
| read-only (vendor) cell | A cell read from GDS/OASIS as a vendor view, kept as a reference to the stream file. | vendor view, vendor cell | `gds-read` |
| abstract view | A cell that carries only LEF pin and obstruction geometry, marked by the cell property `LEFview`. | `LEFview` cell, abstract cell | `lef-read` |
| non-Manhattan geometry | Geometry with 45° or other diagonal edges, stored as split tiles. | non-manhattan, nonmanhattan (except option names) | `layout-geometry-model` ("Non-Manhattan (split tile) geometry") |
| plane infinity / plane minus infinity | The coordinate bounds of a tile plane, used as unscaled markers. | `INFINITY`, `MINFINITY` (Magic macro names) | `layout-geometry-model` |
| undo step | All events recorded while one user command runs. It is undone and redone as a unit. | undo unit, undo event (for a whole command), undo group | `layout-undo` |

## Units and scaling

| Canonical term | Definition | Rejected variants | Owner |
|---|---|---|---|
| internal unit | The integer coordinate unit of the database. | database unit (DEF uses that for its own unit), magic internal unit (except in the exact `scalegrid` message) | `tech-loading-and-scaling` |
| internal units per lambda (I/L) | The scale: I internal units equal L lambda. Write it as `I/L`, with a note when a spec uses the `.mag` ratio `(L0, L1)` or `Λ = L1/L0`. | three different notations today (D-46) | `tech-loading-and-scaling` |
| mask unit | The unit of generated mask geometry. One internal unit is `scaleFactor` mask units. | output unit, CIF unit (output side), mu (fine as an abbreviation once defined) | `mask-output-styles` |
| read unit | The unit of input mask geometry, `1/M` centimicrons. | input unit, CIF unit (input side), RU | `mask-input-styles` |
| `M` | The input multiplier of a cifinput style. | `m` | `mask-input-styles` |
| `S_in` / `S_out` | The input and output scale factors of a mask style. | bare `S`, `σ`, `scaleFactor` used for the output factor in prose | `mask-input-styles` / `mask-output-styles`; `ext-flattening-read` "Scale record" defines the netlister `S_out` |
| `um_per_unit` / `dbu_per_unit` | Micrometers per internal unit, and DEF database units per internal unit. | `oscale` and `s`, each used for both quantities in sibling specs | `lef-write` / `def-write` |
| halo radius | The interaction distance used in hierarchical mask generation. | interaction radius | `mask-hierarchical-generation` ("Halo radius") |
| DRC halo | The interaction distance of hierarchical DRC (`drc halo`). A separate concept from the halo radius. | — | `drc-engine`, `drc-commands` |
| justification | The anchor position of a label's text. | label position | `layout-labels-ports` |

## Compatibility settings

| Canonical term | Definition | Rejected variants | Owner |
|---|---|---|---|
| `magic-compat` setting | The Hefesto setting (default ON) that replicates Magic's output-changing quirks (decision X-2, tier 2). | compat mode, compat flag, compatibility mode, compatibility switch | `hefesto-magic-compat-mode` |
| `netgen-compat` / `xschem-compat` setting | Proposed counterparts for netgen and xschem quirks. Not yet decided (D-2, D-21). | "the compatibility switch" | `hefesto-magic-compat-mode` (proposed) |
| property compat mode | Magic's global property flag (`property compat`, default true): every property is written as `string`. Unrelated to `magic-compat`. | compatibility mode, compat mode, property compatibility mode | `layout-properties` ("Property compat mode") |
| substrate compatibility mode | Netlister behavior when a `.ext` file has no `substrate` record. | compatibility mode | `ext-flattening-hierarchy` |
| ngspice compatibility mode | ngspice's `ngbehavior` setting (`hs`, `hsa`, `ps`, …). | compatibility mode (unqualified) | `simulation-netlist-preparation` ("PDK initialisation commands") |
| script strict mode | The opt-in mode in which failed commands raise Tcl errors. Its name and scope are not yet decided (D-4). | compat flag, compatibility mode (for the default ok-status convention) | `hefesto-diagnostics` |
| `Resolution:` | The line in an open-questions entry that records the answer: `fix`, `compat`, `n/a` (and `platform`, if D-3 allows it). | Status (used for progress notes only) | decision X-2 |

## Files, paths and platforms

| Canonical term | Definition | Rejected variants | Owner |
|---|---|---|---|
| cell search path | The user's ordered list of directories searched for cells (`path` command). | Path, search path (unqualified), library path | `cell-library-resolution` |
| cell library path | The second list searched for cells (`path cell`). | library path (unqualified) | `cell-library-resolution` |
| system search path | The list searched for system files: technology, styles, startup files (`path sys`). | system library path, SysLibPath | `cell-library-resolution`, `tech-loading-and-scaling` ("File search rules") |
| path variables | The table of `$VAR` values used to expand and substitute paths. It is seeded from the environment on desktop and from settings on iPadOS. | environment variables (on iPadOS) | `hefesto-files-and-pdks` (Q-CELL-17) |
| crash backup | Magic's backup file format and the `crash save|recover` mechanism. | autosave (for this file) | `cell-io-safety` |
| autosave | The platform service that triggers crash backups and stores them in the app's location. | crash backup (for the service) | `hefesto-files-and-pdks` |
| PDK store | The abstract store of installed PDKs. | PDK folder | `hefesto-files-and-pdks` |
| PDK root | The PDK store's on-disk layout, which follows ciel. | — | `hefesto-pdk-manager` |
| desktop | Linux and macOS (P-11). "Desktop app" means the Raven app. | — | `hefesto-platform-targets` |
| `hefesto-cli` | The headless executable. | `hefesto --netlist`, headless mode | `hefesto-architecture` ("Headless command-line tool") |
| Tcl build | Magic's build with the Tcl wrapper. Every Hefesto build embeds sandboxed Tcl (X-3), so "scripting build" and "Tcl build" describe Magic only. | scripting build (for Hefesto) | `scripting-api`; see D-7 |

## Extraction and netlisting

| Canonical term | Definition | Rejected variants | Owner |
|---|---|---|---|
| node | One electrical region in a `.ext` file and in flattening. | net (except in `extresist` and DEF) | `extract-nodes-and-labels` |
| net | A node as seen by `extresist` (one `.ext` node) or a DEF net. | — | `extresist-network`, `def-write` |
| `extresist` | The command, its options and the `extract do extresist` option. | `extresist-command` (that is only the capability name) | `extresist-command` |
| full R-C extraction | The process of extracting a resistor network with `extresist`. | R-C extraction, resistance extraction (unqualified) | `extresist-command` |
| resistance-extraction overlay | The `.res.ext` file that is merged into the netlist. | `.res.ext` overlay, `extresist-command` overlay | `extresist-output` |
| extracted lumped resistance | The extractor's single-precision per-node estimate. | lumped resistance (unqualified) | `extract-resistance-lumped` |
| netlister lumped resistance | The flattener's double-precision estimate. | lumped resistance (unqualified) | `ext-flattening-hierarchy` |
| extresist lumped resistance | The `.res.lump` value. | lumped resistance (unqualified) | `extresist-output` |
| `cscale`, `rscale` | The `.ext` header capacitance and resistance scales. | capScale, resistScale, resistance scale | `extract-file-format` |
| `rthresh` | The resistance threshold option. Always state the unit next to it: mΩ (float) for `extresist`, Ω (integer) for ext2spice and ext2sim. | — | `extresist-command`, `netlist-spice-options`, `netlist-sim` |
| LEF layer classes | `ROUTE`, `VIA` (spelled "VIA (cut)" once where the LEF `CUT` type is meant), `MASTERSLICE`, `OVERLAP`, `BOUND`, `IGNORE`, as named in the `lef` tech section. | route, cut/via, cut/contact | `lef-tech-section` |

## Simulation, schematics and generators

| Canonical term | Definition | Rejected variants | Owner |
|---|---|---|---|
| run set | A set of simulation jobs over one or more dimensions. Use `runset` only as the command option and in `runset.json`. | — | `simulation-sweeps` |
| point job | One job of a run set. | — | `simulation-sweeps` |
| sample | One Monte Carlo point. | — | `simulation-monte-carlo` |
| corner set | A named set of corners. `corner sweep` creates an unnamed user corner set. | corner sweep (as a noun) | `simulation-corners` |
| LVS netlist mode | Schematic netlisting for LVS (`-lvs`). | LVS mode (acceptable shorthand) | `schematic-netlisting` |
| device generator (PCell) | Code that generates a parameterized cell. | gencell, PDK device generator | `pcell-toolkit` |
| generated cell | The output of a device generator. | PCell (for the output) | `pcell-toolkit` |

## Product and UI

| Canonical term | Definition | Rejected variants | Owner |
|---|---|---|---|
| Hefesto (as the subject of product obligations) | Use "Hefesto SHALL …" for product requirements and "Magic …" for informative descriptions. | the editor, the system, the layout engine | `docs/spec-conventions.md` |
| `ready` | The checklist state that means sign-off ready. | sign-off ready (fine in prose that quotes P-18) | `tapeout-signoff-checklist` |
| on-prem (OpenAI-compatible) | The self-hosted provider kind. | self-hosted gateway | `agent-model-providers` |
| `danger` color | The design-system token for errors. | error color | `hefesto-design-system` |
| capability reference | Write capability `c`, requirement "X". | (see Requirement: X), (see "X") | `docs/spec-conventions.md` |
