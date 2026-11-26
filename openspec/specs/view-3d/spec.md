# 3D View

## Purpose

Defines the 3D rendering view of a layout ("wind3d" window client): how it is opened and what it renders, its command set and key bindings, the cifoutput-section `render` statement that gives mask layers a display style, height and thickness, the use of extract-section layer `height` values, the viewing transform, and the rendering model. In Magic this client exists only when built with 3D support and only on the OpenGL display under Tk/X11; Hefesto specifies the behaviour independently of the graphics API.

Related capabilities (referenced, not duplicated): `tech-file-format`, `mask-output-styles` and `mask-output-operators` (mask layers generated for the CIF view), `extract-tech-section` (the `height` keyword, `planeorder`, units conversion), `display-style-files` (display style table, style names and numbers), `ui-windows-and-views` (window creation with `specialopen`, `closewindow`, `windownames`, key macros).

## Requirements

### Requirement: Render statement in the cifoutput section

A cifoutput style SHALL accept, outside of and after layer definitions, the statement `render <layer> <style> <height> <thick>` which assigns to every mask layer of the current output style whose name equals `<layer>` (exact, case-sensitive; several layers may share a name) a 3D display style, a height and a thickness. `<style>` is either an integer, matched against the style numbers of the technology display styles, or a style long name (exact match); the stored value is the style's position in the technology style table. `<height>` and `<thick>` are decimal numbers stored as single-precision floats without unit conversion. The statement ends any open layer definition. Without a `render` statement a mask layer has height 0, thickness 0 and the pale-highlight style. Errors (line ignored): wrong argument count, "Error:  Bad render style for CIF layer.", "Syntax: render <layer> <style> <height> <thick>" for non-numeric values, "Unknown layer name." when no layer matched. In builds without 3D support the statement is accepted and ignored. Hefesto resolves `<style>` when the 3D view builds its layer stack (the display styles load after the cifoutput section) and reports the bad-style error then; the statement is ignored as in Magic. Two differences from Magic follow and are accepted in both magic-compat settings (decision J2, approved 2026-11-21): a line whose style is bad is still checked for its syntax and layer name at load, so it may report "Syntax: render <layer> <style> <height> <thick>" or "Unknown layer name." where Magic, which checks the style first, reports only the bad style; and the bad-style error is reported once per `render` line on every build of the layer stack, not once per technology load. The stored values equal Magic's.

Compatibility: exact

