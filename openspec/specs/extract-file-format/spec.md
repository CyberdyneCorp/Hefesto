# Extract File Format

## Purpose

Defines the `.ext` file written by the extractor for each cell: the complete grammar of every record the extractor writes, the exact number formatting, the order of records, and an annotated example. The meaning and computation of each value are defined in `extract-nodes-and-labels`, `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-hierarchy` and `extract-substrate`. Records written only by full R-C extraction (`rnode`, `resist`, `killnode` in `.res.ext`) are owned by capability `extresist-output`; reading `.ext` files for netlisting is owned by the `netlist-*` capabilities, and only the reader rules that constrain the writer are restated here. Where the file is written is specified in `extract-commands`.

## Requirements

### Requirement: Lexical conventions

A `.ext` file SHALL be ASCII text, one record per line terminated by a single `\n`, fields separated by single spaces, with no leading or trailing whitespace and no comments. Node names, port names, equiv names and device terminal names SHALL be enclosed in double quotes with no escaping (names never contain `"` or whitespace); the node name in `attr` records, instance identifiers, cell names, type names and model names SHALL be written unquoted.
Compatibility: exact

Number formats SHALL be those of C `printf`: integers `%d`; areas `%lld`; capacitances in `node`, `cap`, `merge` records `%lg`; device values (`devres`/`devcap` with model `None`), `c` and `r` parameters and scale/`%g` fields `%g`. `%g`/`%lg` SHALL use 6 significant digits, drop trailing zeros and the decimal point when integral, and use exponent form `d.ddddde±XX` when the exponent is < −4 or ≥ 6.

(Magic ref: extract/ExtBasic.c:681-860, extract/ExtCell.c:552-675)

#### Scenario: Capacitance formatting
- **GIVEN** node capacitances 1760, 0.000012345 and 1234567 (after division by `cscale`)
- **WHEN** node records are written
- **THEN** the fields read `1760`, `1.2345e-05` and `1.23457e+06`

### Requirement: Record grammar

The writer SHALL produce only lines conforming to this grammar.
Compatibility: exact

```ebnf
ext_file      = header , { use_rec } , { cellparam_rec } , { devparam_rec } ,
                [ "abstract" , NL ] , { port_rec } , { node_group } ,
                { cap_rec } , { device_line } ,
                { cap_rec } , { merge_group } , { cap_rec } , { merge_group } ,
                { distance_rec } ;
header        = "timestamp" , SP , INT , NL ,
                "version" , SP , TOKEN , NL ,
                "tech" , SP , TOKEN , NL ,
                "style" , SP , TOKEN , NL ,
                "scale" , SP , INT , SP , INT , SP , GREAL , NL ,      (* rscale cscale units-per-lambda *)
                "resistclasses" , { SP , INT } , NL ,
                [ "parameters" , SP , ":" , CELLNAME , SP , TEXT , NL ] ;   (* cell property "parameter" *)
use_rec       = "use" , SP , CELLNAME , SP , USEID , [ arr , arr ] , 6 * ( SP , INT ) , NL ;
arr           = "[" , INT , ":" , INT , ":" , INT , "]" ;            (* lo:hi:sep ; x then y *)
cellparam_rec = "parameters" , SP , ":" , CELLNAME , SP , TEXT , NL ;   (* "device primitive ..." property *)
devparam_rec  = "parameters" , SP , MODEL , { SP , pdecl } , NL ;
pdecl         = KEY , "=" , NAME , [ "*" , GREAL | OFFSET ] ;       (* OFFSET = %+d *)
port_rec      = "port" , SP , QNAME , SP , INT , 4 * ( SP , INT ) , SP , TYPE , NL ;
node_group    = ( node_rec | subs_rec ) , { attr_rec } , { equiv_rec } ;
node_rec      = "node" , SP , QNAME , SP , INT , SP , GREAL , SP , INT , SP , INT , SP , TYPE , { SP , INT , SP , INT } , NL ;
subs_rec      = "substrate" , SP , QNAME , SP , INT , SP , "0" , SP , INT , SP , INT , SP , TYPE , { SP , INT , SP , INT } , NL ;
attr_rec      = "attr" , SP , NAME , 4 * ( SP , INT ) , SP , TYPE , SP , QTEXT , NL ;
equiv_rec     = "equiv" , SP , QNAME , SP , QNAME , NL ;
cap_rec       = "cap" , SP , QNAME , SP , QNAME , SP , GREAL , NL ;
device_line   = fet_rec | dev_rec | "primitive" , NL | propdev_rec ;
fet_rec       = "fet" , SP , MODEL , 4 * ( SP , INT ) , SP , INT , SP , INT , SP , QNAME , gate , { term } , NL ;
dev_rec       = "device" , SP , CLASS , SP , MODEL , 4 * ( SP , INT ) , dims , { SP , param } , [ SP , QNAME ] , gate , { term } , NL ;
dims          = [ SP , INT , SP , INT ]                              (* L W, or W area/W, or L W of resistor *)
              | SP , GREAL ;                                         (* ohms (devres None) or fF (devcap None) *)
param         = KEY , "=" , [ VALUE ] | LABELTEXT ;                  (* LABELTEXT contains "=" *)
gate          = SP , QNAME , SP , INT , attrs_or_zero ;
term          = SP , QNAME , SP , INT , ( attrlist , [ "," , INT , "," , INT ]
                                        | SP , INT , "," , INT
                                        | SP , "0" ) ;
attrs_or_zero = attrlist | SP , "0" ;
attrlist      = SP , QTEXT , { "," , QTEXT } ;
propdev_rec   = ( "fet" | "device" , SP , CLASS ) , SP , MODEL , 4 * ( SP , INT ) , [ SP , INT , SP , INT ] , { SP , TOKEN } , NL ;
merge_group   = merge_first , { merge_more } , { connect_rec } ;
merge_first   = "merge" , SP , QNAME , SP , QNAME , SP , GREAL , { SP , INT , SP , INT } , NL ;
merge_more    = "merge" , SP , QNAME , SP , QNAME , NL ;
connect_rec   = "connect" , 4 * ( SP , INT ) , SP , TYPE , SP , QNAME , SP , QNAME , NL ;   (* extresist mode only *)
distance_rec  = "distance" , SP , NAME , SP , NAME , SP , INT , SP , INT , NL ;
CLASS         = "fet" | "mosfet" | "asymmetric" | "bjt" | "devres" | "devcap" | "devcaprev"
              | "vsource" | "diode" | "pdiode" | "ndiode" | "subckt" | "rsubckt"
              | "msubckt" | "csubckt" | "dsubckt" | "veriloga" ;
QNAME         = '"' , NAME , '"' ;
QTEXT         = '"' , { CHAR } , '"' ;
```

