# Hefesto Design System

## Purpose
Defines Hefesto's visual design system, "forge" (decision P-23):
- design tokens: color, typography, spacing, radius, elevation and motion
- the icon style
- the component patterns shared by the desktop app (Raven themes) and the iPadOS app (SwiftUI)

Reference mockups of the main screens are kept in the repository under `docs/design/mockups/` (with renders in `docs/images/ui/`). The M1 boards were first drawn in the design artifact https://claude.ai/artifact/9fWfmyGAx1EYtzhmgqyeeK (Hefesto UI Mockups); since 2026-11-10 the repository copies are the source of truth (user decision, M2 task 2.1). The guiding rule is that UI chrome stays neutral, so PDK layer colors own the canvas.

## Requirements

### Requirement: Color tokens and themes
Hefesto SHALL define its UI colors as named tokens with a dark theme (the default) and a light theme. The token values SHALL be:

| Token | Dark | Light |
|---|---|---|
| surface-0 | #121418 | #F6F7F9 |
| surface-1 | #181B20 | #FFFFFF |
| surface-2 | #20242B | #EEF0F3 |
| border | #2F343D | #D9DDE3 |
| text-primary | #E8EAED | #16191D |
| text-secondary | #A4ABB6 | #4E5663 |
| accent (ember) | #FF8A3D | #C24E0C |
| canvas | #0B0C0E | #0B0C0E |

The semantic colors SHALL be:

| Token | Value |
|---|---|
| success | #3FB27F |
| warning | #E5B13A |
| danger | #F0605D |
| info | #5AA9F0 |

Text tokens SHALL meet WCAG contrast of at least 4.5:1 against the surfaces they are used on. On the desktop the tokens SHALL be implemented as a Raven theme; on iPadOS as SwiftUI color assets.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: Theme switch
- **WHEN** the user switches from the dark theme to the light theme
- **THEN** every panel SHALL use the light token values, and the layout canvas SHALL keep its dark background and PDK layer colors

