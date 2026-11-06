# Digital Flow Block Import

## Purpose
Defines how Hefesto imports hardened digital blocks (macros) produced by the open digital flow (LibreLane, formerly OpenLane 2, built on OpenROAD) so that they can be viewed, placed and verified inside Magic-compatible layouts. A hardened block is a set of views: GDSII and LEF are essential, and DEF, netlists, SPICE, timing libraries, parasitics, delay files, constraints and metrics are optional. Hefesto keeps the block as a read-only vendor-style cell with two representations (abstract and full), maps its pins to Magic ports, and stores the views it does not interpret as attachments that travel with the block.

LibreLane reference: `librelane/librelane` @ `8494d4fea0ddd9bee12ea5e42a38b59438c2b128` (version 3.0.17). LibreLane references cite paths in that repository. Hefesto does not depend on LibreLane being installed to import a block.

This capability owns block import, the block manifest and block updates. Placing blocks in a top-level layout, power hookup, LVS with black boxes and exporting Hefesto layouts as macros belong to capability `digital-flow-mixed-signal`. Running the flow belongs to `digital-flow-run`; browsing run results belongs to `digital-flow-results`.

## Requirements

### Requirement: Import sources
Hefesto SHALL import a hardened block from any one of these sources:
- **Run output.** The `final/` directory of a LibreLane run (capability `digital-flow-run`). Each view is found in the subfolder named after its view identifier, and multi-corner views in `final/<view>/<corner>/`.
- **Macro declaration.** One entry of the `MACROS` dictionary of a LibreLane configuration file (`.json`, `.yaml` or `.yml`). The entry key is the block name. Its view fields are lists of paths (`gds`, `lef`, `vh`, `nl`, `pnl`, `spice`), corner-keyed dictionaries of path lists (`lib`, `spef`, `sdf`), or a single path (`json_h`). Paths with the `dir::` prefix SHALL resolve against the directory that holds the configuration file. Instance placements in the entry (`instances`) SHALL be ignored by import and offered to capability `digital-flow-mixed-signal`.
- **Loose files.** A set of files the user selects. Each file's view is inferred from its extension (table below) and the user SHALL be able to correct the inference before import.

| View id | Extension(s) | Meaning | Required |
|---|---|---|---|
| `gds` | `.gds`, `.gds.gz`, `.gdsii` | GDSII stream of the block | yes (for the full representation) |
| `lef` | `.lef`, `.lef.gz` | LEF abstract of the block | yes |
| `def` | `.def` | Placed-and-routed DEF | no |
| `odb` | `.odb` | OpenROAD database | no (attachment only) |
| `mag` | `.mag` | Magic layout written by the flow | no |
| `nl` | `.nl.v`, `.v` | Gate-level Verilog netlist | no |
| `pnl` | `.pnl.v` | Powered gate-level Verilog netlist | no |
| `vh` | `.vh`, `.vh.gz` | Verilog header (black-box module) | no |
| `spice` | `.spice`, `.sp` | Extracted SPICE netlist | no |
| `cdl` | `.cdl` | CDL netlist | no |
| `lib` | `.lib`, `.lib.gz` | Liberty timing library, per corner | no |
| `spef` | `.spef`, `.spef.gz` | Parasitics, per corner | no |
| `sdf` | `.sdf` | Standard delay format, per corner | no |
| `sdc` | `.sdc` | Design constraints | no |
| `json_h` | `.json` (Yosys JSON header) | Powered netlist in JSON | no |
| `metrics` | `metrics.json` | Flow metrics of the run that produced the block | no |

When a run's `final/` holds several GDS variants (`gds`, `mag_gds`, `klayout_gds`), the `gds` folder SHALL be the primary GDS view and the others SHALL be stored as attachments. Import SHALL refuse a source that has no LEF view, matching LibreLane's rule that a macro needs at least one GDS and one LEF file; a source with LEF but no GDS SHALL import as abstract-only after a warning (see Q-DIGITAL-5). Gzip-compressed views SHALL be accepted for `gds`, `lef`, `vh`, `lib` and `spef`.

Compatibility: native

(LibreLane ref: librelane/config/variable.py:107-189, librelane/state/design_format.py:157-260, librelane/state/state.py:152-218, docs/source/usage/using_macros.md)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Import from a run's final directory
- **GIVEN** a LibreLane run of design `spm` whose `final/` holds `gds/spm.gds`, `lef/spm.lef`, `def/spm.def`, `pnl/spm.pnl.v`, `spef/nom_*/spm.nom.spef` and `metrics.json`
- **WHEN** the user imports that directory as a block
- **THEN** a block named `spm` SHALL be created with views `gds`, `lef`, `def`, `pnl`, `spef` (corner key `nom_*` preserved) and `metrics`

