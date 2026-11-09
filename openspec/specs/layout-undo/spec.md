# Layout Undo

## Purpose

Specifies the undo/redo model: the event log, how events are grouped into one undo step per user command, what database and editor changes are recorded (and what is not), how undo and redo replay events, the history limit, how a new edit discards the redo tail, and when recording is suspended. This capability also owns the `undo` and `redo` commands (argument syntax, `undo print`, `undo enable|disable`) and the native undo events that Hefesto adds for its own compound operations (agent proposals, testbench edits). The commands are registered by the window client, so they are valid in every window type (capability `ui-command-dispatch`, requirement "Command registration and help text").

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
- This list applies as written while the `magic-compat` setting is ON (the default). With `magic-compat` OFF, property changes and flattening in place are recorded instead, as specified by requirement "Undoable property changes and flatten in place with magic-compat off" (decision D-10). Every other entry of the list stays non-recorded with either setting.
- Hefesto's own compound operations record native events regardless of the setting (requirement "Native undo events for Hefesto compound operations").

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
- In Magic, undo/redo do not by themselves set or clear the modified flag of the affected cells for paint events and label removals. Hefesto SHALL mark the affected cells modified for these replays too (see Q-MODEL-12). Replays that re-create a label, or place or delete a use, mark the cell modified as normal edits do.

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

The system SHALL retain the L most recent completed steps, where L is the undo history limit; when a new step boundary makes the number of retained steps exceed L the oldest steps SHALL be discarded. L SHALL be a user setting with Magic's value, 1000, as its default, in both `magic-compat` modes. A value below 1000 SHALL be rejected and the setting SHALL keep its previous value, so Hefesto never retains fewer steps than Magic. No script can observe the limit beyond running out of undo steps, so a larger setting changes no output (see Q-MODEL-21, resolution: other).
Compatibility: behavioral

- Magic's limit is fixed at 1000 steps.
- Magic additionally supported an undo log file (unused); Hefesto need not.

