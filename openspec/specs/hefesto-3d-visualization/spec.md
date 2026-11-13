# Hefesto 3D Visualization

## Purpose
Defines Hefesto's 3D view of a layout's process stack. The baseline is parity with Magic's 3D window (capability `view-3d`). Beyond that, Hefesto adds interactive inspection, glTF/STL export, and overlays of extracted parasitics and fields. All 3D drawing goes through the rendering interface of capability `hefesto-rendering`. On the desktop, the 3D view is a Raven viewport.

## Requirements

### Requirement: Magic 3D view parity
The 3D view SHALL provide all behavior of capability `view-3d`, including:
- layer heights and thicknesses from the tech file (cifoutput `render` statements and extract `height` values)
- the CIF-layer and magic-layer views
- the `view-3d` command set, its defaults and its view parameters

Compatibility: behavioral

(Hefesto decision: docs/decisions/product.md#P-4)

#### Scenario: Same stack as Magic
- **GIVEN** a tech file with `render` statements for metal1 (height 1.0, thickness 0.5)
- **WHEN** the 3D view is opened on a layout with metal1
- **THEN** metal1 SHALL be drawn as a solid from z = 1.0 to z = 1.5 in tech units, as in Magic

### Requirement: Mesh generation from layout
The 3D model SHALL be built from the generated mask geometry (capability `mask-output-operators`) or from magic layers, as selected by the user. For each layer, the polygons SHALL be extruded between that layer's height and height + thickness. Contact and via cuts SHALL be drawn with their technology height and thickness, as in Magic; a view option, off by default, SHALL draw them as vertical solids spanning the layers they connect, from the top of the lowest connected layer to the bottom of the highest, the layers being found from the contact's residues (capability `tech-contacts-and-stacking`). Generating the model SHALL be incremental and run in the background. It SHALL cover either the whole cell or the box region.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-4)

#### Scenario: Region model
- **WHEN** a user opens the 3D view with a box covering a single transistor
- **THEN** only the geometry inside the box (plus the configured halo) SHALL be meshed

#### Scenario: Spanning contacts and vias
- **GIVEN** a via between metal1 and metal2
- **WHEN** the view is drawn with default options
- **THEN** the via SHALL be drawn with its own technology height and thickness
- **WHEN** the option to span connected layers is turned on
- **THEN** the via SHALL be drawn from the top of metal1 to the bottom of metal2

### Requirement: Interactive 3D inspection
The 3D view SHALL support the following:
- **Camera:** orbit, pan, zoom and preset views (top, isometric, front, side), driven by mouse, trackpad, keyboard or touch gestures.
- **Cross-sections:** along an axis-aligned cut line or plane. Heights are in µm regardless of magic-compat (view-3d, Q-MISC-72); mask layers without a `render` height are reported, not given one.
- **Layer visibility:** per-layer show, hide, isolate and transparency, acting on the same per-mode visibility sets as `see` (capability `view-3d`, Q-MISC-71).
- **Net highlighting:** highlight a selected net in 3D, using the connectivity of capability `layout-connectivity`.
- **Picking:** selecting a solid SHALL select the corresponding layout material in the 2D view, and the reverse.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-4)

#### Scenario: Cross-section
- **WHEN** a user draws a cut line across an inverter and requests a cross-section
- **THEN** the view SHALL show the layer stack along that line, with correct heights and thicknesses

#### Scenario: Touch orbit
- **GIVEN** an iPad
- **WHEN** the user drags with one finger in the 3D view
- **THEN** the camera SHALL orbit, and a two-finger pinch SHALL zoom

### Requirement: 3D export
Hefesto SHALL export the current 3D model:
- as glTF 2.0 (`.gltf` with `.bin`, or `.glb`), with one mesh node per layer and the layer names and display colors as material names and base colors
- as binary STL, either one file per layer or a single merged file, as the user chooses

Units SHALL be microns. Exported geometry SHALL equal the displayed model's geometry (layers, visibility, cut box) in physical microns, without the view's z exaggeration, unless a z scale is requested; the scale SHALL be recorded in the file (glTF `asset.extras`). Heights are in µm regardless of magic-compat (view-3d, Q-MISC-72); mask layers without a `render` height are reported, not given one.

glTF files SHALL be written with the cgltf writer (MIT; tinygltf, MIT, MAY replace it), on every platform (capability `hefesto-licensing`, requirement "Third-party libraries for cryptography, networking and data formats"; decision D-13). STL is written by Hefesto's own code.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-4)

#### Scenario: glTF export
- **WHEN** `export3d inv.glb` is run on a meshed cell
- **THEN** a valid glTF 2.0 binary SHALL be written, with one node per visible layer, in micron units

#### Scenario: Export without the view's exaggeration
- **GIVEN** a metal1 layer of thickness 0.36 µm displayed with z scale 25
- **WHEN** the model is exported without a z scale
- **THEN** the exported metal1 solid SHALL be 0.36 µm thick
- **WHEN** it is exported with a z scale of 4
- **THEN** the solid SHALL be 1.44 µm thick and the scale 4 SHALL be recorded in `asset.extras`

### Requirement: Parasitic and field overlays
The 3D view SHALL display extraction results as overlays:
- node capacitance as a color map on net geometry
- coupling capacitance as markers between coupled nets, filtered by a threshold
- resistance networks from the `extresist-*` capabilities, as edges with resistance values

When current or voltage data are available from an imported simulation result, the view SHALL display a current-density color map. The IR-drop and electromigration datasets, their color maps and thresholds are owned by capability `reliability-visualization`. Each overlay SHALL show its legend, units and value range. Values SHALL come directly from the extraction outputs and SHALL NOT be recomputed.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-4)

#### Scenario: Capacitance heat map
- **GIVEN** a cell extracted with coupling capacitance enabled
- **WHEN** the capacitance overlay is enabled
- **THEN** each net SHALL be colored by its total capacitance from the `.ext` data, with a legend in fF
