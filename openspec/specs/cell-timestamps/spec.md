# Cell Timestamps

## Purpose

Defines the timestamp carried by every cell definition and by every instance record in a parent. The hierarchical design-rule checker uses these timestamps to detect children that were edited outside the context of their parents.

This spec covers:
- the value domain;
- when new timestamps are generated and how they propagate to ancestors;
- the timestamp comparisons made while cells are read;
- deferred mismatch processing;
- fixed, read-only and vendor-GDS timestamps.

The content-hash algorithm that produces vendor-GDS timestamps is owned by capability `gds-read`. The textual `timestamp` lines are defined in `cell-file-format`. The DRC re-check machinery is owned by the DRC capabilities.

## Requirements

### Requirement: Timestamp value domain

A cell timestamp SHALL be a signed 32-bit integer.
Compatibility: exact

- Generated stamps are the current wall-clock time in whole seconds since 1970-01-01T00:00:00Z, truncated to 32 bits (see Q-CELL-35).
- 0 means "unknown". A cell or instance with stamp 0 always counts as mismatched.
- The intended invariant is that a cell's stamp is at least the stamp of every descendant.
- A parent stores, for each child definition it uses, the child stamp it last saw. This value is written as the `timestamp` line of each use block, and it is the child definition's current stamp at the time the parent is written.
- A new cell definition starts with stamp 0.

(Magic ref: database/DBtimestmp.c:42-62, database/DBtimestmp.c:91, database/DBcellname.c:2019-2030, database/DBio.c:6276-6277)

#### Scenario: New cell has zero stamp
- **WHEN** a new empty cell definition is created
- **THEN** its timestamp SHALL be 0

### Requirement: Marking cells for a new stamp

Hefesto SHALL mark a cell as needing a new stamp whenever its real content changes. A pending stamp is independent of the "modified" state used for saving.
Compatibility: behavioral

**Changes that mark a cell.**
- painting or erasing;
- adding, changing or removing labels;
- placing, moving, deleting or re-identifying instances;
- copying into the cell;
- plowing;
- renaming a child definition, which marks every parent of that child.

**Changes that do not mark a cell.** Recomputing a cell's bounding box marks it only if the box actually changed.

**Clearing the mark.** The mark is cleared when the cell is read from a file. A file stamp of 0 sets it again (see "Timestamps on cell read").

(Magic ref: database/DBpaint2.c:65, database/DBlabel.c:228, database/DBcell.c:209, database/DBcell.c:279, database/DBcellbox.c:184-186, database/DBcellname.c:2140-2142, database/DBio.c:1004)

#### Scenario: Paint marks the cell
- **GIVEN** a freshly read cell `a`
- **WHEN** metal1 is painted in `a`
- **THEN** `a` SHALL be both modified and pending a new stamp

### Requirement: Generating new stamps

When stamps are updated, Hefesto SHALL first process pending mismatches (see "Deferred mismatch processing") and then take the current time t once for the whole update.
Compatibility: behavioral

**Scope.**
- The update is limited to a single cell when one is given.
- Otherwise it covers every cell that has a pending new stamp.
- When a single cell is given and it is read-only or has a fixed stamp, only its pending mark is cleared and nothing else happens.

**Per-cell procedure.** Apply this procedure to each cell X in scope, and then recursively to every parent definition of every instance of X:

1. If X's stamp already equals t, stop. Ancestors are not visited from X.
2. If X is read-only, clear its pending mark and stop. Ancestors are not visited.
3. If X has a fixed stamp, clear its pending mark but keep its stamp, then continue with step 6.
4. If X is a vendor-GDS (read-only GDS) cell, clear its pending mark and compute its content stamp from the referenced GDS file (see capability `gds-read`). If the result is 0 or equals X's stamp, stop without visiting ancestors. Otherwise set X's stamp to the result and continue with step 6.
5. Otherwise set X's stamp to t and clear its pending mark.
6. For every instance of X that has a parent P, flag P as having a changed child stamp, then apply this procedure to P.

