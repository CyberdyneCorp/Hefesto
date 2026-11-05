# UI Feedback and Highlights

## Purpose
Defines the highlight layer drawn over layout windows, and the feedback-area facility that subsystems and users use to mark areas with explanatory text. This covers:
- the order of highlight clients and how highlights are redrawn
- the feedback data model, including scaled coordinates, rounding, styles and diagonal lines
- the `feedback` command
- how other subsystems post feedback

Box drawing details are in `ui-windows-and-views`.

## Requirements

### Requirement: Highlight layer and clients
Each layout window SHALL keep, apart from the layout paint, a list of screen areas whose highlights must be erased and a list of layout areas whose highlights must be redrawn. At the end of every command, the system SHALL go through each layout window in this order:
1. Erase the pending erase areas. Restore the saved non-highlight image where available; otherwise paint them in the erase-highlights style (9).
2. Call each highlight client, in registration order, to redraw the parts of its highlights that overlap the pending redraw areas.

Later clients SHALL draw over earlier ones. The registration order SHALL be: box, feedback, user elements, crosshair, then netlist-menu points and cells (when present), then plow boundary (on first use), then selection (on first display). There SHALL be at most 10 highlight clients.

A redraw request for a root cell and an area, with or without erase, SHALL apply to every window showing that root cell. A zero-width or zero-height area SHALL first be widened to one unit on each side. Both the erase and redraw areas SHALL be grown by the window's largest label extent and by the cross size used for points, so that crosses and label text sticking out of the area are refreshed. The redraw area SHALL then be grown by one more layout unit.

Compatibility: behavioral
(Magic ref: dbwind/DBWhlights.c:86-450, dbwind/DBWprocs.c:1068-1117, utils/main.c:742-797)

#### Scenario: Selection drawn above box
- **GIVEN** the box and a selection that overlap
- **WHEN** highlights are redrawn
- **THEN** the selection highlight SHALL be drawn after the box highlight

### Requirement: Feedback area model
A feedback area SHALL hold:
- a rectangle in scaled coordinates of a root cell
- an integer scale factor f ≥ 1, meaning f scaled units = 1 layout unit
- the rectangle rounded outward to layout units
- a display style
- a text, where consecutive areas with identical text share one text instance

Adding an area for cell C SHALL:
1. Find a root cell R that is an ancestor of C and is shown in at least one layout window. If there is none, the area SHALL be silently dropped.
2. Transform the rectangle to R's coordinates. Translation offsets SHALL be multiplied by f.
3. Round outward to layout units: `xtop' = ceil(xtop/f)`, `xbot' = floor(xbot/f)`, same for y.
4. Clip to 10 units inside the maximum plane extent.

Areas SHALL be kept in insertion order, numbered 1…N for users.

Compatibility: behavioral
(Magic ref: dbwind/DBWfdback.c:20-60, dbwind/DBWfdback.c:380-550)

#### Scenario: Scaled rounding
- **GIVEN** f = 20 and the rectangle (-15,5)-(25,41) in the root cell's scaled units
- **WHEN** the area is added
- **THEN** the stored layout rectangle SHALL be (-1,0)-(2,3)

#### Scenario: No window
- **GIVEN** a cell that is shown in no window, either as root or as a descendant
- **WHEN** a subsystem adds feedback in it
- **THEN** no feedback area SHALL be created

### Requirement: Feedback display
Feedback areas SHALL be drawn as a highlight client, in every window whose root is the area's root:
- The scaled rectangle SHALL be converted to pixels with an extra division by f, rounding to nearest. It SHALL be clipped to the window, except for diagonal styles.
- The low-order style bits select the display style.
- A style flagged diagonal SHALL be drawn as the triangle half of the rectangle given by its side and direction flags.
- A style flagged "side" but not "diagonal" SHALL be drawn as a single line: from lower-left to upper-right, or from upper-left to lower-right when the direction flag is set.
- Any other style SHALL be drawn as a rectangle in that style. A zero-area rectangle under an outline style SHALL be drawn as a cross.

New feedback SHALL be displayed at the next update after it is added. Stippled styles SHALL NOT be replaced by solid at small sizes, although the source comments claim they are (see Q-UI-38).

Compatibility: behavioral
(Magic ref: dbwind/DBWfdback.c:123-250, dbwind/DBWfdback.c:577-632)

#### Scenario: Diagonal line feedback
- **WHEN** `feedback add "x" outline 0 0 10 10` is executed
- **THEN** a single line SHALL be drawn from (0,0) to (10,10), not a rectangle

### Requirement: Clearing feedback
Clearing with no text SHALL remove all feedback. Clearing with a text T SHALL remove every area whose text contains T as a substring, keeping the others in order. The removed areas SHALL be erased from the screen, grouped by root cell.

