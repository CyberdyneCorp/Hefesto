# Diagnostic codes: extflat area

Codes `EXTFLAT-<TOPIC>-<NNN>` emitted by `src/core/src/extflat/` (the `.ext`
read-and-flatten subset that `def write` calls: EFReadFile, EFFlatBuild with
`EF_FLATNODES | EF_NOFLATSUBCKT` and EFVisitNodes, pulled forward from M7 by
Open Question 1, decided (a), and built to the M7 specs ext-flattening-read,
ext-flattening-hierarchy and ext-flattening-names, which M7 extends;
`openspec/changes/m6-lef-def-oasis/design.md` D1/D2, D10). Conventions and the
renderings are described in [../codes.md](../codes.md).

The area's table is assembled by `src/core/src/extflat/codes.cpp` (owner:
INTEGRATION) from one fragment (`src/core/src/extflat/codes.inc`, topics
`EXTFLAT-READ-*`, owner EXTSUB). The rows are sorted while compiling
(`src/core/src/diag/sorted_codes.hpp`).

Every option and function the M6 scaffold declared is implemented; its stub
code `CORE-NOTIMPL-006` is retired ([../codes.md](../codes.md)).

## READ

Codes of the `.ext` reader and flattener (`src/core/src/extflat/`; capabilities ext-flattening-read, ext-flattening-hierarchy, ext-flattening-names; design D10). Owner: EXTSUB.

`extflat::read_flat` reads as `def write` does (EFReadFile with dosubckt
on, no `.res.ext` overlay, shorts `none`; EFFlatBuild with
`EF_FLATNODES | EF_NOFLATSUBCKT`). The read fails, and the caller writes no
nets, in the cases where EFReadFile returns FALSE: `EXTFLAT-READ-001`
(`Cannot read extract file <name>.ext`, the text Magic leaves as the Tcl
result) when the root or a used cell has no `.ext` file along the `-p`
path, the loaded cell's directory or the cell search path, and
`EXTFLAT-READ-003` when a `tech` record names another technology (Magic's
`Error: .ext file has different technology <t>` and `Load this technology
and repeat.`). Reading goes on after a failed cell, so its other messages
are still reported; the first failure is the result.

`EXTFLAT-READ-002` carries every efReadError message with Magic's
`<file>, line <n>: ` prefix (unrecognized keywords, short lines, zero
scale factors, too few area/perimeter values, resistance-class and version
mismatches, subscript errors of `merge`, `cap` and `resist` names, bad
`parameters` tokens, device records that do not parse, attributes of
unknown nodes). `EXTFLAT-READ-004` is Magic's `Warning:  .ext file style
<s> is not known in this technology!` (only when the caller lists the
technology's styles), `EXTFLAT-READ-005` `Warning: use <id> appears more
than once in def!`, `EXTFLAT-READ-006` the shorted-ports warning of an
`equiv` (none when one name is the other made unique with `_uq`), and
`EXTFLAT-READ-012` the two `Device ... overlaps ...` messages of a device
record at the place of an incompatible one. While flattening,
`EXTFLAT-READ-007` is Magic's `connect(1): no such node <n>`,
`connect(2): ...` and `kill: no such node <n>`, and `EXTFLAT-READ-008`
the lines of efFlatGlobError for a global name (from the project setting
"global nets and power names") split over unconnected nodes, which are
merged.

`EXTFLAT-READ-009`, `-010` and `-011` are Hefesto's own (design D21, D23
M6-S8; caps approved 2026-11-21). `-009` and `-010` are `native_only`;
`-011` is a warning every console shows, in both modes, so a dropped line
is never silent. Magic has no caps: a use cycle recurses until the
stack overflows, a line of more than 128 tokens overflows efReadLine's
`argv`, and a huge array, subscript range or hierarchy runs out of memory
or time. `EXTFLAT-READ-009` fails the read when a cap of
`extflat::ReadLimits` is passed (file size 1 GiB, logical line 16 MiB,
1 Mi cells, use depth 1000, 4 Mi flattened instances, 16 Mi flat names,
16 Mi merge-record elements), `EXTFLAT-READ-010` fails it on a use cycle,
and `EXTFLAT-READ-011` ignores a line of more than 128 tokens with a
warning.

### M7 additions (EXTFLAT, `openspec/changes/m7-extraction/design.md` D13)

`extflat::read` and `extflat::flatten` read and flatten for the netlisters
(EFReadFile with the netlister's dosubckt, `extresist` overlay, noscale and
isspice; EFFlatBuild with the efFlatNodes path, couplings and the visits;
EFFlatBuildOneLevel for hierarchical output). They reuse the codes above:
`EXTFLAT-READ-002` also carries `Error: expected L and W, got ...`,
`Error: expected resistance value, got ...`, `Bad terminal description for
Short device` and, with `-v`, `Warning: duplicate node name <n>`, `Node <n>
doesn't exist so creating it`, `Merged nodes <a> and <b>` and `Error:
subcap has unknown node <n>`; `EXTFLAT-READ-007` also carries `cap(1)`,
`cap(2)`, `resist(1)` and `resist(2)` lookups and the `kill` lookup of a
device terminal, and in the efFlatNodes path the `connect(1)`/`connect(2)`
warning of flat mode (none in one-level flattening). New codes:

- `EXTFLAT-READ-013`: efFlatNodes with `EF_WARNABSTRACT` (antennacheck)
  for each instance of a cell read with an `abstract` record, Magic's
  `Error:  Cell <name> was extracted as an abstract view.` The run goes on.
- `EXTFLAT-READ-014`: EFHierVisitDevs for a device of the definition being
  netlisted whose terminal is on a killed node; the device is still written.
- `EXTFLAT-READ-015`: `extflat::parse_common_args` (EFArgs) for an
  unrecognised flag, a flag without its value (`-<c> requires a following
  <what>`, `Bad argument <word>`) or `-h`; the text ends with EFArgs' usage.
- `EXTFLAT-READ-016`: EFArgs' messages that do not fail the command:
  `Warning: multiple input files specified; ignoring "<name>"` and the
  efSymAdd rejections of a `-s` symbol.

## Table

The table below is generated from the fragments:

```sh
hefesto-cli --diagnostic-codes=extflat
```

The unit test `each area's code table matches its document and prefix`
fails when the two disagree.

