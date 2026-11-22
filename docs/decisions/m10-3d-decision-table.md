# M10 3D decision table: the open questions of `view-3d` and `hefesto-3d-visualization`

The 3D part of M10 ("3D, plotting, diff, routing, EM/IR, digital flow", docs/roadmap.md) cannot start until every open question its specs cite has a `Resolution:` line. Before approval, `python3 scripts/check_milestone_readiness.py --milestone M10` listed four 3D blockers, all in `docs/decisions/open-questions/MISC.md` and all cited by `openspec/specs/view-3d/spec.md`: Q-MISC-70, 71, 72 and 74. `hefesto-3d-visualization` cites no open question. view-3d also cites Q-MISC-73 and Q-MISC-75, which are resolved; Q-MISC-73's resolution turned out to be incomplete. The rest of M10 (plotting, diff, routing, EM/IR, digital flow) is out of scope here.

Each entry was checked against Magic 8.3.694 (fc5daa9e, `/Users/leonardo/trabalho/magic`, read-only; `graphics/W3Dmain.c` and `cif/CIFtech.c` are identical at fc5daa9e and at the checkout's HEAD 9c83a9a6) and against the sky130A, gf180mcuD and ihp-sg13g2 tech files of the pinned ciel releases. Each open entry now carries `Status: open (recommendation drafted 2026-11-13)` and a rewritten `Recommendation:` line with the full reasoning; corrected evidence is marked `(corrected 2026-11-13: ...)`. Three new entries, Q-MISC-76, 77 and 78, were found during verification and review.

**Approved 2026-11-13.** The user approved every drafted answer as written, with no option changed: Q-MISC-70, 71, 72, 74, 76, 77 and 78, and the amendment of Q-MISC-73. Each entry now carries `Status: approved (recommendation drafted 2026-11-13, approved 2026-11-13)` and a `Resolution:` line; Q-MISC-73's `Resolution:` line is replaced by the amended text and marked "resolution changed by the user 2026-11-13". Q-MISC-75 was verified and is unchanged. The user also decided the judgement items and the plan questions (next section): J1 (a), J2 (a), design Open Question 3 (a) and Open Question 4 (a). `python3 scripts/check_milestone_readiness.py --milestone M10` lists no entry cited by `view-3d` or `hefesto-3d-visualization`; the rest of M10 stays blocked.

The user decided on 2026-11-13 to resolve the 3D questions and plan the 3D work now. Code starts after M5's mask-generation workstream (GEN) lands; the plan is `openspec/changes/m10-3d-view/`, drafted against the recommendations below.

The standing rule for Magic bugs and quirks is compat: `magic-compat` ON copies Magic, OFF does the intended thing. The view-3d requirement "Viewing transform and shading" is `Compatibility: informative` and says exact pixel output is host-dependent. The only exact, observable 3D state is what the commands return (`view`, `scroll`, `zoom`, `render name`, `cutbox`), and none of the fixes below changes those values. So the M5 table's rule applies: **fix when there is no Magic output to match** (X-2 tier 3). Q-MISC-72 is the exception: Magic produces a consistent, if flat, picture there, copying it costs one multiplier, and the standing compat rule applies.

Recommendation kinds (X-2, docs/decisions/cross-cutting.md):

- **replicate**: do what Magic does, in both modes.
- **compat**: replicate with `magic-compat` ON and use the corrected behavior with it OFF (tier 2).
- **fix**: corrected behavior in both modes (tier 1, or tier 3 when there is no Magic output to match).
- **platform**: a Hefesto platform decision (X-3/X-4).
- **other**: none of the above.

**Round 2 decisions, recommended 2026-11-21, pending user confirmation.** Round 1 (PR #77) left six items for the user; they are recorded with recommendations on 2026-11-21 and await the user's confirmation (section "Round 2 decisions" below): the Q-MISC-74 aspect rule for tall windows (amended), two new entries, Q-MISC-79 (split tile with a left half of space, compat) and Q-MISC-80 (reopening after `.magic3d closewindow`, compat, with the use-after-free fixed in both modes), and three items without a Q row (J2 the deferred bad-style error, J4 `noplaneordering`, J6 empty extents, abutting cells and clip-edge faces). Until the user confirms them they carry `Proposed resolution:` lines and a pending `Status:`, and `check_milestone_readiness.py` counts an entry with a pending status as unresolved (PR #81 review), so `--milestone M10` lists Q-MISC-74 (the amendment), 79 and 80 as blocking until then.

## Judgement items

**Decided 2026-11-13: the user chose option (a) for J1, J2 and design Open Questions 3 and 4.** The options are kept for the record.

The open-question rows of the decision table are decided by the standing rules, so none of them needs a user decision. Two native-spec questions came up while planning; they are not Magic quirks, so no rule settles them. Options are listed with the recommended one first. J1 needed the user. J2 was settled by reading the spec (below) and listed so the user could overrule it; the user confirmed it. Of the two plan questions in `openspec/changes/m10-3d-view/design.md` "Open Questions", 3 (how the 3D golden cases are captured) needed the user, because it adds a second capture image to CI; 4 (the extract-style subset before M7) is settled by the user's "first slice from layout layers" decision, which rules out drawing defaults only, and by preferring one reader: option (a). Decisions of 2026-11-13: Open Question 3 (a), a `magic-gl` capture image (the same Magic commit built with OpenGL, Mesa llvmpipe under Xvfb) used only by `MISC-3D-*` cases, plus the GL interception shim compared with the per-(style, z) union comparator; Open Question 4 (a), TECH writes the extract-style subset in P1 in `extract-tech-section`'s home, and M7 extends it.

1. **J1: contacts and vias in the 3D stack.** Decision (2026-11-13): (a). `hefesto-3d-visualization` "Mesh generation from layout" says contact and via cuts SHALL be drawn as vertical solids spanning the layers they connect. view-3d (Magic parity) draws every type, contacts included, with its own height and thickness: extract `height` for the contact type in layout-layer mode, the `render` line of the cut layer in mask-layer mode. The two requirements contradict each other for the same view. The View3D mockup has a "Show contacts and vias" toggle.
   - (a) **Recommended.** Parity by default: contacts use the tech values in both sources, as Magic. "Span the connected layers" is a Hefesto view option (off by default), which extends a cut from the top of the lower connected layer to the bottom of the upper one, using the contact's residues (`tech-contacts-and-stacking`) to find the layers. The spec is reworded to make spanning an option.
   - (b) Spanning always in the Hefesto view, tech values only when the view is driven by Magic's commands (`specialopen wind3d`). Two models of one cell differ by entry point.
   - (c) Tech values only; drop spanning from the spec.
2. **J2: units and exaggeration of exported models.** Decision (2026-11-13): (a), confirmed by the user. `hefesto-3d-visualization` "3D export" says units SHALL be microns and exported geometry SHALL equal the displayed model. The displayed model exaggerates z (s_z = 25 by default, changed by `zoom`), so both cannot hold.
   - (a) **Recommended.** Export physical µm in x, y and z with no exaggeration; the model's geometry (polygons, layer set, cut box, visibility) equals the displayed one. An optional `-zscale k` multiplies z and is recorded in the glTF `asset.extras`. The spec says "the displayed model's geometry, in physical microns".
   - (b) Export what is displayed, z exaggeration included, and record the factor in `asset.extras`.
   - (c) Ask on every export (dialog only); scripts use (a).

   **Settled by review 2026-11-13: (a); confirmed by the user the same day.** s_z is a view parameter held in the view state, like the rotation angles, so "the displayed model" means its geometry; the spec only needed rewording.

## Decision table

Every row is approved (2026-11-13); the Recommendation column is the decision recorded in the entry's `Resolution:` line. Each row's full text is the entry's `Recommendation:` line in `docs/decisions/open-questions/MISC.md`. Where the draft changes the entry's original recommendation, that line ends with `(corrected 2026-11-13, was ...)`. All rows: needs_user no.

### Blocking (cited by view-3d, unresolved)

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MISC-70 | subcell tiles clipped in the wrong coordinate system | **fix** | clip in root coordinates after the instance transform, in both modes; the entry's "only with a cut box" is corrected |
| Q-MISC-71 | one visibility mask for two layer namespaces | **fix** | separate per-mode sets; `defaults` resets both; the mask set resets when the output style changes |
| Q-MISC-72 | units of `render` heights versus extract heights | **compat** | ON: render h/t as stored (Magic); OFF: µm converted by the output style's scale; `render name` and `zoom` unchanged; extensions always µm |
| Q-MISC-74 | window size fixed at 500 × 500 | **fix** | viewport, refresh area and aspect from the current size; initial request stays 500 × 500; aspect for tall windows diag(1, W/H, 1) (amended 2026-11-21) |

### Cited and resolved

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MISC-73 | `cif` toggle with no output style | **fix** (amend) | also cover opening the window (layout-layer mode), the first redraw and the `render` command ("No CIF output style set!") |
| Q-MISC-75 | OpenGL, Tk and GLX dependency | **platform** | no change |

### New entries (not cited; do not block readiness)

| ID | Question | Kind | Recommendation |
|---|---|---|---|
| Q-MISC-76 | redisplay mode is global, mode flag per window | **fix** | the mode is per-window state; a new window draws in the mode it reports |
| Q-MISC-77 | no depth test | **fix** | depth-buffered rendering in both modes; multi-style types drawn once per style with a less-or-equal test, last style wins |
| Q-MISC-78 | `cif` rescales by the current style's scalefactor | **compat** | ON: current f, as Magic; OFF: the f applied on entering mask-layer mode, and a style change rescales from f_old to f_new |

### New entries of round 2 (recommended 2026-11-21, pending user confirmation; cited by view-3d)

| ID | Question | Kind | Decision |
|---|---|---|---|
| Q-MISC-79 | split tile drawn with its left type's h and t, also when the left half is space | **compat** | ON: left type for both triangles, as Magic (a left half of space gives a flat triangle, no side faces); OFF: each triangle's own type for height, thickness and side faces |
| Q-MISC-80 | reopening after `.magic3d closewindow` | **replicate** (revised after the MISC-3D-12 capture; was compat + fix) | Magic's closewindow ends in GrTOGLDelete, which deletes the path's command and destroys the window; both modes do the same, and `specialopen wind3d` on the path opens a new window. No use-after-free exists |

## Counts

| Kind | Rows |
|---|---|
| fix | 5 (Q-MISC-70, 71, 74, 76, 77; Q-MISC-74's aspect amended 2026-11-21, pending confirmation) |
| compat | 3 (Q-MISC-72, 78, 79) |
| replicate | 1 (Q-MISC-80, revised after the MISC-3D-12 capture) |
| fix, amendment of a resolved entry | 1 (Q-MISC-73) |
| platform, no change | 1 (Q-MISC-75) |

Q-MISC-70, 71, 72 and 74 have `Resolution:` lines. The round-2 items (Q-MISC-74's amendment, 79, 80) have pending statuses, so `check_milestone_readiness.py --milestone M10` lists them until the user confirms them.

## Notable changes to the entries' original recommendations

- **Q-MISC-72**: from "replicate numerically for now; revisit with visual comparison" to compat. Magic draws mask-layer mode 100 times flatter than layout-layer mode for sky130, and the factor depends on the internal grid; OFF makes both modes physical.
- **Q-MISC-70**: still fix, but the behaviour statement was wrong: the bug shows without a cut box whenever a subcell's placement moves its tiles outside the window area expressed in child coordinates, and `render3d` sets a cut box only when a box exists in the rendered cell.
- **Q-MISC-73**: the resolution covered only the toggle; a window opened with no output style crashes before any toggle (Set3DDefaults), and the first redraw and `render` dereference the style too. The amendment, approved 2026-11-13, replaced the `Resolution:` line.

## Evidence corrections

- **Q-MISC-70**: add W3Dmain.c:266 (GrClipTriangle), :316-321 (side segments), :362-372 (cut-face closing), :1668-1680 (the root clip area); the bug also shows without a cut box; mask-layer mode is unaffected (:441).
- **Q-MISC-71**: add W3Dmain.c:879-890 (the toggle keeps the set), :1370-1371 (`defaults` sets every bit), cif/CIFmain.c:339-341 (CIFNameToMask sets every layer with the name).
- **Q-MISC-72**: CIFtech.c:1016-1026 → :1015-1027; add extract/ExtTech.c:3963-3965 and :4090-4093, W3Dmain.c:396-400 and :1239-1241; the PDK check.
- **Q-MISC-73**: add W3Dmain.c:1374-1376, :1745, :1257, :1297.
- **Q-MISC-70** (review): `cutbox box` in the wrapper is at tcltk/wrapper.tcl:1595, not :1594 (`specialopen`).
- **Q-MISC-74**: add W3Dmain.c:1566, :1579, graphics/grTOGL1.c:851-876, tcltk/wrapper.tcl:1613.
- **Q-MISC-75**: the Tk window creation starts at about :1538 (recorded as a `Verified` line).

## Spec changes after approval

Applied after the approval of 2026-11-13 in the commit "Align the 3D specs with the approved decisions", including J1 (a) and J2 (a). Each entry has its `Resolution:` line.

### view-3d

- **Q-MISC-70**, "Rendered geometry": replace "Magic clips subcell tiles against a clip area expressed in the subcell's own coordinates (see Q-MISC-70)" with "Tiles of subcells are clipped after their instance transform, in root coordinates (Q-MISC-70: Magic clips against the area in the subcell's coordinates; fixed)." Add a scenario: an instance translated by (1000, 0) under a cut box covering it renders clipped exactly to the cut box.
- **Q-MISC-71**, "Rendered geometry": replace "and one visibility set is shared by both modes … (see Q-MISC-71)" with "each mode keeps its own visibility set (Q-MISC-71: Magic shares one; fixed)". Command table, `see` row: "hides or shows, in the current mode's set, the tile types (layout-layer mode) or every mask layer with that name (mask-layer mode)". `defaults` row: add "both visibility sets reset to all visible".
- **Q-MISC-72**, "Rendered geometry", mask-layer clause: "using the `render` height and thickness: as stored with magic-compat ON; with it OFF taken as micrometres and converted to internal units by the output style's scale (Q-MISC-72)". "Render statement" EBNF comment: "stored as given; units: see Q-MISC-72".
- **Spec error (review 2026-11-13)**, "Opening the 3D window": "with the cut box following the layout box" is not Magic's behaviour. `render3d` runs `cutbox box` once (tcltk/wrapper.tcl:1595), which copies the layout box only when it is in the rendered cell (W3Dmain.c:719-738); nothing updates it later, and the wrapper's Cutbox checkbutton only copies it again. Replace with "which opens it in its own top-level window (toolkit path `.render.magic`) and sets the cut box once from the layout box (`cutbox box`)".
- **Q-MISC-73**, "Opening the 3D window": "mask-layer mode on" becomes "mask-layer mode on if a current output style exists, otherwise layout-layer mode". Command table, `render` rows: add the error "No CIF output style set!" when no output style exists.
- **Q-MISC-74**, "Viewing transform and shading": replace "(Magic keeps 500 × 500 regardless of resizing; see Q-MISC-74)" with "for the window's current size (Q-MISC-74: Magic keeps 500 × 500 after a resize; fixed)". "Opening the 3D window" keeps "an initial size of 500 × 500 pixels".
- **Q-MISC-76**, "3D view commands", `cif` row: add "the mode is per window; a new window draws in the mode it reports (Q-MISC-76)".
- **Q-MISC-77**, "Viewing transform and shading": add "faces are depth-tested; a type in several styles is drawn once per style and the last style drawn wins on equal depth (Q-MISC-77: Magic draws in style order without a depth test; fixed)".
- **Q-MISC-78**, "3D view commands", `cif` row: add "rescales by the current output style's scalefactor (Q-MISC-78: with magic-compat OFF, by the scalefactor applied on entering mask-layer mode)".

### hefesto-3d-visualization

- **Q-MISC-71**, "Interactive 3D inspection", Layer visibility: acts on the same per-mode sets as `see`.
- **Q-MISC-72**, "3D export" and "Interactive 3D inspection" (cross-sections): add "heights are in µm regardless of magic-compat; mask layers without a `render` height are reported, not given one".
- **J1** ((a), approved), "Mesh generation from layout": "Contact and via cuts SHALL be drawn with their technology height and thickness; a view option SHALL draw them as vertical solids spanning the layers they connect."
- **J2** ((a), approved), "3D export": "Exported geometry SHALL equal the displayed model's geometry in physical microns, without the view's z exaggeration, unless a z scale is requested; the scale is recorded in the file."

## Round 2 decisions (recommended 2026-11-21, pending user confirmation)

Items left open by M10 round 1 (PR #77: tasks.md 3.4 integration notes (a) and (c), 4.2, 6.3 note (b), 8.1 input (b), and the round-1 user items). All six are recorded with recommendations on 2026-11-21 and await the user's confirmation. Each is recorded in `docs/decisions/open-questions/MISC.md`, with `Status: recommended 2026-11-21, pending user confirmation` and a `Proposed resolution:` line: on the Q-MISC entry it touches, or under "Round-2 decisions without a Q row" there. On confirmation each becomes a `Resolution:` line.

| Item | Question | Recommendation (2026-11-21, pending confirmation) | Where recorded |
|---|---|---|---|
| J1 | Q-MISC-74 aspect for a window taller than wide | confirmed as built: A = diag(H/W, 1, 1) when W ≥ H, else diag(1, W/H, 1), both modes | Q-MISC-74 (amendment; resolution changed) |
| J2 | `render` bad-style error deferred to the 3D stack build | both documented differences accepted, both modes: a bad style does not suppress the "Syntax" and "Unknown layer name." checks at load; the error is reported on each stack build | MISC.md, without a Q row |
| J3 | split tile whose left half is space | compat: ON keeps Magic (left type: flat, no side faces); OFF uses the triangle's own type for height, thickness and side faces | Q-MISC-79 (new) |
| J4 | `noplaneordering` skips the defaults and the micron conversion | replicate, both modes; no Q row | MISC.md, without a Q row |
| J5 | `.magic3d closewindow` then `specialopen wind3d` | compat: ON copies Magic's "Could not create a new Tk window"; OFF reopens; the use-after-free of `.magic3d` is fixed in both modes (standing rule); disproved by the MISC-3D-12 capture: Magic deletes the path and reopens, so the revised recommendation is replicate in both modes (implemented) | Q-MISC-80 (new) |
| J6 | empty cell extent, abutting cells, clip-edge faces | confirmed, both modes: an empty dimension is left out of the fit (1 × 1 when both are empty); pieces abutting across cells get no inner side faces; faces on the clip area are closed | MISC.md, without a Q row |

### Spec changes of round 2 (applied with the decisions)

`openspec/specs/view-3d/spec.md`:

- **J1 / Q-MISC-74**, "Viewing transform and shading": A = diag(H_w/W_w, 1, 1) when W_w ≥ H_w, else diag(1, W_w/H_w, 1); new scenario "Tall window" (100 × 100 cell at 500 × 800: A = diag(1, 0.625, 1), about 91 % of the width and 57 % of the height).
- **J2**, "Render statement in the cifoutput section": the style is resolved and its error reported when the 3D view builds its layer stack; the two accepted differences; new scenario "Bad style reported when the layer stack is built".
- **J3 / Q-MISC-79**, "Layer height and thickness for the layout-layer view": the split-tile rule per mode; new scenario "Split tile whose left half is space".
- **J4**, same requirement: no defaults and no conversion with `noplaneordering`, both modes; new scenario "No defaults with noplaneordering".
- **J5 / Q-MISC-80**, "Opening the 3D window": the reopening rule per mode and the use-after-free fix; new scenario "Reopening after closewindow".
- **J6**, "Default view": the empty-extent fit and the scenario "Empty cell"; "Rendered geometry": side faces on each layer's flattened boundary, none between pieces abutting across cells, faces on the clip area (cut box or window area) closed; new scenario "Abutting cells leave no inner faces".

`openspec/specs/hefesto-3d-visualization/spec.md` needs no change: its export and cross-sections take the displayed geometry, which these rules define.