Compatibility: behavioral
(Magic ref: dbwind/DBWfdback.c:279-378)

#### Scenario: Selective clear
- **GIVEN** feedback areas with texts `width error`, `spacing error` and `width error`
- **WHEN** `feedback clear width` is executed
- **THEN** only the `spacing error` area SHALL remain, as number 1

### Requirement: feedback command
`feedback OPTION` SHALL accept the options below, matched by unique prefix. Other options SHALL report `X isn't a valid feedback option.  Try one of:` and the option names. A wrong argument count SHALL print `Wrong number of arguments for "feedback" command.` / `Type ":feedback help" for help.`.

`add TEXT [STYLE] [x y …]`:
- STYLE is one of `dotted`, `medium`, `outline`, `pale` (the default), `solid`, matched by unique prefix. Otherwise it is the long name of a display style with index 1–51. A long name with a technology style index SHALL be rejected with `Numbered styles must be between 1 and 51` (see Q-UI-23).
- An unknown style SHALL report `S isn't a valid display style.  Try one of:`.
- If the third argument is numeric, it SHALL start the point list instead of being a style.
- Points SHALL be parsed as positions in 1/20 layout-unit precision (f = 20) and SHALL come in x y pairs.
- With no points, the area SHALL be the box, with f = 1.
- With 1 point, the area SHALL be a point. A non-outlined style SHALL become outline highlights.
- With 2 points, it SHALL be a line. Diagonal lines use the side and direction flags.
- With more points, the polygon SHALL be split into rectangles and triangles, each added as an area with the same text. If the polygon yields nothing, its edges SHALL be added as lines.

Other options:
- `clear [TEXT]` (see Requirement: Clearing feedback). It also resets the `find` counter.
- `count` SHALL print `There are N feedback areas.`. In Tcl it returns N.
- `find [n]`:
  - with no n, it SHALL advance to the next area cyclically
  - with n in 1…N, it SHALL go to that area; n out of range SHALL report `Sorry, but only feedback areas 1-N exist.` and use 1
  - it SHALL place the box on the area's rounded rectangle and print `Feedback #n: TEXT`
  - with no areas it SHALL print `There are no feedback areas right now.`
- `help` SHALL print each option synopsis.
- `save FILE` SHALL write, for each area, the line `box %d %d %d %d` with the rounded rectangle, then `feedback add "TEXT"`. Every `"` in TEXT SHALL be preceded by `\`. If the style is one of the five named styles, its name SHALL follow after a space (see Q-UI-24).
- `why` SHALL print, once each, the texts of all areas in the box's root whose rounded rectangle overlaps the box.

Compatibility: behavioral (save-file content exact)
(Magic ref: commands/CmdFI.c:80-81, commands/CmdFI.c:146-505)

#### Scenario: Save quoting
- **GIVEN** one area at (0,0)-(4,4) with text `bad "x"` and style medium
- **WHEN** `feedback save f.txt` is executed
- **THEN** f.txt SHALL contain `box 0 0 4 4` and then `feedback add "bad \"x\"" medium`

#### Scenario: Find cycles
- **GIVEN** 2 feedback areas
- **WHEN** `feedback find` is executed three times
- **THEN** areas 1, 2 and then 1 SHALL be shown

### Requirement: Feedback producers
Subsystems SHALL report located problems as feedback areas with these default styles:

| Producer | Style | Scale |
|---|---|---|
| Extraction warnings and errors | pale; medium for hierarchical and uniqueness problems | 1 |
| Mask-generation (CIF/GDS output) errors | pale | the output scale factor |
| `cif see`-style displays | the requested style, with diagonal flags taken from the tiles | output scale |
| Selection-based displays | the requested style, with diagonal flags taken from the tiles | — |
| Antenna checks | pale | 1 |
| Paint-time errors | medium | 1 |
| Netlist verification and `showterms` | pale | 1 |
| Routers and plow test hooks | various | 1 |

Each producer's text SHALL describe the problem. The exact texts are owned by the producing capability.

Compatibility: behavioral
(Magic ref: extract/ExtBasic.c:1079, extract/ExtBasic.c:4665, extract/ExtHier.c:842, extract/ExtUnique.c:261, cif/CIFmain.c:431, cif/CIFsee.c:239, select/selDisplay.c:472, extflat/EFantenna.c:767, database/DBpaint.c:3056, utils/netlist.c:126, netmenu/NMcmdLZ.c:458)

#### Scenario: Extraction warning
- **GIVEN** extraction finds a problem in cell `inv`, which is shown in a window
- **WHEN** extraction finishes
- **THEN** a pale feedback area with the warning text SHALL appear over the problem location