#### Scenario: Macro declaration without LEF
- **GIVEN** a `MACROS` entry `sram` with `"gds": ["dir::gds/sram.gds"]` and an empty `lef` list
- **WHEN** it is imported
- **THEN** import SHALL fail with a diagnostic stating that a LEF view is required, and no block SHALL be created

### Requirement: Block storage and manifest
An imported block SHALL be stored in the project as a folder `blocks/<name>/` containing a manifest file `block.json` and a copy of every view. Each view SHALL be stored under `<view id>/`, and corner-keyed views under `<view id>/<corner key>/`, where any `*` in a corner key is stored as `_` in the folder name and kept unchanged in the manifest. Copies SHALL be byte-identical to their sources, including compression.

The manifest SHALL be UTF-8 JSON with this structure:

```ebnf
manifest   = "{" , '"hefesto_block": 1' , "," , '"name": ' , string ,
             "," , '"representation": ' , ( '"abstract"' | '"full"' ) ,
             "," , '"pdk": ' , pdk , "," , '"source": ' , source ,
             "," , '"views": ' , views , "}" ;
pdk        = '{"name": ' , string , ', "version": ' , string , [ ', "scl": ' , string ] , "}" ;
source     = '{"kind": ' , ( '"run"' | '"macro"' | '"files"' ) , ', "origin": ' , string ,
             [ ', "librelane_version": ' , string ] , [ ', "run_tag": ' , string ] , "}" ;
views      = "{" , [ view_entry , { "," , view_entry } ] , "}" ;
view_entry = string , ": " , ( file_list | corner_map ) ;
corner_map = "{" , string , ": " , file_list , { "," , string , ": " , file_list } , "}" ;
file_list  = "[" , file , { "," , file } , "]" ;
file       = '{"path": ' , string , ', "sha256": ' , hex64 , ', "bytes": ' , integer , "}" ;
```

Writing rules:
- keys SHALL be written in the order of the grammar; view entries and corner keys SHALL be sorted by byte value; files keep their source order;
- indentation SHALL be two spaces per level, line endings LF, and the file SHALL end with one LF;
- every `path` SHALL be relative to the block folder, use `/` separators, and SHALL NOT be absolute or contain a `..` segment;
- `origin` records the source path as given at import time and is informative only;
- `hefesto_block` is the manifest format version. A reader SHALL refuse a manifest with a higher version than it supports, with a diagnostic naming both versions.

Because blocks are stored inside the project, a project with blocks SHALL open on any platform without access to the original LibreLane run.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Corner key with wildcard
- **GIVEN** a LIB view with corner key `*_tt_025C_1v80` and file `spm.lib`
- **WHEN** the block is imported
- **THEN** the file SHALL be stored at `blocks/spm/lib/__tt_025C_1v80/spm.lib`, and the manifest SHALL list it under key `"*_tt_025C_1v80"`

#### Scenario: Manifest path escaping the block folder
- **WHEN** a project is opened whose `block.json` lists a view with path `../../etc/passwd`
- **THEN** the block SHALL be rejected with a diagnostic naming the offending path, and no file outside the block folder SHALL be read

### Requirement: Name and geometry consistency checks
Before creating the block, import SHALL check that the views describe the same design:
1. The LEF SHALL contain a `MACRO` whose name equals the block name. When the LEF holds several macros and none matches, the user SHALL choose one.
2. The GDS SHALL contain a structure named like the block that is not referenced by any other structure in the file (a top structure).
3. When present, the DEF `DESIGN` name, the module name in `vh`, `nl` and `pnl`, and the top `.subckt` name in `spice`/`cdl` SHALL equal the block name.
4. The bounding box of the GDS top structure SHALL lie within the LEF macro box (`ORIGIN` and `SIZE`), allowing one manufacturing grid of tolerance on each side. The manufacturing grid comes from the PDK technology (capability `tech-loading-and-scaling`).
5. Every LEF `PIN` SHALL be found in each netlist view's port list, after the bus-bit mapping of "Pin mapping".

A failure of check 1 or 2 SHALL stop the import. Failures of checks 3 to 5 SHALL be reported as warnings that list every mismatch, and the user MAY proceed. The result of the checks SHALL be stored with the block and shown in the block inspector.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: GDS larger than the LEF box
- **GIVEN** LEF macro `spm` with `ORIGIN 0 0` and `SIZE 100 BY 80`, a manufacturing grid of 0.005 µm, and a GDS top structure `spm` whose bounding box is (0, 0)–(100.2, 80) µm
- **WHEN** the block is imported
- **THEN** a warning SHALL report that the GDS extends 0.2 µm beyond the LEF box on the right side, and the user MAY continue the import