```ebnf
render_line = "render" , ws , layer_name , ws , style , ws , height , ws , thick ;
style       = integer | style_long_name ;
height      = number ;   (* stored as given; units: see Q-MISC-72 *)
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

#### Scenario: Bad style reported when the layer stack is built
- **GIVEN** the line `render MET1 nosuchstyle 1.376 0.36` and no display style named `nosuchstyle`
- **WHEN** the technology is loaded and the 3D view then builds its layer stack twice
- **THEN** nothing is reported at load, "Error:  Bad render style for CIF layer." is reported at each build, and MET1 keeps height 0, thickness 0 and the pale-highlight style

### Requirement: Layer height and thickness for the layout-layer view

In the layout-layer (non-mask) view, the system SHALL place each tile using the height h and thickness t of its tile type from the current extract style (keyword `height`, owned by `extract-tech-section`), after the extract style's defaults and unit conversion: a type whose thickness is 0 gets t = 0.05 and a type whose height is 0 gets h = 0.1 × (plane order index of its plane), both before micron-to-internal conversion; with `units microns` both are divided by the micron-per-internal-unit scale. With `noplaneordering` neither the defaults nor the micron conversion apply: a type without a `height` line keeps h = t = 0 and `units microns` values stay unconverted, as in Magic, with magic-compat ON and OFF (decision J4, approved 2026-11-21). For a split (diagonal) tile, with magic-compat ON both triangles use the h and t of the tile's left-side type, as in Magic, so a split tile whose left half is space draws its other triangle flat at z = 0 with no side faces; with magic-compat OFF each triangle uses the h and t of its own type for its height, thickness and side faces (Q-MISC-79). If no extract style is loaded, h = t = 0.

Compatibility: exact

(Magic ref: extract/ExtTech.c:584-602, extract/ExtTech.c:3850-3856, extract/ExtTech.c:3962-3966)

#### Scenario: Default height from plane order
- **GIVEN** an extract style with `planeorder metal2 4`, no `height` line for metal2, and lambda units
- **WHEN** the 3D view renders a metal2 tile
- **THEN** it uses h = 0.4 and t = 0.05

#### Scenario: No defaults with noplaneordering
- **GIVEN** an extract style with `noplaneordering` and no `height` line for metal2
- **WHEN** the 3D view renders a metal2 tile
- **THEN** it uses h = 0 and t = 0 with magic-compat ON and OFF, and the tile is drawn flat

#### Scenario: Split tile whose left half is space
- **GIVEN** a split tile whose left half is space and whose right half is metal1 with h = 2.0 and t = 0.5
- **WHEN** the view is drawn in layout-layer mode
- **THEN** with magic-compat ON the metal1 triangle is drawn flat at z = 0 with no side faces, as in Magic (Q-MISC-79); with it OFF it is a prism from z = −50 to z = −62.5 at s_z = 25, with side faces

### Requirement: Opening the 3D window

The system SHALL open a 3D view with `specialopen wind3d [cellname [toolkit-path]]` (and the scripting convenience `render3d [cell]`, which opens it in its own top-level window (toolkit path `.render.magic`) and sets the cut box once from the layout box (`cutbox box`); the cut box does not follow later changes of the layout box), and SHALL enforce: at most one 3D view exists ("Only one 3D window allowed."); in Magic the display must be OpenGL ("Display type is ...  OpenGL is required for the 3D display."); the rendered cell is `cellname` if given, else the cell holding the layout box, else the root cell of the only layout window, else the error "Ambiguous directive:  Put cursor box in one of the windows."; the cell must already exist (it is read from disk if not yet loaded) — otherwise "Cells cannot be created in the 3D window." and no window is created. The window has no scroll bars, caption or border, an initial size of 500 × 500 pixels, the default toolkit path `.magic3d`, mask-layer mode on if a current output style exists, otherwise layout-layer mode (Q-MISC-73: Magic opens in mask-layer mode and dereferences the missing style; fixed), rendering level 1, and the default view of requirement "Default view". Mouse buttons perform no action. Closing the window (`<path> closewindow` or `closewindow` in it) removes its toolkit-path command, as Magic's GrTOGLDelete does: `info commands <path>` no longer lists it, calling it is "invalid command name", and a new `specialopen wind3d` on that path opens a new window, in both settings (Q-MISC-80, approved 2026-11-22, MISC-3D-12). With magic-compat ON, `specialopen wind3d <cell> <path>` on a path below a top-level window (such as `render3d`'s `.render.magic`) leaves Magic's `window "<path>" isn't a top-level window` (its failing `wm protocol`) as the command's result; with it OFF the result is empty; the window opens in both settings (decision W2, approved 2026-11-22).

Compatibility: behavioral

(Magic ref: graphics/W3Dmain.c:1402-1427, graphics/W3Dmain.c:1447-1598, graphics/W3Dmain.c:1804-1824, graphics/W3Dmain.c:719-738, tcltk/wrapper.tcl:1589-1614, windows/windMove.c:336-342)

#### Scenario: Second 3D window refused
- **GIVEN** a 3D view is open
- **WHEN** the user runs `specialopen wind3d`
- **THEN** the error "Only one 3D window allowed." is reported and no window opens

#### Scenario: Unknown cell
- **WHEN** the user runs `specialopen wind3d nosuchcell`
- **THEN** "Cells cannot be created in the 3D window." is reported and no window opens

#### Scenario: No output style
- **GIVEN** a technology with no cifoutput style
- **WHEN** the user runs `specialopen wind3d` on a cell
- **THEN** the 3D view opens in layout-layer mode with the layout-layer default view, `cif` keeps layout-layer mode and reports it, and `render MET1` reports "No CIF output style set!"

#### Scenario: Reopening after closewindow
- **GIVEN** a 3D view opened with `specialopen wind3d` on the default toolkit path `.magic3d`
- **WHEN** the user runs `.magic3d closewindow` and then `specialopen wind3d`
- **THEN** in both settings `info commands .magic3d` is empty after the close, `.magic3d view` is "invalid command name", and the second `specialopen wind3d` opens a new 3D view at `.magic3d` (Q-MISC-80)

