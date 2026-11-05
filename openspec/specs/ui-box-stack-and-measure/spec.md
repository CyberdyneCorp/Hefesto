# Box Stack, Edit Stack, Redraw Suspension and Measurement Tools

## Purpose
Specifies the script-level helper commands that Magic defines in `tools.tcl` and that layout-generating scripts use heavily, PDK device generators in particular:
- the cursor-box stack (`pushbox`, `popbox`, `peekbox`);
- the hierarchical edit stack (`pushstack`, `popstack`, `clearstack`);
- nested redraw suspension (`suspendall`, `resumeall`);
- ruler and measurement annotations (`ruler`, `measure`, `unmeasure`);
- layer key annotations (`genkey`);
- macro auto-increment.

These are commands in the `magic::` namespace and are imported globally (see `scripting-api`). Hefesto MUST provide them with the same names, arguments, results and errors.

## Requirements

### Requirement: Box stack push
`pushbox ?values?` SHALL push a 4-element box record `{llx lly urx ury}` onto a session-global stack. The stack is shared by all windows and all callers. With no argument, the record SHALL be the current cursor box in internal units, captured while units are temporarily set to `internal` and then restored. With an argument, the argument SHALL be pushed verbatim. The command SHALL return the empty string. It SHALL NOT change the box.
Compatibility: exact

(Magic ref: tcltk/tools.tcl:190-212)

#### Scenario: Push records internal units
- **GIVEN** units are `microns`, the CIF output scale is 0.01 µm per internal unit, and the box is (0,0)-(1,2) µm
- **WHEN** `pushbox` runs
- **THEN** the top of the stack SHALL be `0 0 100 200` and units SHALL still be `microns`

### Requirement: Box stack pop and peek
`popbox ?type?` (type ∈ `values` (default), `size`, `position`) SHALL take the top record b and, with units temporarily set to `internal`, apply it:
- `values`: `box values b0 b1 b2 b3`.
- `size`: `box size (b2−b0) (b3−b1)`.
- `position`: `box position b0 b1`.
- Any other type: the box is not changed, but the record is still removed.

It SHALL then remove the record, restore units, and return b. If the stack was never created, it SHALL raise error `No stack`. If the stack is empty, it SHALL raise `Empty stack`.

`peekbox ?type?` SHALL perform `popbox type` and then push the returned record back. It returns b, or raises `No stack` if the pop failed for any reason.
Compatibility: exact

(Magic ref: tcltk/tools.tcl:214-265)

#### Scenario: Pop size only
- **GIVEN** the stack top is `10 10 30 50` and the box is (100,100)-(101,101)
- **WHEN** `popbox size` runs
- **THEN** the box SHALL be (100,100)-(120,140) and the result SHALL be `10 10 30 50`

#### Scenario: Empty stack
- **GIVEN** `pushbox` and `popbox` have each run once
- **WHEN** `popbox` runs again
- **THEN** the error `Empty stack` SHALL be raised

#### Scenario: Peek leaves the stack unchanged
- **GIVEN** the stack is `{0 0 5 5} {1 1 2 2}`
- **WHEN** `peekbox` runs
- **THEN** the box SHALL be (1,1)-(2,2) and the stack SHALL still be `{0 0 5 5} {1 1 2 2}`

### Requirement: Edit stack push
`pushstack ?name?` SHALL load cell `name` into the window and remember where to return. It SHALL:
1. If `name` is empty and nothing is selected, select the cell under the box. The box is saved and restored with `pushbox`/`popbox` around this select. Then set `name` to the selected cell's definition name.
2. Raise error `No cell to push!` if `name` is still empty, or `Too many cells selected!` if it is a list of more than one element.
3. Append two entries to the session-global edit stack: the current view (`view get` in internal units), then the current window root cell name.
4. Save the `load` tag, clear it, load `name`, refresh the cell manager and window captions if they exist, then restore the `load` tag.
5. Return the empty string.
Compatibility: behavioral

