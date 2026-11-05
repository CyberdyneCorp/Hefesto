# Extract Resistance (Lumped)

## Purpose

Defines the lumped resistance estimate written for every node of a `.ext` file, the per-resistance-class area and perimeter values written with every node and merge record, and the `resistclasses` header. Full R-C extraction (`.res.ext`, `extract do resistance`) is owned by capability `extresist-command`; via resistance (`contact` keyword) is consumed only there. Resistance classes are defined in `extract-tech-section`.

## Requirements

### Requirement: Per-class area and perimeter of a node

For each node and each resistance class i, the extractor SHALL accumulate `area_i` = sum of the (clipped) areas of the node's tiles whose type (residue for contacts) belongs to class i (split tiles: clipped area divided by 2, integer division), and `perim_i` = sum, over the same tiles, of the lengths of boundary segments (and, for split tiles, the integer diagonal length `(int)sqrt(w²+h²)`) whose neighbor type (residue) is not in class i. Space and types of no class count as "not in class i". Types without a class contribute to no class. These values SHALL be stored even when lumped resistance is disabled.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:5544-5582, extract/ExtBasic.c:5643-5645, extract/ExtTech.c:3669-3677)

#### Scenario: Wire with a via
- **GIVEN** classes 0 = {poly}, 1 = {metal1, m1c-residue metal1} and a metal1 wire (0,0)-(10,2) with no other material
- **WHEN** the cell is extracted
- **THEN** the node record ends with `0 0 20 24` (class 0 area 0 perimeter 0, class 1 area 20 perimeter 24)

### Requirement: Lumped resistance formula

When option `lumped` is on, after a node is complete, for each class i with `area_i > 0` and `perim_i > 0` the node resistance SHALL be increased by
Compatibility: exact

```
v = (float)( (int64)perim_i * perim_i − 16 * area_i )      -- computed in 64-bit integer, then rounded to single precision
s = (v < 0) ? 0 : sqrtf-equivalent of v in single precision (s = (float)sqrt((double)v))
fp = (float)perim_i
r  = (int)( ((fp + s) / (fp − s)) * (float)sheet_i )        -- single-precision arithmetic, truncation toward zero
```

where `sheet_i` is the class sheet resistance (mΩ/square). The model is a rectangle of the same area and perimeter, `R = L/W` squares. The running total SHALL be updated only if the new total is positive (saturating against integer overflow; a negative or wrapped sum leaves the previous value). When `lumped` is off, the node resistance SHALL be 0.

(Magic ref: extract/ExtBasic.c:623-670, extract/ExtBasic.c:5196-5200, extract/ExtBasic.c:5490-5493)

#### Scenario: Five-square wire
- **GIVEN** `resist metal1 80`, `rscale 1` and a metal1 wire (0,0)-(10,2)
- **WHEN** the cell is extracted with lumped resistance on
- **THEN** v = 576 − 320 = 256, s = 16, R = (40/8)·80 = 400 and the node record's R field is `400`

#### Scenario: Square
- **GIVEN** the same class and a metal1 square (0,0)-(4,4)
- **WHEN** the cell is extracted
- **THEN** v = 0 and R = 80

### Requirement: Resistance output rounding

The R field of `node` and `substrate` records SHALL be `(R_mΩ + rscale/2) / rscale` in C integer arithmetic (rscale/2 truncated; the quotient truncated toward zero), printed with `%d`.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:689, extract/ExtBasic.c:735-745)

#### Scenario: Default rscale rounds small values to zero
- **GIVEN** the five-square wire (400 mΩ) and default `rscale 1000`
- **WHEN** the cell is extracted
- **THEN** the R field is (400 + 500)/1000 = 0

### Requirement: Area and perimeter values in records

The `node`/`substrate` record SHALL list, for i = 0 .. N−1 (N = number of classes), `area_i` (`%lld`) and `perim_i` (`%d`) as pairs. Hierarchical `merge` records SHALL carry the per-class area and perimeter adjustments in the same layout (see `extract-hierarchy`). A cell with the `device` property SHALL write node resistances of 0 but keep area and perimeter values.
Compatibility: exact

(Magic ref: extract/ExtBasic.c:756-759, extract/ExtBasic.c:356-367)

#### Scenario: Two classes
- **GIVEN** classes `poly` (0) and `metal1` (1) and a node with a poly shape of area 12 perimeter 14 and a metal1 shape of area 20 perimeter 24 joined by a contact whose residues are poly and metal1, each 2×2
- **WHEN** the cell is extracted
- **THEN** the record lists the poly-class pair first and the metal1-class pair second, with the contact area counted in both classes

### Requirement: Resistance class header

The header line `resistclasses` SHALL list the sheet resistance of each class in class order, each preceded by one space, with `%d` formatting; with no classes the line is exactly `resistclasses`.
Compatibility: exact

(Magic ref: extract/ExtCell.c:589-592)

#### Scenario: Header line
- **GIVEN** `resist poly 7000` and `resist metal1 80`
- **WHEN** any cell is extracted
- **THEN** the header contains `resistclasses 7000 80`
