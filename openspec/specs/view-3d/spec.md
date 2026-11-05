# 3D View

## Purpose

Defines the 3D rendering view of a layout ("wind3d" window client): how it is opened and what it renders, its command set and key bindings, the cifoutput-section `render` statement that gives mask layers a display style, height and thickness, the use of extract-section layer `height` values, the viewing transform, and the rendering model. In Magic this client exists only when built with 3D support and only on the OpenGL display under Tk/X11; Hefesto specifies the behaviour independently of the graphics API.

Related capabilities (referenced, not duplicated): `tech-file-format`, `mask-output-styles` and `mask-output-operators` (mask layers generated for the CIF view), `extract-tech-section` (the `height` keyword, `planeorder`, units conversion), `display-style-files` (display style table, style names and numbers), `ui-windows-and-views` (window creation with `specialopen`, `closewindow`, `windownames`, key macros).

## Requirements

### Requirement: Render statement in the cifoutput section

A cifoutput style SHALL accept, outside of and after layer definitions, the statement `render <layer> <style> <height> <thick>` which assigns to every mask layer of the current output style whose name equals `<layer>` (exact, case-sensitive; several layers may share a name) a 3D display style, a height and a thickness. `<style>` is either an integer, matched against the style numbers of the technology display styles, or a style long name (exact match); the stored value is the style's position in the technology style table. `<height>` and `<thick>` are decimal numbers stored as single-precision floats without unit conversion. The statement ends any open layer definition. Without a `render` statement a mask layer has height 0, thickness 0 and the pale-highlight style. Errors (line ignored): wrong argument count, "Error:  Bad render style for CIF layer.", "Syntax: render <layer> <style> <height> <thick>" for non-numeric values, "Unknown layer name." when no layer matched. In builds without 3D support the statement is accepted and ignored.

Compatibility: exact

```ebnf
render_line = "render" , ws , layer_name , ws , style , ws , height , ws , thick ;
style       = integer | style_long_name ;
height      = number ;   (* stored as given; see Q-MISC-72 *)
thick       = number ;
```

Example:

```
cifoutput
style gdsii
 scalefactor 10 nanometers
 layer MET1 m1,m1c
  calma 68 20
 render MET1 metal1 1.376 0.36
end
```

(Magic ref: cif/CIFtech.c:869-876, cif/CIFtech.c:992-1036, dbwind/DBWdisplay.c:1688-1710)

#### Scenario: Render by style name
- **GIVEN** a display style table whose style with long name `metal1` is at technology-style position 20, and the line `render MET1 metal1 1.376 0.36`
- **WHEN** the technology is loaded
- **THEN** mask layer MET1 has render style 20, height 1.376 and thickness 0.36

#### Scenario: Unknown layer
- **GIVEN** the line `render NOSUCH metal1 1 1`
- **WHEN** the technology is loaded
- **THEN** the error "Unknown layer name." is reported and no layer changes

### Requirement: Layer height and thickness for the layout-layer view

In the layout-layer (non-mask) view, the system SHALL place each tile using the height h and thickness t of its tile type from the current extract style (keyword `height`, owned by `extract-tech-section`), after the extract style's defaults and unit conversion: a type whose thickness is 0 gets t = 0.05 and a type whose height is 0 gets h = 0.1 × (plane order index of its plane), both before micron-to-internal conversion; with `units microns` both are divided by the micron-per-internal-unit scale. For a split (diagonal) tile the left-side type is used. If no extract style is loaded, h = t = 0.

Compatibility: exact

(Magic ref: extract/ExtTech.c:584-602, extract/ExtTech.c:3850-3856, extract/ExtTech.c:3962-3966)

#### Scenario: Default height from plane order
- **GIVEN** an extract style with `planeorder metal2 4`, no `height` line for metal2, and lambda units
- **WHEN** the 3D view renders a metal2 tile
- **THEN** it uses h = 0.4 and t = 0.05

### Requirement: Opening the 3D window

The system SHALL open a 3D view with `specialopen wind3d [cellname [toolkit-path]]` (and the scripting convenience `render3d [cell]`, which opens it in its own top-level window with the cut box following the layout box), and SHALL enforce: at most one 3D view exists ("Only one 3D window allowed."); in Magic the display must be OpenGL ("Display type is ...  OpenGL is required for the 3D display."); the rendered cell is `cellname` if given, else the cell holding the layout box, else the root cell of the only layout window, else the error "Ambiguous directive:  Put cursor box in one of the windows."; the cell must already exist (it is read from disk if not yet loaded) — otherwise "Cells cannot be created in the 3D window." and no window is created. The window has no scroll bars, caption or border, an initial size of 500 × 500 pixels, the default toolkit path `.magic3d`, mask-layer mode on, rendering level 1, and the default view of requirement "Default view". Mouse buttons perform no action.