(Magic ref: tcltk/tools.tcl:122-160)

#### Scenario: Generator draws inside a new cell
- **GIVEN** the window shows `top` with view (0,0)-(1000,1000)
- **WHEN** `cellname create foo` and then `pushstack foo` run
- **THEN** the window SHALL show `foo` and the edit stack SHALL end with `{0 0 1000 1000} top`

### Requirement: Edit stack pop and clear
`popstack` SHALL raise `No subcell stack!` if the stack is empty. Otherwise it SHALL:
1. Clear the `load` tag and suspend redraw.
2. Load the last stack entry (the cell).
3. Restore the second-to-last entry (the view), applied in internal units.
4. Refresh the cell manager and captions, resume redraw, and restore the `load` tag.
5. Drop the last two entries.

It returns the empty string. `clearstack` SHALL empty the edit stack. A `load` command issued from the GUI cell manager tag SHALL clear the stack (see `ui-gui-managers`).
Compatibility: behavioral

(Magic ref: tcltk/tools.tcl:162-188, tcltk/cellmgr.tcl:21)

#### Scenario: Nested push/pop
- **GIVEN** `pushstack a` and then `pushstack b` from `top`
- **WHEN** `popstack` runs twice
- **THEN** the window SHALL show `a` after the first pop and `top` after the second, each with the view it had before the corresponding push

### Requirement: Nested redraw suspension
`suspendall` SHALL, for each layout window that has a window path, increment a per-top-level-frame counter (a frame whose top-level is the root uses the window path itself). When the counter becomes 1, it SHALL issue `<window> updatedisplay suspend`. `resumeall` SHALL decrement each counter. When a counter reaches ≤ 0, it SHALL delete the counter and issue `<window> updatedisplay resume`. If no windowing toolkit is present (`winfo` undefined), both commands SHALL be no-ops. Calls nest: redraw resumes only after as many `resumeall` as `suspendall`. An unmatched `resumeall` raises an error; see Q-SCRIPT-15 for its exact text. While a frame is suspended, toolbar rebuilds for that frame SHALL be skipped.
Compatibility: behavioral

(Magic ref: tcltk/tools.tcl:8-50, tcltk/toolbar.tcl:12-16, doc/html/suspendall.html, doc/html/updatedisplay.html)

#### Scenario: Two-level nesting
- **WHEN** `suspendall; suspendall; resumeall` runs
- **THEN** the display SHALL still be suspended, and a further `resumeall` SHALL resume it

#### Scenario: Batch mode
- **GIVEN** magic runs with `-dnull`
- **WHEN** `suspendall` runs
- **THEN** it SHALL return without error and without effect

### Requirement: Ruler annotation
`ruler ?text? ?orient?` (orient ∈ `auto` (default), `horizontal`, `vertical`) SHALL draw a dimension marker inside the cursor box using drawing elements (see capability for the `element` command). Let (llx,lly,urx,ury) be `box values` in the current units, w = urx−llx, h = ury−lly, mmx = (llx+urx)/2 and mmy = (lly+ury)/2, using integer division when the operands are integers. Then:
1. `auto` selects `horizontal` iff w > h, else `vertical`.
2. Let k be a session counter: 0 for the first ruler, then +1 for each ruler.
3. Let gap tclr = 4 for horizontal or 2 for vertical when text is non-empty, else 0.
4. Horizontal ruler:
   - lines `l1_k` (llx,lly)-(llx,ury) and `l4_k` (urx,lly)-(urx,ury);
   - with mmx1 = mmx−tclr and mmx2 = mmx+tclr, adjusted to llx−2 / urx+2 if equal to llx / urx: lines `l2_k` (llx,mmy)-(mmx1,mmy) and `l3_k` (mmx2,mmy)-(urx,mmy);
   - `l2_k` gets flag `arrowleft` if llx < mmx1 else `arrowright`; `l3_k` gets `arrowright` if urx > mmx2 else `arrowleft`.