**When updates run.** Stamps are updated:
- for the saved cell before a save (the `save` command runs the update and then the cell save, which runs it again);
- before saving the selection as a cell;
- before writing all cells (for the named cells, or for all cells);
- before hierarchical extraction.

(Magic ref: database/DBtimestmp.c:223-341, commands/CmdRS.c:220, commands/CmdRS.c:1236, commands/CmdSubrs.c:754, commands/CmdTZ.c:2229-2242, extract/ExtMain.c:934-937)

#### Scenario: Propagation to ancestors
- **GIVEN** a hierarchy `top` → `mid` → `leaf`, where all stamps are 100 and only `leaf` is pending
- **WHEN** stamps are updated at time t = 200
- **THEN** `leaf`, `mid` and `top` SHALL all have stamp 200
- **AND** `mid` and `top` SHALL be flagged as having changed child stamps

#### Scenario: Read-only child does not propagate
- **GIVEN** a read-only cell `pdk` that is pending a new stamp
- **WHEN** stamps are updated
- **THEN** the stamp of `pdk` SHALL be unchanged, its pending mark SHALL be cleared, and its parents SHALL NOT be re-stamped through it

#### Scenario: Fixed-stamp child still propagates
- **GIVEN** cell `lefcell` has a fixed stamp of 7 and is pending, and its parent `top` has stamp 100
- **WHEN** stamps are updated at t = 300
- **THEN** `lefcell` SHALL keep stamp 7 and `top` SHALL get stamp 300

### Requirement: Fixed timestamps

A cell MAY carry a fixed-stamp attribute. Hefesto SHALL never change a fixed stamp during stamp updates.
Compatibility: behavioral

**Where the attribute comes from.** It is set when:
- a cell is created by LEF macro import;
- a GDS structure is read while an explicit non-zero GDS date stamp is configured (see capability `gds-read`).

**Persistence.** The attribute is not written to `.mag` files. A cell re-read from a `.mag` file has an ordinary stamp.

**Setting a stamp explicitly.** Setting a cell's stamp explicitly with the `cellname timestamp` command:
- assigns the value;
- clears the pending mark when the value differs from the current one;
- does not set the fixed-stamp attribute, even though Magic's code suggests that intent (see Q-CELL-36).

**GDS timestamps.** GDS reads that do not use a configured date stamp take each structure's creation time from the GDS file (see capability `gds-read`).

(Magic ref: lef/lefRead.c:2163, calma/CalmaRdcl.c:500-510, commands/CmdCD.c:1546-1566, database/database.h.in:424-430)

#### Scenario: Explicit stamp is not fixed
- **GIVEN** cell `a`, which is pending a new stamp
- **WHEN** its stamp is set explicitly to 5 and it is then modified and saved
- **THEN** the saved file SHALL carry the save time, not 5

### Requirement: Timestamps on cell read

When a cell C is read from a file with header stamp S (0 if absent), Hefesto SHALL compare S with C's current stamp E. E is the stamp recorded by the parent that created C's placeholder, or 0.
Compatibility: behavioral

1. **Mismatch.** If S ≠ E or S = 0, and C has at least one instance inside a parent cell (not only as a window root):
   - record a mismatch for C, with C's current bounding box as the old area;
   - flag every parent of C as having a changed child stamp.
2. **Assign.** Set C's stamp to S.
3. **Zero stamp.** If S = 0:
   - Magic prints `"<cell>" has a zero timestamp; it should be written out` and `    to establish a correct timestamp.`;
   - C is flagged as having a changed child stamp and as pending a new stamp.

(Magic ref: database/DBio.c:1015-1049)

