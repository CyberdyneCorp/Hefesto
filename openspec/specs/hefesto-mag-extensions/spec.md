# Hefesto .mag Extensions

## Purpose
Defines how Hefesto stores data that Magic does not support in Magic's file formats without breaking Magic's ability to read them. Such data includes 3D view settings, annotations, diff baselines and project metadata. The rule is to use only constructs that Magic already reads and preserves: properties under a reserved name prefix.

## Requirements

### Requirement: Reserved property namespace
Hefesto-specific per-cell data SHALL be stored as `string` properties whose names begin with `HEFESTO_`, using the `<< properties >>` syntax of capability `cell-file-format`. Hefesto SHALL NOT add new section keywords or record types to `.mag` files.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Magic reads Hefesto file
- **GIVEN** a cell saved by Hefesto with a `HEFESTO_view3d` property
- **WHEN** Magic 8.3.694 loads and saves the cell
- **THEN** Magic SHALL read it without error, and the property SHALL be preserved verbatim

### Requirement: Byte-compatibility when no extensions are used
A cell that contains no Hefesto-specific data SHALL be saved byte-identical to Magic's output, as required by capability `cell-file-format` and the ordering rules of `hefesto-output-determinism`.

Compatibility: exact

(Hefesto decision: docs/decisions/cross-cutting.md#X-1)

#### Scenario: Round-trip a PDK cell
- **WHEN** a sky130 standard cell is opened in Hefesto and saved without edits
- **THEN** the file SHALL be byte-identical to the original Magic-written file

### Requirement: Extension values are versioned
Each `HEFESTO_*` property value SHALL begin with a schema version token, for example `v1:`. Readers SHALL ignore, but preserve, properties whose version is newer than they understand.

Compatibility: native

(Hefesto decision: docs/decisions/product.md#P-8)

#### Scenario: Newer extension
- **WHEN** an older Hefesto opens a cell whose `HEFESTO_annotations` property has value `v3:...`
- **THEN** it SHALL keep the property unchanged on save and SHALL show an info diagnostic