#### Scenario: Netlist module name differs
- **GIVEN** a block `spm` whose `pnl` view declares `module spm_top`
- **WHEN** it is imported
- **THEN** a warning SHALL name `pnl` and `spm_top`, and the import SHALL be allowed to continue

### Requirement: Abstract and full representations
Each block SHALL have two representations of one cell named after the block, and exactly one SHALL be active at a time:
- **Abstract.** Built by reading the LEF view in creation mode (capability `lef-read`): the macro box, ports from `PIN` geometry, obstruction geometry from `OBS`, and the `LEFview` property. Downstream capabilities treat it as an abstract view.
- **Full.** Built by reading the GDS view in read-only mode (capability `gds-read`, Requirement "Read-only vendor views"), then annotating the top cell with the LEF view in annotation mode (capability `lef-read`, Requirement "Annotation of existing cells") so that ports, port order and LEF properties match the abstract. Subcells of the GDS, such as standard cells, SHALL also be read-only.

Switching the active representation SHALL NOT change any instance's placement, transform, instance name or connections, and SHALL be recorded as one undoable operation. The default SHALL be `full` when a GDS view exists and `abstract` otherwise. The representation is chosen per block, not per instance, because both representations share one cell definition.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Switching to abstract keeps placement
- **GIVEN** block `spm` in full representation, placed in cell `top` as instance `u_spm` with transform `(0 1 1000 / -1 0 2000)`
- **WHEN** the user switches `spm` to the abstract representation
- **THEN** `u_spm` SHALL keep the same transform and instance name, `spm` SHALL show only LEF pin and obstruction geometry, and one undo SHALL restore the full representation