### Requirement: Layer colors come from the PDK, not the theme
The layout and 3D canvases SHALL draw layers with the colors, stipples and outlines of the PDK display styles (capabilities `display-style-files` and `hefesto-rendering`). Theme tokens SHALL NOT recolor layers. On the canvas:
- selection SHALL be drawn in white (solid outline, with a dashed bounding box)
- the box SHALL be drawn as capability `ui-windows-and-views` requirement "Box display" specifies (Magic's solid box in the display styles' highlight colors, white in Magic's styles); that requirement takes precedence over the reference mockups' dashed box

Neither SHALL use a hue that could be confused with a layer. The ember accent SHALL be used for actions, focus and the active tool, and SHALL NOT be used for canvas geometry.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: Box drawn as in Magic
- **GIVEN** the display styles of Magic's `mos` style file, where the solid-highlights style is white
- **WHEN** a box larger than 4 × 4 pixels is shown over the edit cell
- **THEN** it SHALL be drawn with solid white sides 2 pixels wide, not dashed

#### Scenario: Selected wire on an orange-colored layer
- **GIVEN** a PDK whose metal3 display style is orange
- **WHEN** a metal3 wire is selected
- **THEN** the selection SHALL be drawn as a white outline, distinguishable from the layer color

### Requirement: Status is never color-only
Every status indicator SHALL pair a shape or a text label with its color. The shapes are: ● success, ▲ warning, ■ error, ◆ info. Examples of status indicators are DRC and LVS chips, sign-off items and diagnostics. Colors that must be told apart SHALL also differ in lightness.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: Color-blind user
- **WHEN** a user with red-green color blindness views the sign-off panel
- **THEN** each item's state SHALL be identifiable by its symbol and text alone

### Requirement: Typography
The UI font SHALL be Inter. JetBrains Mono SHALL be used for coordinates, dimensions, net and cell names, commands and code. The type scale SHALL be:

| Style | Size | Weight |
|---|---|---|
| display | 28 | 700 |
| title | 20 | 600 |
| heading | 15 | 600 |
| body | 13 | 400 |
| label | 11 | 600, uppercase, tracked 0.06–0.08 em |
| mono | 12 | 400 |

On iPadOS, sizes SHALL scale with Dynamic Type. Both fonts are bundled, under their open font licenses (capability `hefesto-licensing`).

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: Coordinates in mono
- **WHEN** the status bar shows the cursor position
- **THEN** it SHALL use JetBrains Mono 11–12 px, so digits align as they change

### Requirement: Spacing, radius, elevation, motion and density
The design SHALL use these values:
- **Spacing:** a 4 px scale (4, 8, 12, 16, 24, 32).
- **Radius:** 4 px for desktop controls, 8 px for panels and cards, 12 px for iPad sheets and controls.
- **Elevation:**
  - panels: a 1 px border only
  - popovers and menus: a shadow of 0 8–12 px, 24–32 px blur, at 45–55% black
- **Motion:** 120 ms ease-out for hover and press, 200 ms for panel transitions. Motion SHALL be disabled when the system requests reduced motion.
- **Desktop density:** controls 28–32 px tall.
- **iPad density:** every touch target at least 44 × 44 pt; primary tool buttons 52 pt.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: Reduced motion
- **WHEN** the system's reduce-motion setting is on
- **THEN** panels SHALL appear without animated transitions

### Requirement: Icons
Icons SHALL be 1.6 px stroke drawings on a 24 px grid. They SHALL be filled only to show an active state. The core set SHALL include:
- box, select, paint, wire, label, measure, place device
- 3D, agent, undo
- zoom to fit, and the layer visibility and lock icons

Icon-only buttons SHALL carry an accessible label. Icons SHALL be shipped as SVG files. On the desktop they are drawn by Raven's SVG support at the screen's density; on iPadOS they are converted to SF-Symbol-style vector assets.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: Icon-only button accessibility
- **WHEN** a screen reader focuses the box-tool button
- **THEN** it SHALL announce "Box tool" and its pressed state

### Requirement: Screen patterns follow the reference mockups
The main screens SHALL follow the structure of the reference mockups:
- **Desktop layout editor:** tool rail, layer palette and hierarchy, canvas with rulers and minimap, inspector, command bar, status bar; with the editing states of the M2 boards: live tool rail with cursor glyphs, selection inspector (paint, label, port, instance, mixed), layer palette paint layer, visibility and lock, command bar completion and history popovers, undo/redo in the top bar, prompt dialogs, dockable and collapsible panels.
- **Color window:** Magic's color editor surface drawn on the canvas, framed by a caption, an index stepper, Load/Save and a read-only value strip.
- **Key-binding reference:** a modal sheet listing the macro tables by tool, with search, key capture and an inline conflict warning.
- **Schematic editor:** symbol library, schematic canvas, waveform dock, instance properties with back-annotated operating point.
- **DRC browser:** a docked DRC panel listing the root cell's errors grouped by why-text with counts and their rectangles, ignored rules, a style selector and Re-check; `#k of n` stepping with the error tiles and the box on the canvas; an inspector for the error area under the box; empty, running and stale states; a DRC status chip and status-bar segment (not run, checking, clean, N errors, off) and a `drc why` popover on the canvas. Measured versus required values, nets, suggested fix and the sign-off summary are later-milestone parts of this screen, shown disabled with their milestone.
- **Scripting and PDK screens:** a menu bar with Magic's wrapper menus (File, Edit, Cell, Window, Layers, Drc, Options) followed by the PDK's device menus, native on macOS and in the window on Linux, with PDK-inserted items marked; Place device on the tool rail and a non-modal PCell parameter dialog (new, edit, corrected-value and dependency states, native or Tcl generator shown); a consent prompt naming the script's origin and PDK version (Don't run, Run once, Trust this version), trusted versions in the PDK panel, a read-only state when declined, sandbox denials as diagnostics with file and line, and a long-running-script card with Interrupt; cell, library and technology managers as dock panels, a text helper, the layer palette in toolbar order with a reorder dialog; and the console as a multi-line Tcl console with stdout, stderr and errors kept apart and an Interrupt control.
- **3D inspection:** layer stack, isometric view with overlay legend, cross-section, camera presets.
- **Agent proposal review:** request and steps, visual diff, verification gates, alternatives, accept / accept some / reject.
- **iPad editor:** collapsible sidebar, full-bleed canvas, floating 52 pt tool palette, long-press action menu, floating modifier button.

Changes to these patterns SHALL be made in the reference mockups (`docs/design/mockups/`) first, and then reflected here.

The desktop window SHALL NOT be made smaller than 1024 × 640 logical pixels, and at every width down to that minimum every enabled control of the desktop layout editor SHALL stay visible and reachable. When the top bar does not fit, it SHALL hide, in this order and only as far as needed: the disabled actions of later milestones, the DRC and LVS status chips, and then the editor-view tabs.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: New screen
- **WHEN** a new major screen is designed
- **THEN** a board SHALL be added to the reference mockups (`docs/design/mockups/`), and a bullet to this requirement, before implementation starts

#### Scenario: Top bar at the minimum window size
- **GIVEN** the desktop window at 1024 × 640
- **WHEN** the top bar is laid out
- **THEN** the Open and PDKs buttons SHALL be fully visible, and the bar's content SHALL NOT be wider than the window