<!-- BEGIN DIAGNOSTIC CODES (generated by `hefesto-cli --diagnostic-codes=extflat`) -->
| Code | Default severity | Meaning |
|---|---|---|
| `EXTFLAT-READ-001` | error | An `.ext` file of the hierarchy cannot be found or read, so the read fails (`Cannot read extract file <name>.ext`). |
| `EXTFLAT-READ-002` | warning | A `.ext` read message (Magic's efReadError text, with its `<file>, line <n>: ` prefix). |
| `EXTFLAT-READ-003` | error | A `tech` record names another technology than the loaded one; the read fails. |
| `EXTFLAT-READ-004` | warning | A `style` record names an extraction style the technology does not have. |
| `EXTFLAT-READ-005` | warning | A `use` record repeats a use id of its cell; the later use replaces the earlier. |
| `EXTFLAT-READ-006` | warning | An `equiv` record joins two ports of one cell (`Ports "a" and "b" are electrically shorted.`). |
| `EXTFLAT-READ-007` | warning | A merge or kill names a node the flattened hierarchy does not have (`connect(1): no such node <name>`). |
| `EXTFLAT-READ-008` | info | A global node name is split over unconnected nodes, which are merged (`*** Global name <g> not fully connected:`). |
| `EXTFLAT-READ-009` | error | The `.ext` input passes a resource cap of the read (file size, line length, cells, depth, instances, names or connections); the read fails. |
| `EXTFLAT-READ-010` | error | The `use` records of the `.ext` files form a cycle; the read fails. |
| `EXTFLAT-READ-011` | warning | A `.ext` line has more than 128 tokens; the line is ignored with this warning, in both modes. |
| `EXTFLAT-READ-012` | warning | A `device` record overlaps an incompatible device at the same place and is ignored. |
| `EXTFLAT-READ-013` | warning | A cell of the flattened hierarchy was extracted as an abstract view (`Error:  Cell <name> was extracted as an abstract view.`). |
| `EXTFLAT-READ-014` | warning | A device of a hierarchical netlist is connected to a killed node and is written anyway (`Error:  Device at (<x> <y>) is connected to one or more eliminated nodes.`). |
| `EXTFLAT-READ-015` | error | A netlister argument is not recognised or lacks its value; the command prints its usage and fails. |
| `EXTFLAT-READ-016` | warning | A netlister argument is ignored (a second root cell, or a `-s` symbol that is malformed or already defined). |
<!-- END DIAGNOSTIC CODES -->
