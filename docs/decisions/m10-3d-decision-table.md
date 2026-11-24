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

**Round 2 decisions, approved 2026-11-21 except J5.** Round 1 (PR #77) left six items for the user; the user approved them as recommended on 2026-11-21 (section "Round 2 decisions" below): the Q-MISC-74 aspect rule for tall windows (amended), two new entries, Q-MISC-79 (split tile with a left half of space, compat) and Q-MISC-80 (reopening after `.magic3d closewindow`, compat, with the use-after-free fixed in both modes), and three items without a Q row (J2 the deferred bad-style error, J4 `noplaneordering`, J6 empty extents, abutting cells and clip-edge faces). J1 (Q-MISC-74's amendment), J2, J3 (Q-MISC-79), J4 and J6 carry `Resolution:` lines. J5 (Q-MISC-80) is not approved: the MISC-3D-12 capture disproved the premise of the approved text ("ON refuses, OFF reopens"; Magic deletes the command and reopens), so it keeps a `Proposed resolution:` line and a pending `Status:` until the user decides again. `check_milestone_readiness.py` counts an entry with a pending status as unresolved (PR #81 review), so `--milestone M10` listed Q-MISC-80 as blocking until the user decided it on 2026-11-22 (next paragraph).

**Round 3 decisions, approved 2026-11-22.** Round 2 (PR #81) left J5 and eleven further items for the user; the user approved every one with the recommended option on 2026-11-22 (section "Round 3 decisions" below). J5 (Q-MISC-80) is now approved as revised: replicate in both modes, closing deletes the toolkit-path command and `specialopen wind3d` on the path reopens, with no refusal. Three new entries: Q-MISC-81 (R1, depth and culling sides flipped so higher layers are in front in the top view, fix in both modes), Q-MISC-82 (N1, layout commands a script runs go to the open 3D window with magic-compat ON, compat) and Q-MISC-83 (N2, a split tile cut by the clip area closes along its clipped bounding rectangle with magic-compat ON, compat, already implemented). R2, R3, M1, M2, W1-W4 and N3 are recorded without a Q row under "Round-3 decisions without a Q row" in `docs/decisions/open-questions/MISC.md`. Every entry carries `Status: approved 2026-11-22` and a `Resolution:` line; `python3 scripts/check_milestone_readiness.py --milestone M10` lists no entry cited by `view-3d` or `hefesto-3d-visualization`.

**Round 4 decisions, approved 2026-11-24.** After the round-3 desktop capture the user asked for the desktop 3D tab to look like the approved board and approved two decisions with the recommended option on 2026-11-24 (section "Round 4 decisions" below): Q-MISC-84 (V1, platform, both modes: the desktop tab renders the board look, with flat per-face shading, the theme's canvas background and the isometric preset on opening, in both magic-compat modes; amends R2 for the desktop tab) and Q-MISC-85 (V2, platform, both modes: non-conducting area layers ghosted by default in the desktop tab). Both carry `Status: approved 2026-11-24` and a `Resolution:` line.

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

### New entries of round 2 (cited by view-3d; Q-MISC-79 approved 2026-11-21, Q-MISC-80 approved 2026-11-22)

| ID | Question | Kind | Decision |
|---|---|---|---|
| Q-MISC-79 | split tile drawn with its left type's h and t, also when the left half is space | **compat** | ON: left type for both triangles, as Magic (a left half of space gives a flat triangle, no side faces); OFF: each triangle's own type for height, thickness and side faces |
| Q-MISC-80 | reopening after `.magic3d closewindow` | **replicate** (revised after the MISC-3D-12 capture; was compat + fix) | Magic's closewindow ends in GrTOGLDelete, which deletes the path's command and destroys the window; both modes do the same, and `specialopen wind3d` on the path opens a new window. No use-after-free exists. Approved 2026-11-22 |

### New entries of round 3 (cited by view-3d; approved 2026-11-22)

| ID | Question | Kind | Decision |
|---|---|---|---|
| Q-MISC-81 | depth-tested faces put lower layers in front in the top view (R1) | **fix** | flip the depth and culling sides in both modes so higher layers are in front in the top view; RENDER3D updates the reference pick maps |
| Q-MISC-82 | layout commands a script runs after `specialopen wind3d` (N1) | **compat** | ON: they go to the 3D window, as Magic (`box`/`load` fail, `cif ostyle` reaches the 3D `cif`); OFF: to the layout window (MISC-3D-05, MISC-3D-10) |
| Q-MISC-83 | split tile cut by the clip area (N2) | **compat** | ON: closed along its bounding rectangle clipped to the area, as Magic (W3Dmain.c:360-378); OFF: along the triangle's own cut; already implemented |

### New entries of round 4 (cited by view-3d and hefesto-3d-visualization; approved 2026-11-24)

| ID | Question | Kind | Decision |
|---|---|---|---|
| Q-MISC-84 | the desktop 3D tab uses Magic's GL-state lighting and a top-down default (V1) | **platform** | both modes: the desktop tab renders the board look (theme canvas background, flat per-face shading on each face's own normal, saturated style colours, isometric preset from above on opening with axis gizmo); Magic's GL lighting is not used by the desktop tab; `defaults`, scripts and golden cases keep the top-down default; amends R2 for the desktop tab |
| Q-MISC-85 | wells, implants and other non-conducting area layers hide the devices (V2) | **platform** | both modes: in the desktop tab a layer whose type is not a contact and whose plane holds no contact residue is drawn at alpha 0.2, no depth write, no pick id; the row's transparency control changes it and the user's value is kept for the window |

## Counts

| Kind | Rows |
|---|---|
| fix | 6 (Q-MISC-70, 71, 74, 76, 77, 81; Q-MISC-74's aspect amended and approved 2026-11-21) |
| compat | 5 (Q-MISC-72, 78, 79, 82, 83) |
| replicate | 1 (Q-MISC-80, revised after the MISC-3D-12 capture, approved 2026-11-22) |
| fix, amendment of a resolved entry | 1 (Q-MISC-73) |
| platform, no change | 1 (Q-MISC-75) |
| platform (product, desktop tab) | 2 (Q-MISC-84, 85; approved 2026-11-24) |

Every row has a `Resolution:` line: Q-MISC-70 to 79 since 2026-11-13 and 2026-11-21 (Q-MISC-74 with its 2026-11-21 amendment), Q-MISC-80 to 83 since 2026-11-22, Q-MISC-84 and 85 since 2026-11-24. Round-3 items without a Q row (R2, R3, M1, M2, W1-W4, N3) are listed in "Round 3 decisions"; they add no row here.

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

## Round 2 decisions (approved 2026-11-21, except J5)

Items left open by M10 round 1 (PR #77: tasks.md 3.4 integration notes (a) and (c), 4.2, 6.3 note (b), 8.1 input (b), and the round-1 user items). The user approved all six as recommended on 2026-11-21. Each is recorded in `docs/decisions/open-questions/MISC.md`, with `Status: approved 2026-11-21` and a `Resolution:` line: on the Q-MISC entry it touches, or under "Round-2 decisions without a Q row" there. The exception is J5: the MISC-3D-12 capture later disproved the premise of the approved text, so Q-MISC-80 keeps `Status: recommended 2026-11-21, pending user confirmation` and a `Proposed resolution:` line (replicate in both modes) until the user decides again.

| Item | Question | Decision (approved 2026-11-21; J5 approved 2026-11-22 as revised) | Where recorded |
|---|---|---|---|
| J1 | Q-MISC-74 aspect for a window taller than wide | confirmed as built: A = diag(H/W, 1, 1) when W ≥ H, else diag(1, W/H, 1), both modes | Q-MISC-74 (amendment; resolution changed) |
| J2 | `render` bad-style error deferred to the 3D stack build | both documented differences accepted, both modes: a bad style does not suppress the "Syntax" and "Unknown layer name." checks at load; the error is reported on each stack build | MISC.md, without a Q row |
| J3 | split tile whose left half is space | compat: ON keeps Magic (left type: flat, no side faces); OFF uses the triangle's own type for height, thickness and side faces | Q-MISC-79 (new) |
| J4 | `noplaneordering` skips the defaults and the micron conversion | replicate, both modes; no Q row | MISC.md, without a Q row |
| J5 | `.magic3d closewindow` then `specialopen wind3d` | compat: ON copies Magic's "Could not create a new Tk window"; OFF reopens; the use-after-free of `.magic3d` is fixed in both modes (standing rule); disproved by the MISC-3D-12 capture: Magic deletes the path and reopens, so the approval does not stand; the revised recommendation, replicate in both modes (implemented), was approved on 2026-11-22 ("Round 3 decisions") | Q-MISC-80 (new) |
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

## Round 3 decisions (approved 2026-11-22)

Items left open by M10 round 2 (PR #81: J5, the design.md "Open for the user" paragraph R1, the review findings N1 to N3 and the as-landed items of RENDER3D, MESH, PERF and WIND3D). The user approved every item with the recommended option on 2026-11-22. Each is recorded in `docs/decisions/open-questions/MISC.md` with `Status: approved 2026-11-22` and a `- Resolution:` line: on its Q entry, or under "Round-3 decisions without a Q row".

| Item | Question | Decision (approved 2026-11-22) | Where recorded |
|---|---|---|---|
| J5 | `closewindow` then `specialopen wind3d` (revised) | replicate, both modes: closing deletes the toolkit-path command as Magic's GrTOGLDelete does and a later `specialopen wind3d` on the path reopens; no refusal | Q-MISC-80 (resolution added) |
| R1 | lower layers in front in the top view with the depth test | fix, both modes: flip the depth and culling sides so higher layers are in front; RENDER3D updates the reference pick maps | Q-MISC-81 (new) |
| R2 | lighting | kept as built: ON Magic's GL state (normal (0, 0, 1), positional light at (50, 50, 50), ambient 0.6), OFF one directional light from (1, 1, 1) on each face's normal, ambient 0.4; **amended by Q-MISC-84 (2026-11-24)**: the desktop 3D tab uses the board's per-face shading in both modes | MISC.md, without a Q row |
| R3 | translucent display styles | both modes: drawn opaque in their plain colour (a consequence of Q-MISC-77) | MISC.md, without a Q row |
| M1 | spanned contact whose residues leave no gap | keeps its own h and t (J1 option) | MISC.md, without a Q row |
| M2 | memory per meshed rectangle | keep the round-2 `view3d-bench` gates (about 170 B/rect at most); D6's 64-byte target deferred, revisited after P1 | MISC.md, without a Q row |
| W1 | console noise after `view`/`scroll`/`zoom`/`findbox`/`see` | compat: ON prints the two "Unknown command:" failures of the wrapper tag's `*bypass logcommands`, OFF prints nothing | MISC.md, without a Q row |
| W2 | `specialopen` on a path below a top-level window | compat: ON leaves `window "<path>" isn't a top-level window` as the result, OFF an empty result; the window opens in both | MISC.md, without a Q row |
| W3 | crashes: missing output style, `atoi` overflow, a retired path | fix, both modes | MISC.md, without a Q row |
| W4 | `level` limits | replicate, both modes: lower limit 0, no upper limit (saturating at the `int` range) | MISC.md, without a Q row |
| N1 | layout commands a script runs while the 3D window is open | compat: ON to the 3D window as Magic, OFF to the layout window; WIND3D implements | Q-MISC-82 (new) |
| N2 | split tile cut by the clip area | compat: ON its clipped bounding rectangle, OFF the triangle; already implemented | Q-MISC-83 (new) |
| N3 | which styles are drawn | replicate, both modes: only solid and stippled styles, in the renderer and in the GL face log (MISC-3D-15) | MISC.md, without a Q row |

### Spec changes of round 3 (applied with the decisions)

`openspec/specs/view-3d/spec.md` (no requirement heading renamed):

- **J5 / Q-MISC-80**, "Opening the 3D window": "(Q-MISC-80, approved 2026-11-22, MISC-3D-12)".
- **W2**, "Opening the 3D window": the `specialopen` result per mode; new scenario "specialopen below a top-level window".
- **N1 / Q-MISC-82**, "3D view commands": the routing sentence; new scenarios "Script layout commands reach the 3D window with magic-compat ON" and "Script layout commands reach the layout window with magic-compat OFF".
- **W1**, "3D view commands": the console noise per mode; new scenario "Wrapper console noise".
- **W3 / W4**, "3D view commands": the `level` row ("clamped at 0 from below, no upper limit; values beyond the integer range saturate"); new scenario "Level limits".
- **N2 / Q-MISC-83**, "Rendered geometry": "no Q row yet" replaced by Q-MISC-83; new scenario "Split tile under the cut box".
- **N3**, "Rendered geometry": only solid or stippled styles are drawn, by the renderer and in the GL face log; new scenario "Outline style draws nothing".
- **R1 / Q-MISC-81**, "Viewing transform and shading": the depth and culling sides; new scenario "Higher layers in front in the top view".
- **R2**, "Viewing transform and shading": "(R2, recommended 2026-11-21, pending user confirmation)" becomes "(R2, approved 2026-11-22)"; the text already described both modes.
- **R3**, "Viewing transform and shading": translucent styles drawn opaque in their plain colour; new scenario "Translucent style drawn opaque".

`openspec/specs/hefesto-3d-visualization/spec.md`:

- **M1**, "Mesh generation from layout": a contact whose residues leave no gap keeps its own h and t; new scenario "Spanning a contact whose residues leave no gap".

## Round 4 decisions (approved 2026-11-24)

The user's request of 2026-11-24, after the round-3 desktop capture (`sky130_fd_sc_hd__dfxtp_1`, sky130A, magic-compat ON: light grey background, pastel faces washed to white by Magic's positional lighting on the normal (0, 0, 1), the nwell drawn as an opaque slab over the devices, a stack that reads as inverted), was that the desktop 3D tab look like the approved board (`docs/design/mockups/View3D.dc.html`, `docs/images/ui/view3d.png`). Two decisions, both approved with the recommended option on 2026-11-24, each recorded in `docs/decisions/open-questions/MISC.md` with `Status: approved 2026-11-24` and a `- Resolution:` line.

| Item | Question | Decision (approved 2026-11-24) | Where recorded |
|---|---|---|---|
| V1 | the desktop 3D tab's lighting, background and default camera | platform, both modes: the board look regardless of magic-compat (background `ThemeTokens::canvas`; flat directional shading on each face's own outward normal in the displayed scene, top faces lighter than side faces; the style colour, never washed to white; the isometric preset from above, framed to the cell, applied on opening through the same `view`/`zoom`/`scroll` commands a preset runs; axis gizmo; presets Isometric, Top, Front, Side). Magic's GL-state lighting is not used by the desktop tab; `defaults`, scripts, hefesto-cli and the golden MISC-3D cases keep the top-down default. Amends R2 (2026-11-22) for the desktop tab | Q-MISC-84 (new) |
| V2 | wells and other non-conducting area layers drawn as opaque slabs | platform, both modes: ghosted by default in the desktop tab (alpha 0.2, transparency pass, no depth write, no pick id); class rule: not a contact type and on a plane that holds no residue of any contact type (stacked contacts with their full residue set); user override per row (100 / 50 / 20 %), kept for the window, outside Magic's state; no default ghosting for a mask-layer stack (P2; the P1 tab draws the layout stack in mask-layer mode too, so the same layers are ghosted there); the board's synthetic substrate slab deferred | Q-MISC-85 (new) |

Workstreams of round 4 (design.md "Round 4 decisions"; tasks.md section 16): RENDER (`m10-3d-round4-render`) and SHELL (`m10-3d-round4-shell`).

Round-4 FIX (review of PR #102): the opening isometric camera applies only to a window the desktop opened with `render3d` (toolkit path `.render.magic`; the 3D tab and Options > 3D Display), so `specialopen wind3d` windows keep the top-down view, as the Q-MISC-84 Resolution says; its s_z is 1 in layout-layer mode and f in mask-layer mode. Two round-4 choices are not part of the Q-MISC-84 Resolution and are marked pending the user's confirmation in view-3d: the opening s_z = 1 (f in mask-layer mode) and the z mirror of the displayed scene (tz along the displayed height).

### Spec changes of round 4 (applied with the decisions)

`openspec/specs/view-3d/spec.md` (no requirement heading renamed):

- **V1 / Q-MISC-84**, "Viewing transform and shading": the sentence "Shading follows the magic-compat setting: ..." is replaced by the V1 rule (the desktop 3D tab's board look in both modes; the background from the theme in the desktop tab and the display's background colour elsewhere; Magic's GL lighting no longer used by the desktop tab); new scenarios "Board look with magic-compat ON", "Board look with magic-compat OFF", "Top faces lighter than side faces", "Background from the theme" and "Isometric default from above". "Top-down default shows the layout" and "Higher layers in front in the top view" are kept (the window state and the top preset).
- **V1 / Q-MISC-84**, "Default view": one sentence: the desktop 3D tab then applies the isometric preset through the preset's commands; `defaults` and windows opened by scripts keep the values above. No value changes.

`openspec/specs/hefesto-3d-visualization/spec.md`:

- **V2 / Q-MISC-85**, "Interactive 3D inspection", Layer visibility: the ghosting default and the layer-class rule; new scenarios "Wells ghosted by default", "User makes a ghosted layer opaque" and "Contact planes stay opaque".
- **V1 / Q-MISC-84**, "Interactive 3D inspection", Camera: the desktop tab opens on the isometric preset with an axis gizmo.