A `merge_group` with values is followed by its value-less continuation merges; `connect` records appear only with full R-C extraction enabled, after the merges of the node they belong to.

(Magic ref: extract/ExtCell.c:552-675, extract/ExtBasic.c:216-620, extract/ExtBasic.c:2282-3245, extract/ExtHier.c:1111-1205, extract/ExtLength.c:303-305, extflat/extparse.h:41-66)

#### Scenario: Array use record
- **GIVEN** a 4×2 array use `A` of cell `bit` with x indices 0..3 pitch 10, y indices 0..1 pitch 20 and identity transform
- **WHEN** the parent is extracted
- **THEN** the record `use bit A[0:3:10][0:1:20] 1 0 0 0 1 0` is written

### Requirement: Header records

The header SHALL be, in order: `timestamp <T>` with T the cell's timestamp (the integer timestamp stored with the cell in its `.mag` file), `version <V>` with V the extractor format version string (Magic writes its major.minor version, `8.3`; see Q-EXTRACT-10), `tech <technology name>`, `style <current extract style name>`, `scale <rscale> <cscale> <units-per-lambda>` (the last with `%g`, already scaled to internal units, see `extract-tech-section`), and `resistclasses` with the class values.
Compatibility: exact

(Magic ref: extract/ExtCell.c:552-605)

#### Scenario: Header of a default style
- **GIVEN** technology `scmos`, style `default`, no `cscale`/`rscale`/`lambda` lines, a 1:1 grid and `resist metal1 80`
- **WHEN** a cell with timestamp 1700000000 is extracted
- **THEN** the file begins `timestamp 1700000000`, `version 8.3`, `tech scmos`, `style default`, `scale 1000 1000 100`, `resistclasses 80`

### Requirement: Use records

For every subcell use of the cell whose definition is not "don't use", in the cell's subcell enumeration order, the writer SHALL output `use <cellname> <useid>` followed, for an array (more than one element in x or y), immediately (no space) by `[xlo:xhi:xsep][ylo:yhi:ysep]`, then the six transform coefficients `a b c d e f` of the use (x' = a·x + b·y + c, y' = d·x + e·y + f) as integers.
Compatibility: exact (content); behavioral (order, see Q-EXTRACT-5)

(Magic ref: extract/ExtCell.c:640-675)

