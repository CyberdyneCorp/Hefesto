## MODIFIED Requirements

### Requirement: Touch and platform parity
All reliability views SHALL be available on Linux, macOS and iPadOS:
- **iPadOS:** a tap on a colored item shows its readout, and a long press opens its cross-probe menu.
- **Desktop:** the same actions are on hover and the context menu.

Image export of the current heat map (PNG, at the view's resolution or a chosen size) SHALL go through the view export of capability `hefesto-rendering` (requirement "View export to SVG and PNG"), and SHALL include the legend.

Compatibility: native

(Hefesto decisions: docs/decisions/product.md#P-21, docs/decisions/product.md#P-13, docs/decisions/audit/DECISIONS.md#D-37)

#### Scenario: Long press on iPad
- **WHEN** the user long-presses a red via in the 2D heat map on iPad
- **THEN** a menu SHALL offer "Show current", "Show in schematic", "Open in browser" and "Show in 3D"
