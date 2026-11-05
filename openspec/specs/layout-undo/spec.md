# Layout Undo

## Purpose

Specifies the undo/redo model: the event log, how events are grouped into one undo step per user command, what database and editor changes are recorded (and what is not), how undo and redo replay events, the history limit, how a new edit discards the redo tail, and when recording is suspended. The `undo`/`redo` commands themselves (argument syntax, `undo print`, `undo enable|disable`) belong to `cmd-*`.

## Requirements

### Requirement: Event log and command grouping

The system SHALL record reversible changes as events in a single linear log and SHALL group all events produced while executing one user command (typed command, key macro or button action) into one undo step, closing the step after the command completes only if at least one event was recorded.
Compatibility: behavioral

- A step boundary is inserted after each command dispatched from a layout window, after each command dispatched to other window clients that record events, and after compound internal operations that explicitly end a step (e.g. the "erase cursor"-style edits).
- A command that records no event creates no step; `undo` then undoes the previous step.
- Steps are undone and redone as units; partial undo of a step is not possible.
- When the log is played back, events of a step are replayed backwards (undo) or forwards (redo) in recording order.

(Magic ref: utils/undo.c:58-59,77-176,465-548, windows/windClient.c:640-660, dbwind/DBWprocs.c:990-1009, commands/CmdLQ.c:1050-1060)

#### Scenario: One command, one step
- **GIVEN** a command that paints `metal1` at two rectangles and adds a label
- **WHEN** `undo` is executed once
- **THEN** both rectangles and the label SHALL be removed together

#### Scenario: Command without events
- **GIVEN** step S1 = paint, followed by a command that only prints information
- **WHEN** `undo` is executed
- **THEN** step S1 SHALL be undone

### Requirement: Recorded event kinds

The system SHALL record undo events for at least: paint-type changes of plane areas (old type, new type, rectangle, plane, including split tiles), split-tile fracture and join operations, label creation and deletion (full label contents including port data), placement and deletion of cell uses (full use state: child, parent, identifier, transform, array, flags, bounding boxes, expansion state), instance identifier changes, lock/unlock of uses, changes of the cell being edited, changes of the box, selection changes, and the editor-specific clients (wiring, netlist menu, color map).
Compatibility: behavioral

- Moving, rotating or re-arraying a use is recorded as deletion of the old use followed by placement of the new use.
- An identifier change is recorded as a "clear id" event followed by a "set id" event.
- Label attribute changes (justification, layer migration, size adjustment) are recorded as an erase of the old label followed by a put of the new label.
- Paint events are attributed to a cell; when consecutive events concern a different cell, "close cell"/"open cell" events are interleaved so replay applies each event to the right cell.

(Magic ref: database/DBundo.c:44-188,456-530,620-690,908-940, dbwind/DBWundo.c:100-115,247-275, select/selUndo.c:130-150, wiring/wireUndo.c:85-90, netmenu/NMundo.c:225-235, cmwind/CMWundo.c:80-88)

#### Scenario: Undo a move
- **GIVEN** use `inv_0` at `(0,0)`
- **WHEN** it is moved to `(100,0)` and `undo` is executed
- **THEN** `inv_0` SHALL be back at `(0,0)` with the same identifier

#### Scenario: Undo restores the box
- **GIVEN** the box at `(0,0)-(10,10)`
- **WHEN** the box is moved to `(20,20)-(30,30)` and `undo` is executed
- **THEN** the box SHALL be back at `(0,0)-(10,10)`

### Requirement: Non-recorded operations

The system SHALL NOT record undo events for: reading cells from any file format (`.mag`, GDS, CIF, LEF, DEF), property changes, cell definition rename and delete, flattening (both into a new cell and in place), bounding-box recomputation and propagation, split-tile merging, and internal scratch-cell operations (DRC, extraction, mask generation, routing searches).
Compatibility: behavioral

- Recording is suspended with a nesting counter: nested suspensions require the same number of resumptions.
- Deleting the cell whose events are being recorded clears the whole history.
- Flattening in place deletes the use without an undo record, so the deletion cannot be undone (see Q-MODEL-28).

(Magic ref: utils/undo.c:408-436, database/DBio.c:838-841, calma/CalmaRead.c:211, database/DBcellname.c:167-173,326-357, commands/CmdFI.c:2116-2245, database/DBcellbox.c:705-735, database/DBpaint.c:1018-1035, database/DBundo.c:197-206)

#### Scenario: Reading a cell is not undoable
- **GIVEN** an empty session
- **WHEN** a cell is loaded from disk and `undo` is executed
- **THEN** the cell SHALL remain loaded with its contents unchanged

### Requirement: Undo and redo semantics

Undo SHALL revert the most recent not-yet-undone step and move the current position back one step; redo SHALL re-apply the next undone step; recording any new event after one or more undos SHALL permanently discard all undone steps.
Compatibility: behavioral

- `undo n` / `redo n` repeat up to n steps and stop at the ends of the log.
- Replaying paint applies the inverse table operation over the recorded rectangle (erase new type, paint old type) without image resolution; after a replay the affected cells' bounding boxes are recomputed, the changed area is redisplayed and re-queued for DRC.
- Replay itself records nothing.
- Undo/redo do not by themselves set or clear the modified flag of the affected cells for paint events and label removals (see Q-MODEL-12); replays that re-create a label, or place or delete a use, mark the cell modified as normal edits do.

(Magic ref: utils/undo.c:568-694,857-904, database/DBundo.c:258-420,545-600,687-830,989-1000)

#### Scenario: New edit discards redo history
- **GIVEN** steps S1, S2, S3 and two `undo` commands (S3 and S2 undone)
- **WHEN** a new paint command runs
- **THEN** `redo` SHALL have nothing to redo and S1 followed by the new step SHALL be the history

#### Scenario: Redo after undo
- **GIVEN** `metal1` painted at `(0,0)-(10,10)` and then undone
- **WHEN** `redo` is executed
- **THEN** `metal1 (0,0)-(10,10)` SHALL be present again

### Requirement: History limit

The system SHALL retain at least the 1000 most recent completed steps; when a new step boundary makes the number of retained steps exceed 1000 the oldest steps SHALL be discarded.
Compatibility: behavioral

- Magic's limit is fixed at 1000 (`MAXCOMMANDS = LOWCOMMANDS = 1000`). Hefesto MAY allow a larger configurable limit but SHALL NOT retain fewer than 1000 by default (Q-MODEL-21).
- Magic additionally supported an undo log file (unused); Hefesto need not.

(Magic ref: utils/undo.c:46-59,546-547,811-836)

#### Scenario: 1001st command
- **GIVEN** 1001 consecutive paint commands
- **WHEN** `undo` is executed 1001 times
- **THEN** at least the last 1000 commands SHALL have been undone and the first paint MAY remain