Compatibility: behavioral

(Magic ref: graphics/W3Dmain.c:1402-1427, graphics/W3Dmain.c:1447-1598, graphics/W3Dmain.c:1804-1824, tcltk/wrapper.tcl:1589-1614, windows/windMove.c:336-342)

#### Scenario: Second 3D window refused
- **GIVEN** a 3D view is open
- **WHEN** the user runs `specialopen wind3d`
- **THEN** the error "Only one 3D window allowed." is reported and no window opens

#### Scenario: Unknown cell
- **WHEN** the user runs `specialopen wind3d nosuchcell`
- **THEN** "Cells cannot be created in the 3D window." is reported and no window opens

### Requirement: Default view

The default view SHALL be computed from the rendered cell's bounding box (W × H, lower-left (x0, y0)) as: translation (tx, ty, tz) = (−(x0 + ⌊W/2⌋), −(y0 + ⌊H/2⌋), 0) (integer halving by arithmetic shift); xy-scale s_xy = min(2 / (1.1 W), 2 / (1.1 H)); z-scale s_z = 25 (height exaggeration); z prescale p_z = 0.0001; view angles (0, 0, 0) (top-down); all layers visible; no cut box. In mask-layer mode these values are then rescaled for mask units with factor f = output scalefactor: s_xy ← s_xy / f, p_z ← p_z / f, s_z ← s_z × f, tx ← tx × f, ty ← ty × f. The `defaults` command restores this view (keeping the current mode).

Compatibility: exact

(Magic ref: graphics/W3Dmain.c:1332-1380, graphics/W3Dmain.c:847-857, graphics/W3Dmain.c:975-988)

#### Scenario: Default view of a 100 x 50 cell in layout-layer mode
- **GIVEN** a cell with bounding box (0,0)-(100,50) and layout-layer mode
- **WHEN** defaults are applied
- **THEN** translation is (−50, −25, 0), s_xy = 2/110 ≈ 0.0181818, s_z = 25, p_z = 0.0001 and angles are (0,0,0)

#### Scenario: Same cell in mask-layer mode with scalefactor 10
- **GIVEN** the same cell, mask-layer mode and output scalefactor 10
- **WHEN** defaults are applied
- **THEN** translation is (−500, −250, 0), s_xy ≈ 0.00181818, s_z = 250 and p_z = 0.00001

### Requirement: 3D view commands

The 3D view SHALL accept the following commands (exact names; extra or missing arguments produce the usage message shown and no change; every successful change triggers a full redraw):

| Command | Effect |
|---|---|
| `view` | returns the three view angles (x, y, z) in degrees |
| `view ax ay az [abs*\|rel*]` | sets (default/absolute) or adds (relative, keyword prefix `rel`) the rotation angles; a fourth word not starting with `rel` or `abs` is an error |
| `scroll` | returns the translation (tx, ty, tz) |
| `scroll x y z [abs*\|rel*]` | absolute: sets the translation; relative: adds x/s_xy, y/s_xy, z/s_xy |
| `zoom` | returns (s_xy, s_z) |
| `zoom xy z [abs*\|rel*]` | sets or multiplies s_xy and s_z; non-positive values: "Error: zoom values/factors must be positive and nonzero" |
| `refresh` | redraws |
| `cif` | toggles between mask-layer mode and layout-layer mode, rescaling the view by f (to mask mode) or 1/f (to layout mode) as in requirement "Default view"; switching to mask mode requires a current output style (see Q-MISC-73) |
| `level` / `level n\|up\|down` | returns / sets the rendering quality level (integer, clamped at ≥ 0) |
| `defaults` | restores the default view |
| `closewindow` | closes the 3D view |
| `render name` | returns (height, thickness, style) of the named mask layer |
| `render name height thick [style]` | sets height and thickness (and integer style if given) of every mask layer with that name; unknown name: "Unknown CIF layer "<name>"" |
| `see [no] layer` | makes visible / hides layers: tile types (layout-layer mode) or one mask layer name (mask-layer mode) |
| `cutbox` / `cutbox none\|box\|llx lly urx ury` | returns the cut box or `none`; `none` disables clipping; `box` sets it to the layout box if the box is in the rendered cell; four integers set it in internal units |
| `help` | lists the commands |

Default key bindings: `L`/`l` level up/down, `C` cif, space defaults, Ctrl-L refresh, `Z`/`z` zoom ×2/×0.5 in xy, `>`/`<` zoom ×2/×0.5 in z, digits `1`–`6` rotate by ±10° about y, x, z, digits `7`–`0` by ±1° about y and x, `-`/`=` ±1° about z, arrow keys scroll by 0.25 in screen units.

