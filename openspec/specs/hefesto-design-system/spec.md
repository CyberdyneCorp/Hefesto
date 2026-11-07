# Hefesto Design System

## Purpose
Defines Hefesto's visual design system, "forge" (decision P-23):
- design tokens: color, typography, spacing, radius, elevation and motion
- the icon style
- the component patterns shared by the desktop app (Raven themes) and the iPadOS app (SwiftUI)

Reference mockups of the main screens are kept as a design artifact: https://claude.ai/artifact/9fWfmyGAx1EYtzhmgqyeeK (Hefesto UI Mockups). The guiding rule is that UI chrome stays neutral, so PDK layer colors own the canvas.

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
- the box SHALL be drawn in white dashes

Neither SHALL use a hue that could be confused with a layer. The ember accent SHALL be used for actions, focus and the active tool, and SHALL NOT be used for canvas geometry.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

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
- **Desktop layout editor:** tool rail, layer palette and hierarchy, canvas with rulers and minimap, inspector, command bar, status bar.
- **Schematic editor:** symbol library, schematic canvas, waveform dock, instance properties with back-annotated operating point.
- **DRC browser:** violation list grouped by rule, zoomed error view with measured versus required values, rule detail with suggested fix, sign-off summary.
- **3D inspection:** layer stack, isometric view with overlay legend, cross-section, camera presets.
- **Agent proposal review:** request and steps, visual diff, verification gates, alternatives, accept / accept some / reject.
- **iPad editor:** collapsible sidebar, full-bleed canvas, floating 52 pt tool palette, long-press action menu, floating modifier button.

Changes to these patterns SHALL be made in the design artifact first, and then reflected here.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-23)

#### Scenario: New screen
- **WHEN** a new major screen is designed
- **THEN** a board SHALL be added to the design artifact, and a bullet to this requirement, before implementation starts
