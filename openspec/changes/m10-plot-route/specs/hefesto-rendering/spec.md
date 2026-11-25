## ADDED Requirements

### Requirement: View export to SVG and PNG
Hefesto SHALL export the contents of a layout view as an SVG 1.2 document or a PNG image, on every platform and in headless sessions, without a GPU and without Cairo. The export SHALL draw what the 2D view draws for the same window state (requirement "Style-file-faithful 2D drawing"): layers with their fills, stipples and outlines in display-style order, labels, unexpanded-cell outlines with their names, and the grid and box only when asked. The export SHALL be computed from the same scene model the renderer draws, so the view and its export do not drift apart.
- **SVG.** The root element has `width` and `height` equal to the exported pixel size and a `viewBox` of the same size. Stipples are SVG patterns, one per stipple used. Layer geometry is written as merged rectangles and polygons per (layer, style), in display-style order. Text is SVG `text`. The document uses no scripting and no external references.
- **PNG.** 8-bit RGB (RGBA when a transparent background is asked), non-interlaced, deflate-compressed with the zlib library already in the build, at the view's pixel size or a requested size of at most 16 384 pixels per side.

Both outputs SHALL be byte-identical for the same inputs on Linux x86_64, Linux aarch64 and macOS arm64. `plot svg` (capability `plot-output`) and the image export of `reliability-visualization` use this export.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-3, docs/decisions/audit/DECISIONS.md#D-37, docs/decisions/audit/DECISIONS.md#D-38) (See Q-MISC-52.)

#### Scenario: Headless SVG export
- **GIVEN** a headless session showing a cell with metal1 and a label
- **WHEN** the view is exported as SVG at 512 × 484 pixels
- **THEN** the document's root has `width="512"`, `height="484"` and `viewBox="0 0 512 484"`, the metal1 geometry is filled with metal1's display colour and stipple, and the label's text is a `text` element

#### Scenario: PNG matches the view
- **GIVEN** a desktop layout window showing a cell
- **WHEN** the view is exported as PNG at the window's size
- **THEN** the image has the window's pixel size and each pixel inside the drawn layers has the colour of the topmost style drawn there