#### Scenario: Child edited out of context
- **GIVEN** parent `top` records child `inv` with use timestamp 100, and `inv.mag` now has header `timestamp 150`
- **WHEN** `inv` is read as a child of `top`
- **THEN** a mismatch SHALL be recorded for `inv`, `top` SHALL be flagged as having a changed child stamp, and `inv`'s stamp SHALL become 150

#### Scenario: Root cell is never mismatched
- **GIVEN** cell `a` with header `timestamp 150` is loaded as a top-level cell only
- **WHEN** it is read
- **THEN** no mismatch SHALL be recorded

### Requirement: Timestamps on instance read

For each use block that is read with child stamp s (0 if absent) for child definition D, Hefesto SHALL apply the following.
Compatibility: behavioral

**Placeholder creation.** If D did not exist before this block, its placeholder takes stamp s.

**Mismatch.** After the instance is placed, if s ≠ D's stamp or s = 0:
- record a mismatch for D, with the block's box as the old area;
- if D has not been read yet, set D's stamp to 0, which forces a mismatch until D is read;
- otherwise, also record a mismatch for D, with D's current bounding box as the old area.

(Magic ref: database/DBio.c:2543-2547, database/DBio.c:3040-3060)

#### Scenario: Two parents disagree
- **GIVEN** parent `p1` creates placeholder `inv` with use timestamp 100, and `inv` has not been read
- **WHEN** parent `p2`, with `use inv` and timestamp 120, is read
- **THEN** a mismatch SHALL be recorded for `inv` and `inv`'s stamp SHALL become 0

### Requirement: Deferred mismatch processing

Hefesto SHALL process recorded mismatches only at safe points. The safe points are:
- after each interactive command;
- before stamp updates;
- before DRC continuous checking;
- before extraction;
- before LEF and CIF/GDS output.

**Processing.** When at least one mismatch is pending, Magic prints `Processing timestamp mismatches.`. Then, for each recorded mismatch (cell D, old area A), most recently recorded first, with each cell processed at most once:

1. If D is not available, read it, ignoring the technology and following its dereference flag.
2. Force D's bounding box to be recomputed and propagated to all of its instances.
3. For each instance U of D inside a parent P, queue a subcell re-check in P over two areas:
   - the area that A occupied in P, taking U's whole array extent (the first and last array elements) through U's transform;
   - U's current bounding box in P.

**Report.** After processing, Magic prints `Timestamp mismatches found in these cells: ` followed by the processed cell names, separated by `, ` and listed in reverse order of processing, then `.`. If any re-check was queued, the windows are redrawn.

(Magic ref: database/DBtimestmp.c:113-205, database/DBtimestmp.c:363-395, dbwind/DBWprocs.c:1007-1010, drc/DRCcontin.c:558, extract/ExtMain.c:376)

#### Scenario: Mismatch triggers subcell re-check
- **GIVEN** child `inv`, whose recorded box in `top` was (0,0)-(8,12) and whose real bounding box is (0,0)-(10,12), with an instance at the identity transform
- **WHEN** mismatches are processed
- **THEN** a subcell DRC re-check SHALL be queued in `top` over (0,0)-(8,12) and over (0,0)-(10,12)

### Requirement: Timestamps in written files

When writing a cell, Hefesto SHALL write the cell's current stamp in the header and, for each instance, the child definition's current stamp in its use block.
Compatibility: exact

**Pending warning.** If the cell is still pending a new stamp when it is written, Magic prints `Magic error: writing out-of-date timestamp for <cell>.`. Writing a crash backup or archive suppresses this warning for that write.

**Flags after writing.**
- A successful `.mag` save clears the cell's changed-child-stamp flag.
- Backup and archive writes change no stamps or flags.

(Magic ref: database/DBio.c:4721-4735, database/DBio.c:6276-6277, database/DBio.c:6528-6531)

#### Scenario: Save after edit writes the new stamp
- **GIVEN** cell `a` with stamp 100 is modified and saved at time 250
- **WHEN** the file is inspected
- **THEN** its header SHALL contain `timestamp 250`
