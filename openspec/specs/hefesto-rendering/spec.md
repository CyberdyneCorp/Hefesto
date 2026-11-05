# Hefesto Rendering

## Purpose
Defines Hefesto's GPU rendering for the 2D layout view. One internal rendering interface sits over two backends: Metal on macOS and iPadOS, and Vulkan on Linux. The look of the layout comes from Magic display style files, so PDK colors and stipples appear as they do in Magic. The 3D view uses the same interface (see capability `hefesto-3d-visualization`).

## Requirements

### Requirement: Rendering abstraction with Metal and Vulkan backends
All drawing SHALL go through one internal, API-neutral rendering interface. The interface SHALL provide:
- buffers and textures
- pipelines for filled, stippled, outlined, text and line primitives
- render passes
- frame presentation

The backends SHALL be Metal on macOS and iPadOS and Vulkan 1.2 or later on Linux. There SHALL be no OpenGL backend. Code outside the backend modules SHALL NOT call Metal or Vulkan directly.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-3)

#### Scenario: Backend selection
- **WHEN** Hefesto starts on Linux
- **THEN** the Vulkan backend SHALL be used, and the same view code SHALL draw through the Metal backend on macOS

### Requirement: Style-file-faithful 2D drawing
The 2D layout view SHALL draw every layer with the fill, stipple, outline, color and pale variant given by the display style files and the tech `styles` section. These are specified by capabilities `display-style-files` and `tech-styles-section`. The view SHALL also draw the following as specified by capability `ui-windows-and-views`:
- labels
- port markers
- subcell outlines and names
- grid
- box
- selection
- feedback
- DRC error styles

The default style variant SHALL be Magic's OpenGL dstyle/cmap variant, which uses true color with translucency. Other variants SHALL be selectable in settings. Stipple bit orientation SHALL follow Magic's OpenGL/Cairo driver.

Compatibility: behavioral

(Hefesto decision: docs/decisions/cross-cutting.md#X-5)

#### Scenario: PDK colors
- **GIVEN** the sky130A tech and its display styles
- **WHEN** a layout is displayed
- **THEN** each layer SHALL use the color and stipple pattern that Magic would use for that style

### Requirement: Large-layout performance
The 2D view SHALL keep interactive pan and zoom on large layouts, using level-of-detail culling, cached tessellation and incremental GPU buffer updates after edits. The targets are given in capability `hefesto-performance`.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-3)

#### Scenario: Incremental redraw after paint
- **WHEN** a single rectangle is painted
- **THEN** only GPU data for the affected region SHALL be updated, not the whole layout

### Requirement: High-DPI and color management
Rendering SHALL respect the display scale factor on every platform (Retina, HiDPI). It SHALL map the colors in style files as sRGB.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-3)

#### Scenario: Retina display
- **WHEN** Hefesto runs on a 2× display
- **THEN** outlines and text SHALL be drawn at native pixel resolution, not upscaled