(Magic ref: utils/undo.c:46-59,546-547,811-836; Hefesto decision: docs/decisions/open-questions/MODEL.md#Q-MODEL-21)

#### Scenario: 1001st command
- **GIVEN** the default undo history limit and 1001 consecutive paint commands
- **WHEN** `undo` is executed 1001 times
- **THEN** the last 1000 commands SHALL have been undone and the first paint SHALL remain

#### Scenario: Larger configured limit
- **GIVEN** the undo history limit set to 2000 and 1500 consecutive paint commands
- **WHEN** `undo` is executed 1500 times
- **THEN** all 1500 commands SHALL have been undone

#### Scenario: Limit below Magic's is rejected
- **WHEN** the undo history limit is set to 500
- **THEN** the change SHALL be rejected and the limit SHALL stay at its previous value

### Requirement: undo command

The `undo` command SHALL accept the forms `undo`, `undo <n>`, `undo print <n>`, `undo enable` and `undo disable`, and SHALL act as follows.
Compatibility: behavioral

- `undo` is `undo 1`. `undo <n>` with an integer n > 0 SHALL undo up to n steps (requirement "Undo and redo semantics"). If no step could be undone, it SHALL print `Nothing more to undo`.
- An integer is an optional `+` or `-` followed by decimal digits. A negative n SHALL print `Count must be a positive integer` and change nothing.
- `undo 0`: with the `magic-compat` setting ON (the default), it SHALL behave as `undo enable`, resuming recording by one nesting level, silently. With `magic-compat` OFF, it SHALL be a no-op: it changes nothing and prints nothing (see Q-MODEL-30, decided by D-11).
- `undo enable` and `undo disable` SHALL match their keywords exactly (no prefix matching). `undo disable` SHALL suspend recording by one nesting level and `undo enable` SHALL resume it by one level, never below zero. These share the nesting counter of requirement "Non-recorded operations", so two `undo disable` need two `undo enable`.
- Any other single non-integer argument SHALL print `Option must be a count (integer)`.
- With two arguments, the first SHALL begin with `print` (its first five characters are compared) and the second SHALL be an integer, otherwise `Usage: undo print count` is printed. `undo print <n>` SHALL list the n log entries before the current position, newest first; n ≤ 0 lists back to the start of the log. It changes nothing. The listing format is informative (Magic prints internal addresses).
- More than two arguments SHALL print the usage text `Usage: undo [count]`, `       undo print [count]`, `       undo enable|disable`.
- While recording is suspended, `undo <n>` SHALL NOT replay any step. Magic prints `Attempted undo with undo disabled. . . abort function.` followed by `Nothing more to undo`; message text is informative.

(Magic ref: windows/windCmdSZ.c:634-697, windows/windClient.c:779-781, utils/undo.c:408-436,569-584,955-988, utils/strdup.c:125-134)

#### Scenario: Undo with a count
- **GIVEN** three paint commands S1, S2, S3
- **WHEN** `undo 2` is executed
- **THEN** S3 and S2 SHALL be undone and S1 SHALL remain

#### Scenario: Undo past the start of the log (verify against Magic)
- **GIVEN** one recorded step after the startup fence
- **WHEN** `undo 5` is executed
- **THEN** the step SHALL be undone and no message SHALL be printed
- **AND** a further `undo` SHALL print `Nothing more to undo`

#### Scenario: Negative count
- **WHEN** `undo -1` is executed
- **THEN** `Count must be a positive integer` SHALL be printed and nothing SHALL change

#### Scenario: Disable and enable nest
- **GIVEN** `undo disable` executed twice
- **WHEN** `undo enable` is executed once and then `paint metal1` is executed
- **THEN** the paint SHALL NOT be recorded
- **AND** after a second `undo enable`, a further paint SHALL be recorded

### Requirement: redo command

The `redo` command SHALL accept the forms `redo`, `redo <n>` and `redo print <n>`, and SHALL act as follows.
Compatibility: behavioral

- `redo` is `redo 1`. `redo <n>` with an integer n > 0 SHALL redo up to n undone steps. If no step could be redone, it SHALL print `Nothing more to redo`.
- A negative n SHALL print `Count must be a positive integer`. A non-integer single argument SHALL print `Count must be numeric`. Neither changes anything.
- `redo 0`: with the `magic-compat` setting ON (the default), it SHALL suspend recording by one nesting level, like `undo disable`, silently. With `magic-compat` OFF, it SHALL be a no-op: it changes nothing and prints nothing (see Q-MODEL-30, decided by D-11).
- With two arguments, the first SHALL begin with `print` and the second SHALL be an integer, otherwise `Usage: redo print count` is printed. `redo print <n>` SHALL list the n log entries after the current position, oldest first; n ≤ 0 lists to the end of the log. The listing format is informative.
- More than two arguments SHALL print `Usage: redo [count]`.
- `redo` has no `enable` or `disable` keyword: `redo enable` prints `Count must be numeric`.

(Magic ref: windows/windCmdNR.c:335-391, windows/windClient.c:751-753, utils/undo.c:651-694,932-953,979-988)

#### Scenario: Redo with nothing undone
- **GIVEN** a paint command and no undo since
- **WHEN** `redo` is executed
- **THEN** `Nothing more to redo` SHALL be printed and nothing SHALL change

#### Scenario: redo 0 suspends recording
- **GIVEN** recording is enabled and the `magic-compat` setting is ON
- **WHEN** `redo 0` is executed and then `paint metal1` is executed
- **THEN** the paint SHALL NOT be recorded until `undo enable` (or `undo 0`) is executed

#### Scenario: undo 0 and redo 0 are no-ops with magic-compat off
- **GIVEN** recording is enabled, the `magic-compat` setting is OFF, and step S1 = paint has been recorded
- **WHEN** `redo 0` is executed, then `paint metal2` is executed, then `undo 0` is executed
- **THEN** the `metal2` paint SHALL be recorded as a new step
- **AND** `undo 0` SHALL undo nothing, leaving both S1 and the `metal2` step in place

### Requirement: Native undo events for Hefesto compound operations

Hefesto SHALL record native undo events, in the same linear log as the Magic-compatible events, for operations that have no Magic counterpart and that are promised to be undoable as one step: applying an accepted agent proposal (capability `agent-safety-and-approval`), testbench edits made through `sim` (capability `simulation-command`) and measurement or specification definitions (capability `simulation-measurements-and-statistics`).
Compatibility: native

- Applying an accepted proposal SHALL be one step. For every cell the proposal changes, the step SHALL record enough state to restore it exactly, including the changes that ordinary commands do not record (requirement "Non-recorded operations"): property changes, cell creation, rename and deletion, and flattening. Undo SHALL restore every such cell, delete cells the proposal created and restore cells it deleted or renamed; redo SHALL re-apply the proposal.
- A testbench or measurement edit SHALL be one step whose events restore the previous `testbench.json` content. Undoing it SHALL also save the restored content.
- Recording these native events SHALL NOT change which events ordinary Magic commands record. Outside these operations, requirement "Non-recorded operations" keeps applying, including its `magic-compat` dependence (decision D-10).
- These steps obey the history limit and the redo-discard rule like any other step.

(Hefesto decision: docs/decisions/product.md#P-10, docs/decisions/product.md#P-7, docs/decisions/product.md#P-19, docs/decisions/audit/DECISIONS.md#D-10)

#### Scenario: Undo an accepted proposal that set a property and created a cell
- **GIVEN** an accepted proposal that painted in cell `top`, set property `FIXED_BBOX` on `top` and created cell `inv_x1`
- **WHEN** `undo` is executed once
- **THEN** the paint and the property SHALL be removed from `top`
- **AND** cell `inv_x1` SHALL no longer exist

#### Scenario: Plain property change stays non-recorded
- **GIVEN** recording enabled, the `magic-compat` setting ON, and no agent proposal involved
- **WHEN** `property x 1` is executed and then `undo` is executed
- **THEN** the property SHALL remain set

#### Scenario: Undo a sim testbench edit
- **GIVEN** the current testbench has no analyses and its `testbench.json` is saved
- **WHEN** `sim analysis add tran 10ps 20ns` is executed and then `undo` is executed once
- **THEN** the testbench SHALL again have no analyses, and the restored `testbench.json` SHALL be saved
- **AND** no cell SHALL change

### Requirement: Undoable property changes and flatten in place with magic-compat off

With the `magic-compat` setting OFF, Hefesto SHALL record undo events for property changes and for flattening in place, so that both can be undone and redone like any other command. With `magic-compat` ON (the default), it SHALL keep Magic's behavior of recording neither (requirement "Non-recorded operations").
Compatibility: native

- **Property changes.** Every set, change or removal of a cell property, by any command, records the property key, its previous value and type (or its absence), and its new value and type (or its absence). Undo restores the previous state of the property; redo applies the new one. Undo and redo mark the cell modified, as the property command does (capability `layout-properties`).
- **Flatten in place.** `flatten -doinplace` (capability `cmd-hierarchy`) records, in one step, the paint, labels and instances it adds to the edit cell and the deletion of each flattened instance, with the full instance state of requirement "Recorded event kinds". Undo removes the added contents and restores the instances; redo flattens them again.
- Flattening into a new cell, cell rename and cell delete stay non-recorded with either setting.
- The recorded events follow the grouping, history-limit and redo-discard rules of this capability. The setting changes nothing in any file Hefesto writes.

(Hefesto decision: docs/decisions/audit/DECISIONS.md#D-10, docs/decisions/cross-cutting.md#X-2)

#### Scenario: Property change is undoable with magic-compat off
- **GIVEN** the `magic-compat` setting OFF and a cell without property `x`
- **WHEN** `property x 1` is executed and then `undo` is executed
- **THEN** property `x` SHALL no longer exist
- **AND** after `redo`, property `x` SHALL have the value `1`

#### Scenario: Flatten in place is undoable with magic-compat off
- **GIVEN** the `magic-compat` setting OFF, and edit cell `top` with instance `x1` of `leaf` at translate(100,0), where `leaf` has metal1 (0,0)–(5,5)
- **WHEN** `flatten -doinplace x1` is executed and then `undo` is executed
- **THEN** `top` SHALL again contain instance `x1` at translate(100,0) and SHALL NOT contain metal1 (100,0)–(105,5)