#### Scenario: Rotated instance
- **GIVEN** a use `I3` of cell `buf` rotated 90° and placed at (100,50)
- **WHEN** the parent is extracted
- **THEN** the record is `use buf I3 0 -1 100 1 0 50` (transform coefficients of the 90° rotation)

### Requirement: Record order

Within a file, records SHALL appear in this order: header; use records; the `parameters :<cell>` record of a `device primitive` property; device `parameters` records; `abstract` (when the cell has `LEFview`); all `port` records; for each node in node order its `node`/`substrate` record followed by its `attr` and then its `equiv` records; flat `cap` records (unless the cell has the `device` property); device records (or the single property-device or `primitive` line); hierarchical subcell `cap` records; subcell `merge` groups; array `cap` records; array `merge` groups; `distance` records. Within a record class, the order follows decision X-1 (capability `hefesto-output-determinism`, Q-EXTRACT-7):
- Subcell and array `merge` groups come from Magic string hash tables keyed by node names. Hefesto SHALL reproduce that iteration order exactly, and golden comparison SHALL be byte for byte.
- `cap` records, and the order of the two names within each, come from tables keyed by node memory addresses. Hefesto SHALL write them in the deterministic order of capability `hefesto-output-determinism`, and golden comparison SHALL canonicalize that order.

Compatibility: exact (class order and `merge` order); behavioral (order within `cap` classes)

(Magic ref: extract/ExtBasic.c:216-620, extract/ExtCell.c:501-547, extract/ExtHier.c:1111-1180, extract/ExtSubtree.c:223, extract/ExtArray.c:166)

#### Scenario: Ports precede nodes
- **GIVEN** a cell with two ports and three nodes
- **WHEN** it is extracted
- **THEN** both `port` lines appear before the first `node` line

### Requirement: Reader constraints honored by the writer

The writer SHALL keep each record within the minimum field counts the reader requires (`node` ≥ 7 fields, `substrate` ≥ 3, `cap` 4, `merge` ≥ 3, `equiv` 3, `port` 8, `attr` 8, `use` 9, `device` ≥ 11 for mosfet-like classes, `fet` ≥ 12, `distance` 4, `scale` 4) and SHALL write node and merge area/perimeter lists with exactly one pair per resistance class. A reader treats a device whose model is `Short` as an `equiv` between two of its terminal nodes rather than as a device (see the `netlist-*` capabilities); the extractor SHALL still write `Short` devices as ordinary device records.
Compatibility: exact

(Magic ref: extflat/extparse.h:41-66, extflat/EFread.c:307-470)

#### Scenario: Version mismatch warning in reader
- **GIVEN** a `.ext` file with `version 8.2`
- **WHEN** a Magic-compatible reader reads it
- **THEN** it warns `Cell was extracted using version 8.2 of the extractor.` and continues

### Requirement: Annotated example

The following file SHALL be accepted as a complete, well-formed example of the format (cscale 1000: capacitances in fF; rscale 1000; one resistance class `metal1`).
Compatibility: informative

```
timestamp 1730800000                     # cell timestamp
version 8.3                              # extractor version string
tech scmos                               # technology name
style default                            # extract style
scale 1000 1000 100                      # rscale cscale units-per-lambda
resistclasses 80                         # one class: metal1 at 80 mOhm/sq
use nand2 x1 1 0 20 0 1 0                # instance x1 of nand2 translated by (20,0)
parameters nfet_01v8 l=l w=w             # parameter keys of model nfet_01v8
port "OUT" 1 40 2 42 4 metal1            # port label text, number, rect, type
node "OUT" 0 1.76 40 2 metal1 40 28      # name R C x y type area perim
attr OUT 2 2 2 2 metal1 "cap=10f"        # node attribute label (trailing @ dropped)
equiv "OUT" "out_int"                    # alias
node "IN" 0 0.52 0 -2 metal1 12 14
substrate "VSUB" 0 0 -1073741817 -1073741817 space 0 0
cap "OUT" "IN" 0.2                       # coupling 0.2 fF
device msubckt nfet_01v8 0 0 1 1 l=2 w=4 "VSUB" "IN" 4 0 "VSUB" 4 12,14 "OUT" 4 12,14
cap "x1/A" "OUT" 0.015                   # hierarchical coupling correction
merge "x1/A" "IN" -0.52 -12 -14          # connection with C, area, perimeter adjustments
merge "x1/VSUB" "VSUB" 0 0 0             # child substrate merged into parent substrate
```

(Magic ref: extract/ExtCell.c:552-675, extract/ExtBasic.c:681-860)

#### Scenario: Example is well-formed
- **GIVEN** the example above
- **WHEN** it is checked against the record grammar
- **THEN** every line (comments removed) matches a production of the grammar