Compatibility: behavioral

(Magic ref: graphics/W3Dmain.c:651-1315, graphics/W3Dmain.c:1843-1890, magic/proto.magicrc.in:285-310)

#### Scenario: Relative rotation
- **GIVEN** view angles (0, 0, 0)
- **WHEN** the user types `view 0 10 0 rel` twice
- **THEN** `view` returns (0, 20, 0)

#### Scenario: Relative scroll in screen units
- **GIVEN** s_xy = 0.02 and translation (−50, −25, 0)
- **WHEN** the user runs `scroll 0.25 0 0 rel`
- **THEN** the translation becomes (−37.5, −25, 0)

#### Scenario: Zoom rejects zero
- **WHEN** the user runs `zoom 0 1`
- **THEN** the zoom error is reported and the scales are unchanged

### Requirement: Rendered geometry

The system SHALL render, in layout-layer mode, every non-space tile of the rendered cell and all its descendants (flattened through instance transforms) whose type is visible, grouped by display style in style-table order, clipped to the cut box if one is set (otherwise to the window area enlarged by one unit), as a prism whose top face is at z_top = −h·s_z and bottom face at z_bot = z_top − t·s_z (h, t from requirement "Layer height and thickness"), drawn with the tile type's display style; and in mask-layer mode, the mask layers generated by the current output style for the flattened cell over the cell bounding box (or the cut box) grown by the style's interaction radius, each visible mask layer's rectangles as prisms with z_top = −h·s_z and z_bot = z_top − t·s_z using the `render` height, thickness and style. Prisms have top and bottom faces and side faces on every tile boundary segment (only when z_top ≠ z_bot); diagonal (split) tiles render as triangles with a diagonal side face; faces cut by the cut box are closed. Only solid or stippled styles are rendered; outlines, contact crosses and labels are not. Magic clips subcell tiles against a clip area expressed in the subcell's own coordinates (see Q-MISC-70), and one visibility set is shared by both modes although it holds tile types in one and mask-layer indices in the other (see Q-MISC-71).

Compatibility: behavioral

(Magic ref: graphics/W3Dmain.c:234-446, graphics/W3Dmain.c:549-636, graphics/W3Dmain.c:1650-1784)

#### Scenario: Metal prism in layout-layer mode
- **GIVEN** a metal1 tile (0,0)-(10,4) with h = 2.0, t = 0.5 and s_z = 25
- **WHEN** the view is drawn
- **THEN** a box spanning x 0..10, y 0..4 and z −62.5..−50 is rendered in the metal1 display style

### Requirement: Viewing transform and shading

The system SHALL map a model point v = (x, y, z) (x, y in internal or mask units, z as computed above) to normalized view coordinates by v' = A · S · Rx(ax) · Ry(ay) · Rz(az) · T · v, where T translates by (tx, ty, tz), Rx/Ry/Rz are right-handed rotations by the view angles in degrees, S = diag(s_xy, s_xy, p_z), and A = diag(H_w / W_w, 1, 1) for a window of W_w × H_w pixels (Magic keeps 500 × 500 regardless of resizing; see Q-MISC-74), with an orthographic projection of the cube [−1, 1]³ onto the window (no perspective); faces are emitted counter-clockwise when seen from outside and back faces are not drawn. Shading uses an ambient light of 0.4 and one white diffuse light at (50, 50, 50) applied to the style colour; the background is the display's background colour; at level ≥ 1 edges and polygons are anti-aliased. Exact pixel output is host-dependent.

Compatibility: informative

(Magic ref: graphics/W3Dmain.c:471-544, graphics/W3Dmain.c:449-466, graphics/W3Dmain.c:87-109, graphics/wind3d.h:18-45)

#### Scenario: Top-down default shows the layout
- **GIVEN** the default view of a cell
- **WHEN** the window is drawn
- **THEN** the cell appears as in a plan view, centred, filling about 91% (1/1.1) of the limiting window dimension

### Requirement: Platform mechanism of the 3D view

Magic's 3D view requires a build with 3D support, the OpenGL display driver, Tk and GLX; Hefesto SHALL provide the same view using the platform's native 3D API (including iPadOS) and touch or pointer gestures equivalent to the key bindings (see Q-MISC-75).

Compatibility: informative

(Magic ref: graphics/W3Dmain.c:1470-1480, graphics/W3Dmain.c:1538-1586, windows/windCmdSZ.c:497-503)

#### Scenario: No OpenGL display in Magic
- **GIVEN** Magic running with a Cairo display
- **WHEN** the user runs `specialopen wind3d`
- **THEN** Magic reports that OpenGL is required, whereas Hefesto opens the 3D view