#### Scenario: specialopen below a top-level window
- **GIVEN** a top-level window `.render` and no 3D view
- **WHEN** the user runs `specialopen wind3d c3d .render.magic`
- **THEN** the 3D view opens at `.render.magic` in both settings, and the command's result is `window ".render.magic" isn't a top-level window` with magic-compat ON and empty with it OFF

#### Scenario: render3d copies the box once
- **GIVEN** a layout box (0,0)-(10,10) in the rendered cell
- **WHEN** the user runs `render3d` and then moves the layout box to (20,20)-(30,30)
- **THEN** `cutbox` still returns the box (0,0)-(10,10)

### Requirement: Default view

The default view SHALL be computed from the rendered cell's bounding box (W × H, lower-left (x0, y0)) as: translation (tx, ty, tz) = (−(x0 + ⌊W/2⌋), −(y0 + ⌊H/2⌋), 0) (integer halving by arithmetic shift); xy-scale s_xy = min(2 / (1.1 W), 2 / (1.1 H)); z-scale s_z = 25 (height exaggeration); z prescale p_z = 0.0001; view angles (0, 0, 0) (top-down); all layers visible; no cut box. When W or H is 0, s_xy is fitted on the other dimension alone, and a cell with W = H = 0 is fitted as 1 × 1, with magic-compat ON and OFF (Magic divides by zero; decision J6, approved 2026-11-21). In mask-layer mode these values are then rescaled for mask units with factor f = output scalefactor: s_xy ← s_xy / f, p_z ← p_z / f, s_z ← s_z × f, tx ← tx × f, ty ← ty × f. The `defaults` command restores this view (keeping the current mode). A window opened with `specialopen wind3d` (by a script or the user) or by hefesto-cli keeps this view; when the desktop opens a window itself (with `render3d`, toolkit path `.render.magic`, as the 3D tab and Options > 3D Display do) the tab then applies the isometric preset from above once, framed to the cell, by running the same `view`, `zoom` and `scroll` commands the preset runs, so these commands report the view shown (Q-MISC-84, approved 2026-11-24). That opening `zoom` also sets s_z to the board value, so layer heights are drawn in proportion to the x/y extent rather than exaggerated 25 times: 1 in layout-layer mode; in mask-layer mode f with magic-compat OFF and f / (µm per internal unit) with magic-compat ON, where the `render` heights are kept as stored while x and y are in mask units (Q-MISC-72) (decisions A4, approved 2026-11-25, and B1, approved 2026-11-26; Q-MISC-84 amendments). The presets keep the current s_z, and `defaults`, scripts, hefesto-cli and the golden cases keep the values above.

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

#### Scenario: Empty cell
- **GIVEN** a cell whose bounding box is empty, (0,0)-(0,0), in layout-layer mode
- **WHEN** defaults are applied
- **THEN** translation is (0, 0, 0) and s_xy = 2/1.1 ≈ 1.81818, with magic-compat ON and OFF

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
| `cif` | toggles between mask-layer mode and layout-layer mode, rescaling the view by f (to mask mode) or 1/f (to layout mode) as in requirement "Default view"; f is the current output style's scalefactor (Q-MISC-78: with magic-compat OFF, leaving mask-layer mode divides by the scalefactor applied on entering it, and a change of output style while in mask-layer mode rescales the view from the old to the new scalefactor); switching to mask mode requires a current output style, otherwise layout-layer mode is kept and reported (Q-MISC-73); the mode is per window, and a new window draws in the mode it reports (Q-MISC-76: Magic keeps the redisplay procedure of the last toggle; fixed) |
| `level` / `level n\|up\|down` | returns / sets the rendering quality level (integer, clamped at 0 from below, with no upper limit as in Magic; a value or step beyond the integer range saturates instead of overflowing; decisions W3 and W4, approved 2026-11-22) |
| `defaults` | restores the default view; both visibility sets reset to all visible |
| `closewindow` | closes the 3D view |
| `render name` | returns (height, thickness, style) of the named mask layer, as stored; with no current output style: "No CIF output style set!" |
| `render name height thick [style]` | sets height and thickness (and integer style if given) of every mask layer with that name; unknown name: "Unknown CIF layer "<name>""; with no current output style: "No CIF output style set!" |
| `see [no] layer` | makes visible / hides, in the current mode's visibility set, the tile types (layout-layer mode) or every mask layer with that name (mask-layer mode) |
| `cutbox` / `cutbox none\|box\|llx lly urx ury` | returns the cut box or `none`; `none` disables clipping; `box` sets it to the layout box if the box is in the rendered cell; four integers set it in internal units |
| `help` | lists the commands |

Default key bindings: `L`/`l` level up/down, `C` cif, space defaults, Ctrl-L refresh, `Z`/`z` zoom ×2/×0.5 in xy, `>`/`<` zoom ×2/×0.5 in z, digits `1`–`6` rotate by ±10° about y, x, z, digits `7`–`0` by ±1° about y and x, `-`/`=` ±1° about z, arrow keys scroll by 0.25 in screen units.

While a 3D view is open, after a command sent through the 3D window's toolkit path (Magic's `_tk_dispatch` makes that window current with `TxSetPoint`) or a `setpoint x y <window>` naming that path, the layout commands a script runs (`box`, `load`, `cif ostyle`, ...) go to the 3D view with magic-compat ON, as in Magic, which dispatches them with the current window, then the 3D window: `box` and `load` fail there with an empty error, and `cif ostyle <style>` reaches the 3D view's `cif`. Opening the window alone does not make it current. A command sent through a layout window's path, a `setpoint` naming another window, a key or button in a layout window, or closing the 3D window sends them to the layout window again. With magic-compat OFF they always go to the layout window (Q-MISC-82; the `setpoint` trigger is Q-MISC-86, approved 2026-11-25). A numeric window ID naming the 3D window (in `setpoint` or another window-ID argument) is not supported, in both settings: it names no window, so script commands go to the layout window (a known gap, decision A2, approved 2026-11-25). With magic-compat ON, a command sent through the window's toolkit path whose wrapper tag runs `magic::scrollupdate` (`view`, `scroll`, `zoom`, `findbox`) or `magic::toolupdate` (`see`) also prints the two "Unknown command:" failures of the tag's `*bypass logcommands suspend|resume`, as Magic's OpenGL window does; with it OFF nothing more is printed (decision W1, approved 2026-11-22).

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

#### Scenario: Output style changed in mask-layer mode
- **GIVEN** a 3D view in layout-layer mode with s_z = 25 and a current output style with scalefactor 10
- **WHEN** the user runs `cif`, then `cif ostyle` selecting a style with scalefactor 5, then `cif`
- **THEN** with magic-compat ON `zoom` returns s_z = 50, as in Magic (Q-MISC-78); with it OFF it returns s_z = 25

#### Scenario: Level limits
- **GIVEN** a 3D view at level 1
- **WHEN** the user runs `level -5`, then `level 99999999999`, then `level up`
- **THEN** `level` returns 0 after the first command and 2147483647 after the second and the third, with magic-compat ON and OFF

#### Scenario: Script layout commands reach the 3D window with magic-compat ON
- **GIVEN** magic-compat ON, a layout window on cell `c3d` and a script that runs `specialopen wind3d c3d .render.magic`, then `.render.magic cutbox none`, then `box 0 0 10 10`
- **WHEN** the script runs
- **THEN** `box` is sent to the 3D view, which has no such command, and fails with an empty error, as in Magic; the layout box is unchanged (Q-MISC-82; MISC-3D-05)

#### Scenario: Script layout commands reach the layout window with magic-compat OFF
- **GIVEN** magic-compat OFF and the same script
- **WHEN** the script runs
- **THEN** `box` sets the layout box to (0,0)-(10,10) and the 3D view is unchanged (Q-MISC-82)

#### Scenario: Opening the 3D window alone leaves script commands in the layout window
- **GIVEN** magic-compat ON and a script that runs `specialopen wind3d c3d .render.magic` and then `box 0 0 10 10`, with no command on the window's path in between
- **WHEN** the script runs
- **THEN** `box` sets the layout box to (0,0)-(10,10): `specialopen` does not make the 3D window current, as in Magic (Q-MISC-82)

#### Scenario: A layout key or button makes the layout window current again
- **GIVEN** magic-compat ON, an open 3D window, a command sent through its path, and a layout button macro `box 20 10 70 45`
- **WHEN** the user presses the button in the layout window
- **THEN** the layout box becomes (20,10)-(70,45), as Magic's `TxSetPoint` on a layout event makes that window current; with magic-compat OFF the box changes the same way (Q-MISC-82)

#### Scenario: setpoint names the window script commands go to
- **GIVEN** magic-compat ON and an open 3D window at `.render.magic`
- **WHEN** a script runs `setpoint 10 10 .render.magic` and then `box 2 2 6 6`, then `setpoint 10 10 .layout1` and `box 2 2 6 6` again
- **THEN** the first `box` fails with an empty error and the second sets the layout box, as Magic's `setpoint` makes the named window current (windCmdSZ.c:286); with magic-compat OFF both set the layout box and `setpoint` never routes script commands to the 3D view (Q-MISC-86)

#### Scenario: Wrapper console noise
- **GIVEN** a 3D view opened at `.render.magic` with the wrapper's `magic::scrollupdate` tag on `view`
- **WHEN** the user runs `.render.magic view`
- **THEN** it returns the view angles in both settings; with magic-compat ON the console also shows "Unknown command:" twice, followed by `'logcommands' 'suspend'` and `'logcommands' 'resume'`; with it OFF nothing more is printed (decision W1)

#### Scenario: Visibility sets are per mode
- **GIVEN** a 3D view in mask-layer mode
- **WHEN** the user runs `see no MET1`, `cif` and `cif` again
- **THEN** every tile type is visible while the view is in layout-layer mode, and MET1 is still hidden when it is back in mask-layer mode

### Requirement: Rendered geometry

The system SHALL render, in layout-layer mode, every non-space tile of the rendered cell and all its descendants (flattened through instance transforms) whose type is visible, grouped by display style in style-table order, clipped to the cut box if one is set (otherwise to the window area enlarged by one unit), as a prism whose top face is at z_top = −h·s_z and bottom face at z_bot = z_top − t·s_z (h, t from requirement "Layer height and thickness"), drawn with the tile type's display style; and in mask-layer mode, the mask layers generated by the current output style for the flattened cell (over the area given below), each visible mask layer's rectangles as prisms with z_top = −h·s_z and z_bot = z_top − t·s_z using the `render` height, thickness and style: h and t as stored with magic-compat ON, as in Magic; with it OFF taken as micrometres and converted to internal units by the output style's scale, cs_scaleFactor / (100 · cs_expander) µm per internal unit (Q-MISC-72). The mask layers are generated over the cut box or, without one, over the cell bounding box (magic-compat ON, as Magic: a layer grown past the cell edge is cut there) or that box grown by the style's interaction radius (magic-compat OFF) (Q-MISC-87, approved 2026-11-26); they are drawn in mask units (internal units times the scale factor). Magic draws every CIF layer of the style, temporary layers included, and so does Hefesto with magic-compat ON; with magic-compat OFF temporary layers are not drawn (Q-MISC-88, approved 2026-11-26). Magic adds the generation errors of the 3D view's mask generation as feedback on every redraw; with magic-compat ON Hefesto reports them once per build (a full build or an incremental rebuild) to the console and adds no feedback; with magic-compat OFF they are not reported (Q-MISC-89, approved 2026-11-26). Mask-layer planes are drawn already clipped, so a clipped split tile is closed along its own cut in both settings (Q-MISC-83 concerns the layout-layer clip). A mask layer with no `render` statement is drawn flat (h = t = 0) in both settings. Prisms have top and bottom faces and side faces on the boundary of each layer's flattened geometry, where the neighbour is another layer, space or the clip edge (only when z_top ≠ z_bot); pieces of one layer that abut across cells get no side faces between them (Magic draws each cell's tile outlines, so it emits such inner faces; decision J6, approved 2026-11-21); diagonal (split) tiles render as triangles with a diagonal side face; faces cut by the clip area (the cut box, or the window area grown by one unit) are closed: for a split tile, with magic-compat ON along the edges of its bounding rectangle clipped to the area, as Magic does (so the cut face also spans the part of the rectangle the triangle does not cover), and with it OFF along the triangle's own cut (Q-MISC-83). Only solid or stippled styles are rendered; outlines, contact crosses and labels are not; the same rule applies to the GL face log Hefesto writes for the golden geometry comparison, with magic-compat ON and OFF (decision N3, approved 2026-11-22; MISC-3D-15). Tiles of subcells are clipped after their instance transform, in root coordinates (Q-MISC-70: Magic clips against the area in the subcell's coordinates; fixed). Each mode keeps its own visibility set: tile types in layout-layer mode, mask-layer indices of the current output style in mask-layer mode, the latter reset to all visible when the output style changes (Q-MISC-71: Magic shares one; fixed).

Compatibility: behavioral

(Magic ref: graphics/W3Dmain.c:234-446, graphics/W3Dmain.c:549-636, graphics/W3Dmain.c:1650-1784)

#### Scenario: Metal prism in layout-layer mode
- **GIVEN** a metal1 tile (0,0)-(10,4) with h = 2.0, t = 0.5 and s_z = 25
- **WHEN** the view is drawn
- **THEN** a box spanning x 0..10, y 0..4 and z −62.5..−50 is rendered in the metal1 display style

#### Scenario: Transformed subcell clipped in root coordinates
- **GIVEN** a root cell containing an instance of a subcell translated by (1000, 0), and a cut box covering part of the instance
- **WHEN** the view is drawn in layout-layer mode
- **THEN** the instance's prisms are clipped exactly to the cut box in root coordinates, with closed cut faces and no other side faces

#### Scenario: Abutting cells leave no inner faces
- **GIVEN** two instances of a cell holding a metal1 rectangle (0,0)-(10,10), placed at x = 0 and x = 10 so that the rectangles abut
- **WHEN** the view is drawn in layout-layer mode
- **THEN** metal1 has side faces only on the outline of (0,0)-(20,10), none along x = 10

#### Scenario: Split tile under the cut box
- **GIVEN** a split tile (0,0)-(10,10) with diagonal from (0,0) to (10,10) whose lower-right triangle is metal1, and the cut box (0,0)-(5,10)
- **WHEN** the view is drawn in layout-layer mode
- **THEN** with magic-compat ON the closed cut face at x = 5 spans y 0..10, the tile's bounding rectangle clipped to the cut box, as in Magic; with it OFF it spans y 0..5, the triangle's own cut (Q-MISC-83)

#### Scenario: Outline style draws nothing
- **GIVEN** a contact type that belongs to a solid display style and to an outline (cross) style
- **WHEN** the view is drawn in layout-layer mode
- **THEN** only the solid style's prism is drawn, and the GL face log lists faces of the solid style only, with magic-compat ON and OFF (decision N3)

#### Scenario: Grown mask layer at the cell edge
- **GIVEN** a mask layer that grows metal1 by 20 mask units and no cut box
- **WHEN** the view is drawn in mask-layer mode
- **THEN** with magic-compat ON it ends at the cell bounding box, as in Magic; with it OFF it extends 20 units past it (Q-MISC-87)

#### Scenario: Temporary mask layer drawn with magic-compat ON
- **GIVEN** an output style with a `templayer` that has a `render` height, magic-compat ON and mask-layer mode
- **WHEN** the view is drawn
- **THEN** the temporary layer's rectangles are drawn as prisms like any mask layer, as in Magic (Q-MISC-88)

#### Scenario: Temporary mask layer left out with magic-compat OFF
- **GIVEN** the same output style and view with magic-compat OFF
- **WHEN** the view is drawn
- **THEN** the temporary layer is not drawn; the mask layers are drawn as with magic-compat ON (Q-MISC-88)

#### Scenario: Mask generation errors reported once per build with magic-compat ON
- **GIVEN** magic-compat ON, mask-layer mode and an output style whose generation reports an error for the rendered cell
- **WHEN** the 3D view builds its mesh and is then redrawn twice without a change
- **THEN** the generation error is printed to the console once, for the build, and no feedback area is added to the cell (Q-MISC-89)

#### Scenario: Mask generation errors not reported with magic-compat OFF
- **GIVEN** the same cell, style and view with magic-compat OFF
- **WHEN** the 3D view builds its mesh and is redrawn
- **THEN** nothing is printed for the generation error and no feedback area is added (Q-MISC-89)

#### Scenario: Render heights in micrometres with magic-compat OFF
- **GIVEN** `render MET1 metal1 1.376 0.36` and a current output style with 0.01 µm per internal unit
- **WHEN** the view is drawn in mask-layer mode
- **THEN** with magic-compat ON MET1 is placed with h = 1.376 and t = 0.36, as in Magic; with it OFF with h = 137.6 and t = 36 internal units; in both settings `render MET1` returns 1.376, 0.36 and the style

### Requirement: Viewing transform and shading

The system SHALL map a model point v = (x, y, z) (x, y in internal or mask units, z as computed above) to normalized view coordinates by v' = A · S · Rx(ax) · Ry(ay) · Rz(az) · T · v (Magic's mapping, which the view state and the face log follow; the renderer applies it to the model mirrored in z, below), where T translates by (tx, ty, tz), Rx/Ry/Rz are right-handed rotations by the view angles in degrees, S = diag(s_xy, s_xy, p_z), and, for the window's current size of W_w × H_w pixels, A = diag(H_w / W_w, 1, 1) when W_w ≥ H_w and A = diag(1, W_w / H_w, 1) otherwise, so units stay 1:1 and the limiting dimension is fitted; the current size also sets the viewport and the refresh area (Q-MISC-74: Magic keeps 500 × 500 after a resize, and its formula, the first case, overflows the width of a window taller than wide; fixed; the second case is an amendment approved 2026-11-21, decision J1), with an orthographic projection of the cube [−1, 1]³ onto the window (no perspective); faces are emitted counter-clockwise when seen from outside and back faces are not drawn. Faces are depth-tested; a type in several display styles is drawn once per style, in style-table order, and the last style drawn wins on equal depth (Q-MISC-77: Magic draws in style order without a depth test; fixed). The depth comparison and the culled side are chosen so that higher layers are in front in the top view, with magic-compat ON and OFF (Q-MISC-81: with a depth test, Magic's face orientation would show lower layers in front; fixed). A display style that Magic draws translucent (blended over the background) is drawn opaque in its plain colour, with magic-compat ON and OFF (decision R3, approved 2026-11-22). The desktop 3D tab draws the approved board look with magic-compat ON and OFF alike (Q-MISC-84, approved 2026-11-24; amends decision R2 of 2026-11-22 for the desktop tab): each face is shaded flat by one white directional light coming from above, at a fixed elevation and turning with the view's azimuth (z angle) so the screen-left and screen-right sides stay distinct at every orbit angle, evaluated on the face's own outward normal in the displayed scene; the displayed scene is the model mirrored in z, so that the projection and the depth test both put the viewer above the cell and every tilted view shows the higher layers up: the renderer draws v' = A · S · Rx(ax) · Ry(ay) · Rz(az) · T · M · v with M = diag(1, 1, −1), so a height h is displayed at z = +h · s_z and the translation's z (tz) moves along the displayed height (the opposite sign of Magic's model z; decision A5, approved 2026-11-25, Q-MISC-84 amendment); the outputs of `view`, `scroll` and `zoom` and the window's view state stay Magic's; the face's colour is the display style's colour times the face's factor, so colours stay saturated: top faces get the largest factor, between 1.0 and 1.1, each side face a factor at least 0.15 below it, so edges read, and no face is washed to white unless its style colour is white; the background is the desktop theme's canvas colour (`ThemeTokens::canvas`). The z scale the desktop tab sets on opening (requirement "Default view") is 1 in layout-layer mode and, in mask-layer mode, f with magic-compat OFF and f / (µm per internal unit) with magic-compat ON, so the mask-layer stack keeps the proportions of the layout-layer one in both settings (decisions A4 and B1). Magic's fixed-function lighting (no normal set, so every face has the normal (0, 0, 1), lit per vertex by one white diffuse light positioned at (50, 50, 50) in model coordinates plus an ambient light of 0.6) is not used by the desktop tab; outside the desktop tab (a headless image) the background is the display's background colour. At level ≥ 1 edges and polygons are anti-aliased. Exact pixel output is host-dependent.

Compatibility: informative

(Magic ref: graphics/W3Dmain.c:471-544, graphics/W3Dmain.c:449-466, graphics/W3Dmain.c:87-109, graphics/wind3d.h:18-45)

#### Scenario: Top-down default shows the layout
- **GIVEN** the default view of a cell
- **WHEN** the window is drawn
- **THEN** the cell appears as in a plan view, centred, filling about 91% (1/1.1) of the limiting window dimension

#### Scenario: Higher layers in front in the top view
- **GIVEN** metal1 (h = 2.0) and poly (h = 1.0) overlapping at (0,0)-(10,10)
- **WHEN** the default top view is drawn
- **THEN** the overlap shows metal1, with magic-compat ON and OFF (Q-MISC-81)

#### Scenario: Board look with magic-compat ON
- **GIVEN** magic-compat ON and the desktop 3D tab on a cell with a metal1 rectangle, in the isometric preset
- **WHEN** the tab is drawn
- **THEN** every face of the metal1 prism is one flat colour in the metal1 style's hue (no fade across a face), its top face is lighter than its side faces and the background is the theme's canvas colour; Magic's lighting with the normal (0, 0, 1) is not used (Q-MISC-84)

#### Scenario: Board look with magic-compat OFF
- **GIVEN** magic-compat OFF and the same cell, tab and view
- **WHEN** the tab is drawn
- **THEN** the image is the same as with magic-compat ON (Q-MISC-84)

#### Scenario: Top faces lighter than side faces
- **GIVEN** a metal1 prism in the isometric preset of the desktop 3D tab
- **WHEN** the tab is drawn
- **THEN** its top face is lighter than each of its visible side faces, all of them in the metal1 style's hue and none of them white, with magic-compat ON and OFF (Q-MISC-84)

#### Scenario: Background from the theme
- **GIVEN** the desktop 3D tab, in the dark theme and in the light theme
- **WHEN** the tab is drawn
- **THEN** the pixels outside the model have the theme's canvas colour (`ThemeTokens::canvas`), not the display styles' background colour, with magic-compat ON and OFF (Q-MISC-84)

#### Scenario: Isometric default from above
- **GIVEN** a cell with poly (h = 1.0) under metal1 (h = 2.0) where they overlap
- **WHEN** the user opens the desktop 3D tab on it (the tab runs `render3d`)
- **THEN** `view` returns the isometric preset's angles, the cell is framed in the view, and on screen metal1's top face is above and in front of poly where they overlap and the top faces of the higher layers are visible, with magic-compat ON and OFF (Q-MISC-84)

#### Scenario: Isometric default in mask-layer mode
- **GIVEN** the desktop 3D tab opening a window in mask-layer mode with output scalefactor f
- **WHEN** the first mesh is drawn
- **THEN** `view` returns the isometric preset's angles, with magic-compat ON and OFF, and `zoom` returns s_z = f with magic-compat OFF and s_z = f / (µm per internal unit) with it ON, so the heights keep the same proportion to the f-scaled x and y as s_z = 1 gives in layout-layer mode (Q-MISC-84; decisions A4 and B1)

#### Scenario: Mask-layer board view in proportion with magic-compat ON
- **GIVEN** magic-compat ON, an output style with scalefactor f = 10 and 0.005 µm per internal unit, and `render MET1 metal1 1.376 0.36`
- **WHEN** the desktop 3D tab opens a window in mask-layer mode
- **THEN** `zoom` returns s_z = 2000 (f / 0.005), MET1's top is displayed at 1.376 × 2000 = 2752 mask units, 1.376 µm at the mask units' 0.0005 µm each, and `render MET1` still returns 1.376 and 0.36 (decision B1)

#### Scenario: Mask-layer board view with magic-compat OFF
- **GIVEN** the same technology and output style with magic-compat OFF
- **WHEN** the desktop 3D tab opens a window in mask-layer mode
- **THEN** `zoom` returns s_z = 10 (f), and MET1, stored by Q-MISC-72 as h = 275.2 internal units, is displayed at 2752 mask units, as with magic-compat ON (decision B1)

#### Scenario: Magic's z scale unchanged outside the desktop tab in mask-layer mode
- **GIVEN** magic-compat ON and a window opened by `specialopen wind3d` in mask-layer mode with f = 10
- **WHEN** the first mesh is drawn, and later the user runs `defaults` in a desktop-opened window
- **THEN** `zoom` returns s_z = 250 (25 × f) in both cases, Magic's value (decision B1)

#### Scenario: specialopen keeps the top-down view in the desktop
- **GIVEN** the desktop app and no 3D window
- **WHEN** a script runs `specialopen wind3d c3d` (toolkit path `.magic3d`)
- **THEN** `view` returns 0 0 0 after the first mesh is drawn, with magic-compat ON and OFF (Q-MISC-84)

#### Scenario: Translucent style drawn opaque
- **GIVEN** a display style that Magic draws translucent
- **WHEN** a tile drawn in that style is shown
- **THEN** its faces are opaque, in the style's plain colour, with magic-compat ON and OFF (decision R3)

#### Scenario: Tall window
- **GIVEN** the default view of a 100 × 100 cell in a window of 500 × 800 pixels
- **WHEN** the window is drawn
- **THEN** A = diag(1, 0.625, 1) and the cell spans about 91% of the window's width and about 57% of its height, centred, with equal units in x and y (Q-MISC-74)

### Requirement: Platform mechanism of the 3D view

Hefesto SHALL provide the 3D view on every platform, including iPadOS, through the rendering interface of capability `hefesto-rendering` (Vulkan on the desktop, Metal on iPadOS; decision P-3), with touch or pointer gestures equivalent to the key bindings (see Q-MISC-75). Hefesto's extensions of the view are owned by capability `hefesto-3d-visualization`. Informative: Magic provides the view only when it is compiled with 3D support and runs with the OpenGL display driver, Tk and GLX.

Compatibility: informative

(Magic ref: graphics/W3Dmain.c:1470-1480, graphics/W3Dmain.c:1538-1586, windows/windCmdSZ.c:497-503)

#### Scenario: No OpenGL display in Magic
- **GIVEN** Magic running with a Cairo display
- **WHEN** the user runs `specialopen wind3d`
- **THEN** Magic reports that OpenGL is required, whereas Hefesto opens the 3D view