### Requirement: Pin mapping
Block ports SHALL be derived from the LEF `PIN` statements:
- **Name.** The LEF pin name. Bus bits written `name[i]` in LEF SHALL match `name[i]` or `name<i>` in GDS labels and netlists, following the bracket substitution of capability `lef-read` (see Q-DIGITAL-6). Names escaped in DEF or Verilog (a leading `\` and a trailing space) SHALL be compared without the escape.
- **Order.** Port indices SHALL follow the LEF `PIN` order, starting at 1 for the first pin, as for creation mode in capability `lef-read`.
- **Direction and use.** `DIRECTION` (INPUT, OUTPUT, INOUT, FEEDTHRU, and OUTPUT TRISTATE) and `USE` (SIGNAL, POWER, GROUND, CLOCK, ANALOG) SHALL be kept as port attributes (capability `layout-labels-ports`).
- **Power pins.** Pins with `USE POWER` or `USE GROUND` SHALL be listed separately in the block inspector as the block's supply pins.

A LEF pin that has no matching label in the GDS SHALL still get port labels in the full representation, created from its LEF port geometry, and SHALL be reported as a warning. A GDS label that names no LEF pin SHALL remain a plain label and SHALL NOT become a port.

Compatibility: native

(LibreLane ref: librelane/steps/openroad.py:1234-1237)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Bus pin with angle-bracket GDS label
- **GIVEN** LEF pin `data[3]` with `DIRECTION INPUT` and a GDS label `data<3>` inside its port rectangle
- **WHEN** the block is imported in full representation
- **THEN** the label SHALL become a port named `data<3>` with direction input and the port index of `data[3]` in LEF pin order

#### Scenario: Supply pins
- **GIVEN** LEF pins `VPWR` (`USE POWER`) and `VGND` (`USE GROUND`)
- **WHEN** the block is imported
- **THEN** the inspector SHALL list `VPWR` and `VGND` as supply pins of the block

### Requirement: Read-only block cells
The cells of an imported block (the top cell and all its subcells) SHALL be read-only:
- editing commands that would change their paint, labels, subcells or properties SHALL fail with a diagnostic naming the block, and the cells SHALL be unchanged;
- they SHALL NOT be queued for DRC when loaded, as specified for read-only cells by capability `gds-read`;
- writing GDS that contains a full-representation block SHALL copy the block's original GDS structure bytes (capability `gds-write`);
- saving the project SHALL NOT write `.mag` files for block cells; the block is reloaded from its stored views.

The user MAY detach a block, which creates an editable copy as ordinary cells under a new name and leaves the block unchanged.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Paint into a block cell
- **GIVEN** block `spm` loaded in full representation
- **WHEN** the user edits `spm` and paints `metal1`
- **THEN** the command SHALL fail with a diagnostic stating that `spm` is a read-only imported block, and `spm` SHALL be unchanged

#### Scenario: Detach a block
- **WHEN** the user detaches block `spm` as `spm_edit`
- **THEN** an editable cell `spm_edit` with the same geometry and ports SHALL be created, and block `spm` SHALL remain read-only and unchanged

### Requirement: Attached views
Views that Hefesto does not interpret for layout (`lib`, `spef`, `sdf`, `sdc`, `odb`, `json_h`, `nl`, `vh`, `metrics` and secondary GDS variants) SHALL be kept as attachments of the block:
- they SHALL be preserved byte for byte, and their SHA-256 SHALL be checked when the project is opened; a mismatch SHALL be reported as a warning naming the file;
- the block inspector SHALL list each attachment with its view id, corner key, size and hash, and SHALL let the user reveal the file;
- attachments SHALL be included when the block is exported again (capability `digital-flow-mixed-signal`, Requirement "Export for top-level digital flows");
- `pnl`, `spice` and `cdl` SHALL additionally be available to LVS (capability `digital-flow-mixed-signal`);
- the `metrics` attachment SHALL be shown in the inspector using the metric presentation of capability `digital-flow-results`.

Hefesto SHALL NOT perform static timing analysis with these views. See Q-DIGITAL-10.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Modified attachment
- **GIVEN** a project whose block `spm` has a `lib` attachment with a recorded SHA-256
- **WHEN** the file is edited outside Hefesto and the project is reopened
- **THEN** a warning SHALL name the file and state that its hash no longer matches the manifest, and the block SHALL still load

### Requirement: DEF view inspection
When a block has a `def` view, the user SHALL be able to open it as a separate, read-only inspection cell named `<name>__def`. The cell SHALL be read with capability `def-read` using the PDK's technology LEF and standard-cell LEFs from the PDK store, with net annotation so that routed nets carry their names. The inspection cell SHALL NOT replace either representation and SHALL NOT be used for GDS output.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Open the DEF of a block
- **GIVEN** block `spm` with a `def` view and sky130A with `sky130_fd_sc_hd` installed in the PDK store
- **WHEN** the user opens the DEF view
- **THEN** a read-only cell `spm__def` SHALL be created whose routes carry net-name labels, and block `spm` SHALL be unchanged

### Requirement: Block update
When a block's source is still reachable, Hefesto SHALL offer to update the block from it. An update SHALL:
1. compare each source file's SHA-256 with the manifest and list added, removed and changed views;
2. compare the new LEF pin list with the current one and list pins that were added, removed, renamed by position, or moved (changed port geometry);
3. after confirmation, replace the views, rebuild both representations, and keep every instance's transform and instance name;
4. mark every instance of the block whose connections may have changed (any pin added, removed or moved) with a warning that remains until the user acknowledges it.

The update SHALL be one undoable operation. A failed consistency check that stops import (Requirement "Name and geometry consistency checks") SHALL also stop the update and leave the block unchanged.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Pin removed by an update
- **GIVEN** block `spm` with pins `a`, `b`, `y`, placed twice in `top`
- **WHEN** it is updated from a new run whose LEF has pins `a` and `y` only
- **THEN** the confirmation SHALL list `b` as removed, both instances SHALL keep their transforms, and both SHALL carry a connection-change warning after the update

### Requirement: Untrusted block views
Every view SHALL be read through the untrusted-input rules of capability `hefesto-security`. Importing a block SHALL NOT execute any script, configuration hook or command found in its views or in the LibreLane configuration it came from. A `MACROS` entry is read as data only; LibreLane preprocessor expressions other than `dir::` (for example `ref::`, `refg::`, `expr::` and `pdk_dir::`) SHALL be reported as unsupported in import and the affected view SHALL be skipped with a warning.

Compatibility: native

(LibreLane ref: librelane/config/preprocessor.py:223-300)

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Unsupported preprocessor expression
- **GIVEN** a `MACROS` entry whose `lef` list is `["refg::$DESIGN_DIR/lef/spm.lef"]`
- **WHEN** it is imported
- **THEN** a diagnostic SHALL state that `refg::` is not supported in block import, and the import SHALL fail because no LEF view remains

### Requirement: Platform availability
Importing and updating blocks SHALL be available in the desktop app on Linux and macOS. On iPadOS, import and update SHALL be unavailable, and their controls SHALL follow capability `hefesto-ipad-feature-scope`, Requirement "Graceful handling of desktop-only features". Blocks already stored in a project SHALL load, display, switch representation and be placed on every platform, because they need only file reading. See Q-DIGITAL-1.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-17)

#### Scenario: Block in a project opened on iPad
- **GIVEN** a project created on macOS with block `spm` stored under `blocks/spm/`
- **WHEN** it is opened on iPadOS
- **THEN** `spm` SHALL load in its stored representation, and the import and update commands SHALL be reported as available only in the desktop app