5. Vertical ruler: symmetric, with lines `l1_k` bottom edge and `l4_k` top edge, `l2_k`/`l3_k` on x = mmx, and flags `arrowbottom`/`arrowtop`.
6. If tclr > 0, add text element `t_k` at (mmx,mmy) with the text.
7. All elements are colored `black`. Coordinates are applied as internal units. See Q-SCRIPT-16.
Compatibility: exact

(Magic ref: tcltk/tools.tcl:322-425)

#### Scenario: First horizontal ruler with text
- **GIVEN** units `internal`, box (0,0)-(100,20), and no rulers yet
- **WHEN** `ruler hello` runs
- **THEN** the elements SHALL be: `l1_0` line (0,0)-(0,20), `l4_0` (100,0)-(100,20), `l2_0` (0,10)-(46,10) arrowleft, `l3_0` (54,10)-(100,10) arrowright, and text `t_0` at (50,10) `hello`

### Requirement: Measure and unmeasure
`measure ?orient?` SHALL compute s = `cif scale out`, the box values in the current units, and the orientation as for `ruler`. It SHALL set text = `format "%g um" (s × w)` for horizontal or `(s × h)` for vertical, then call `ruler text orient`. See Q-SCRIPT-17 for the dependence on the current units.

`unmeasure` SHALL list the elements in the box (`element inbox`) and collect ruler indices: from names matching regex `l[1-4]_[0-9]`, take the text from position 3; from names matching `t_[0-9]`, take the text from position 2. Matching is unanchored. It SHALL de-duplicate the indices and delete `t_k`, `l1_k`, `l2_k`, `l3_k` and `l4_k` for each.
Compatibility: exact

(Magic ref: tcltk/tools.tcl:427-486, doc/html/measure.html, doc/html/unmeasure.html)

#### Scenario: Measure text
- **GIVEN** units `internal` and the box (0,0)-(250,10), with `cif scale out` = 0.01
- **WHEN** `measure` runs
- **THEN** the ruler text SHALL be `2.5 um` and the orientation horizontal

#### Scenario: Unmeasure removes the whole ruler
- **GIVEN** ruler index 3 exists inside the box
- **WHEN** `unmeasure` runs
- **THEN** the elements `t_3`, `l1_3`, `l2_3`, `l3_3` and `l4_3` SHALL no longer exist

### Requirement: Layer key annotation
`genkey layer ?keysize 4?` SHALL run the following steps:
1. Set the box size to keysize × keysize and paint `layer`.
2. Increment a session key counter (first key = 0).
3. Move the box east by keysize.
4. Add a white text element `key<k>` with text `layer` at the box center, using integer division when the operands are integers, with flag `east`.

Coordinates are interpreted per the current snap/units settings.
Compatibility: behavioral

(Magic ref: tcltk/tools.tcl:488-510)

#### Scenario: First key
- **GIVEN** units internal and the box at (0,0)
- **WHEN** `genkey poly` runs
- **THEN** poly SHALL be painted at (0,0)-(4,4) and a text `key0` `poly` SHALL be placed at (6,2)

### Requirement: Macro auto-increment
`autoincr ?amount 1?` SHALL take the text of the last-executed command macro (`macro list .`). It SHALL find the first match of the regex `[+-]*[[:digit:]]+`, add `amount` (integer arithmetic), and replace that first occurrence. It SHALL then evaluate the new text and store it as the `.` (repeat) macro. If there is no number, it does nothing. The keys `+` and `-` SHALL be bound to `autoincr 1` and `autoincr -1`.
Compatibility: behavioral

(Magic ref: tcltk/tools.tcl:297-313)

#### Scenario: Label renumbering
- **GIVEN** the last command was `label A12`
- **WHEN** the user presses `+`
- **THEN** `label A13` SHALL execute and become the repeat macro
