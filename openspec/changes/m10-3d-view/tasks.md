# Tasks

How this plan runs:
- **Every decision this plan cites was approved on 2026-11-13**: the answers of `docs/decisions/m10-3d-decision-table.md` (Q-MISC-70..78 as drafted) and option (a) of design Open Questions 1–4 (J1, J2, the `magic-gl` golden capture, the extract subset in `extract-tech-section`'s home). Section 1 is done; no task changed.
- Phases (design D1): **P0** section 1, now (docs only). **P1** sections 2–9 and the P1 tasks of 12–14: code starts only after M5's mask-generation workstream (GEN, m5-mask-gds-cif section 6) is merged (user decision 2026-11-13). **P2** sections 10–11 and the P2 tasks of 12–14. **P3** section 15, after M7 extraction is merged.
- Each task ends with how it is verified. Workstreams edit only what design D3 assigns to them; shared-file needs are reported as open items.
- Unit tests carry `[cap:<capability>][req:<exact requirement heading>]`. Q-MISC-72 gets one test per magic-compat mode.
- Golden case inputs are authored by GOLDEN from the owning workstream's requirements; `expected/` is captured in CI (design D13).

## 1. DECIDE: the 3D open questions (P0, now)

- [x] 1.1 Present `docs/decisions/m10-3d-decision-table.md` to the user: the six blocking/cited rows, the three new entries (Q-MISC-76, 77, 78), the `render3d` cut-box spec correction, judgement item J1 and design Open Question 3 (decisions), and J2 and Open Question 4 (settled by review, for confirmation). Record the answers in the table. Verify that each answer is recorded with its date.
- [x] 1.2 For each approved entry, set `Status: approved (...)` and add the `Resolution:` line in `docs/decisions/open-questions/MISC.md` (for Q-MISC-73, replace the Resolution with the amended text). Apply "Spec changes after approval" to `openspec/specs/view-3d/spec.md` and `openspec/specs/hefesto-3d-visualization/spec.md`, including J1, J2 and the `render3d` cut-box correction. Add to the M7 entry of `docs/roadmap.md` that `extract-tech-section` starts from the subset TECH writes in M10 P1 (Open Question 4). Verify that `python3 scripts/check_milestone_readiness.py --milestone M10` lists no Q-MISC-7x entry and `openspec validate --all --strict` passes.
- [x] 1.3 Revise this change where an answer differs from the recommendation (design "Decisions this design depends on"). Verify that design.md, tasks.md and the decision table agree, and that the README and proposal no longer say "not yet approved".
- [ ] 1.4 Ask M5 STYLES to keep the `render` attributes on `mask::OutputStyle` (style index in the technology style table, height and thickness as stored, defaults) and record the answer in design D5. Verify that the M5 plan or this design names who adds `RenderAttrs`.

## 2. SCAFFOLD (P1; after M5 GEN is merged)

- [ ] 2.1 Confirm the start gate: M5 GEN merged on main and section 1 approved. Verify by the merge commit and the readiness check.
- [ ] 2.2 Add the area `view3d` (subdirectories `stack`, `mesh`, `camera`, `client`, `pick`, `section`, `export`, `overlay`) and `extract/tech`, each with `sources.cmake` and `codes.inc`, their test directories, and `docs/diagnostics/codes/view3d.md` with one section per workstream (design D2). Verify that the build passes and the code-table test covers `view3d`.
- [ ] 2.3 Write the documented public headers of design "Area contracts" with stubs, and `view3d::Session` in `EditorState` (design D4). Verify that the headers compile in `macos-debug` and `macos-bench`, `scripts/check_core_includes.py` passes, and the existing core and shell tests pass unchanged.
- [ ] 2.4 Seed `docs/design/m10-3d-ui-conformance.md` and record the as-landed contracts in design.md. Verify that `openspec validate --all --strict` passes.

## 3. TECH: layer stack data (P1)

- [ ] 3.1 Implement the extract-style subset in `extract/tech/` (design D5): styles and variants, `scale`, `units`, `planeorder`, `noplaneordering`, `height`, the end-of-style plane-order check with its warnings, the defaults and the micron conversion; every other keyword accepted silently. Hand it the `extract` raw lines from `tech/loader.cpp`. Verify the `[cap:extract-tech-section]` tests for the covered requirements (including "Layer height and thickness defaults" and its "Default height" scenario), and that sky130A, gf180mcuD and ihp-sg13g2 load with no new diagnostics.
- [ ] 3.2 Rescale the subset's h and t when the internal grid changes (ExtTech.c:4090-4093). Verify a test that changes the grid and checks h and t.
- [ ] 3.3 Add or confirm `RenderAttrs` on `mask::OutputStyle` (task 1.4), with `<style>` resolved against the display styles (integer style numbers or long names, exact). Verify the `[cap:view-3d][req:Render statement in the cifoutput section]` tests: "Render by style name", "Unknown layer", the error texts, several layers sharing a name, and the default pale-highlight style.
- [ ] 3.4 Implement `view3d::LayerStack` with `draw_height` and `physical_height_um` (design D5, Q-MISC-72). Verify `[req:Layer height and thickness for the layout-layer view]` ("Default height from plane order", split tiles use the left type, no extract style gives h = t = 0) and one Q-MISC-72 test per mode (sky130 MET1: ON draws 1.3761 as stored; OFF converts by 0.01 µm per internal unit; `physical_height_um` equal in both); a mask layer without `render` has no physical height and `VIEW3D-STACK-001` names it (gf180mcuD); and a type in two display styles gives two (type, style) entries in style-table order (Q-MISC-77).

## 4. CAMERA: view state and camera (P1)

- [ ] 4.1 Implement `ViewState`, `default_view` and `rescale` with Magic's `float` arithmetic and arithmetic-shift halving (design D7). Verify the `[req:Default view]` scenarios in both modes ("Default view of a 100 x 50 cell in layout-layer mode", "Same cell in mask-layer mode with scalefactor 10"), odd widths and negative origins.
- [ ] 4.2 Implement the viewing matrix from the current drawable size (Q-MISC-74), orthographic and perspective projections. Verify `[req:Viewing transform and shading]` "Top-down default shows the layout" (projected bbox ≈ 91% of the limiting dimension) at 500×500, 800×500 and 500×800.
- [ ] 4.3 Implement `Gesture` and `apply` (orbit, pan, zoom, reset) and the presets top, isometric, front and side as operations on `ViewState`. Verify that each gesture changes exactly the values the matching `view`/`scroll`/`zoom` command would, that presets give the documented angles, and that the touch mapping (one-finger drag orbits, pinch zooms) produces those gestures for M14 ("Touch orbit" itself is deferred to M14, design D19).
- [ ] 4.4 Implement the output-style rescale rule (Q-MISC-78, design D7). Verify one test per mode: enter mask-layer mode with f = 10, change to a style with f = 5, run `cif`; ON returns Magic's drifted trans and s_xy, OFF returns the values of the original layout-layer view.

## 5. RENDER3D: renderer (P1)

- [ ] 5.1 Spike: render a depth-tested test mesh offscreen on Raven's Vulkan device and show it through the viewport's texture mode (`Rvulkan_texture_create_from_image_view`, `Rviewport_set_texture`) on macOS (MoltenVK) and Linux (lavapipe), with resize (design D8, risk R1). Verify with screenshots in `docs/design/m10-3d-ui-conformance.md` and a note of synchronisation and layout transitions; record the color image format and views chosen (an sRGB-encoded image shown through a UNORM view, design D8); if it fails, record the fallback taken.
- [ ] 5.2 Add the render-interface pieces: offscreen target with color, `D32_SFLOAT` depth and `R32_UINT` pick-id images, double-buffered, resized from the viewport's resize callback. Verify a headless test that renders, resizes and reads back the three images.
- [ ] 5.3 Implement `Mesh3dRenderer`: chunk upload by version, instanced rectangle, side-segment and triangle pipelines, z from h, t and s_z in the shader, back-face culling, depth test with `LESS_OR_EQUAL` (Q-MISC-77), Magic's lighting, MSAA at level ≥ 1. Verify headless image tests against Hefesto reference images (top view, isometric, a split tile, a cut box) on the lavapipe CI leg, and a type in two styles showing the last style's color on its top face.
- [ ] 5.4 Implement the pick read (one pixel of the id image) and the transparency pass. Verify a headless test that picks known instances, including one behind a transparent layer.

## 6. MESH: mesh model (P1)

- [ ] 6.1 Implement the layout-layer flatten and clip in root coordinates (Q-MISC-70) to the cut box or the window area grown by 1. Verify the new scenario (instance translated by (1000, 0) under a cut box renders clipped exactly to it), the decision table's no-cut-box example (no missing faces, no spurious side faces), and `[cap:hefesto-3d-visualization][req:Mesh generation from layout]` "Region model": a box over one transistor meshes only the geometry inside it, with halo 0 for the layout-layer source (design D6).
- [ ] 6.2 Implement face generation: top and bottom rectangles, side segments on different-layer and clip boundaries, split-tile triangles with diagonal faces, closed cut faces, zero-thickness layers. Verify `[req:Rendered geometry]` "Metal prism in layout-layer mode", and property tests: every layer's mesh is closed (each edge shared by two faces) and its volume equals Σ area·t.
- [ ] 6.3 Implement chunking, the instance encoding with chunk-relative `float32` and `double` origins, Morton chunk order and the deterministic emission order (design D6). Verify that two builds of the same cell are byte-equal, and a precision test at coordinates near 2³⁰.
- [ ] 6.4 Implement background builds with progress, cancellation, progressive chunks and incremental rebuild from database change events. Verify tests that cancel mid-build (previous model kept) and that a local paint rebuilds only the affected chunks.
- [ ] 6.5 Implement the J1 option "Span connected layers" from contact residues. Verify that by default a contact uses its tech h and t, and with the option a via spans from the lower layer's top to the upper layer's bottom.

## 7. DESIGN: the 3D inspection board (P1)

- [ ] 7.1 Update `docs/design/mockups/View3D.dc.html` where the plan changes it (design D12): parity controls (level, mask/layout mode), the J1 option, the perspective toggle, P2/P3 parts disabled with their phase, the "Open in separate window" item. Verify that the board and `hefesto-design-system` "3D inspection" agree, and record the review in `docs/design/m10-3d-ui-conformance.md`.

## 8. WIND3D: window and commands (P1)

- [ ] 8.1 Implement `specialopen wind3d` in `view/window_ops.cpp` and the client (design D9): one window, cell resolution and its errors, `.magic3d`, level 1, the mode rule (mask-layer when an output style exists, otherwise layout-layer; Q-MISC-73 amendment), the mask-layer placeholder drawing until P2 (design D1), no mouse actions. Retire the `UI-CLIENT-002` `wind3d` path and update its row in `docs/diagnostics/codes/ui.md`. Verify `[req:Opening the 3D window]` "Second 3D window refused" and "Unknown cell", opening with and without an output style, `[cap:view-3d][req:Platform mechanism of the 3D view]` "No OpenGL display in Magic" (Hefesto opens the view with any display), and that the code table no longer names `wind3d`.
- [ ] 8.2 Implement `view`, `scroll`, `zoom`, `refresh`, `level`, `defaults`, `closewindow`, `help` with Magic's parsing and usage messages. Verify `[req:3D view commands]` "Relative rotation", "Relative scroll in screen units", "Zoom rejects zero", every usage error and `level` clamping.
- [ ] 8.3 Implement `cif` (per-window mode, Q-MISC-76; the f rescale with the Q-MISC-78 rule; both directions in P1), `render` query and set (with "No CIF output style set!", Q-MISC-73), `see` on per-mode sets with the output-style reset (Q-MISC-71), and `cutbox`. Verify tests for each row, the Q-MISC-71 example (hiding MET1 in mask mode does not hide a tile type after `cif`), that closing in layout mode and reopening reports and draws the same mode, the `cif` round trip returning Magic's `float` values, and a `cif ostyle` change while in mask-layer mode (Q-MISC-78, both modes).
- [ ] 8.4 Add `magic::render3d` (toolkit path `.render.magic`, `cutbox box` once, `Opts(cutbox)` set to 1 and `Opts(wind3d)` cleared on close, design D9) and wire the `wind3d` key macros and the `.magic3d` and `.render.magic` toolkit-path commands. Verify a script test that runs `render3d` with the box in the rendered cell (cutbox returns the box), moves the box (cutbox unchanged), and runs `render3d` with the box in another cell (cutbox returns `none`); and that each default key runs its command.

## 9. SHELL and INSPECT (P1 part)

- [ ] 9.1 (SHELL) Enable the 3D tab and Window > 3D Display; build the 3D viewport like `LayoutCanvas` with the RENDER3D texture, progress while building, and focus-based command routing. Verify view-model tests and a captured screenshot on macOS and Linux in the conformance document.
- [ ] 9.2 (SHELL) Build the layer stack panel (µm ranges from `physical_height_um`), show/hide/isolate/transparency, the presets, the perspective toggle and the gesture mapping of mouse, trackpad and keys. Verify view-model tests for each control and that panel visibility and `see` stay in sync.
- [ ] 9.3 (INSPECT) Implement `PickTarget` resolution and the 3D → 2D link (edit-cell material, or the subcell use with its path; scroll into view). Verify tests that pick a tile in the edit cell and one in a subcell and check the resulting `Selection`.
- [ ] 9.4 (INSPECT) Implement the 2D → 3D highlight from the selection's flattened paint. Verify that selecting a rectangle in 2D highlights exactly the overlapping instances.
- [ ] 9.5 (INSPECT) Implement net highlighting and "Isolate selected net" through `connect` (flat and hierarchical). Verify against `connect::search_flat` results on an inverter: the highlighted set equals the net's tiles.
- [ ] 9.6 (SHELL) Desktop review of P1 against the board, keyboard and screen-reader labels for every control. Verify the review entries in `docs/design/m10-3d-ui-conformance.md`.

## 10. MASKSRC: mask-layer source (P2)

- [ ] 10.1 Build mask-layer meshes from `mask::generate_flat` over the bbox or cut box grown by the radius, on the build worker with progress and cancellation (design D15). Verify `[req:Rendered geometry]` mask-layer tests, including layers without `render` (h = t = 0, pale highlight) and gf180mcuD; `[cap:hefesto-3d-visualization][req:Magic 3D view parity]` "Same stack as Magic" (metal1 with `render` 1.0 / 0.5 drawn from 1.0 to 1.5 tech units, scaled by s_z, in mask-layer mode); and "Region model" for the mask source (halo = the style's radius, mesh clipped to the box).
- [ ] 10.2 Replace the P1 placeholder of mask-layer mode with the mask-layer mesh (design D1, D15). Verify the Q-MISC-72 draw heights in both modes, that a change of output style resets the mask visibility set, and that the "mask layers arrive in phase 2" notice is gone.
- [ ] 10.3 Invalidate and regenerate planes per chunk on edits and style changes. Verify that a local edit regenerates only the affected chunks with the halo.
- [ ] 10.4 Net highlighting in mask-layer mode as a layout-net overlay (design D10). Verify the inverter net drawn over mask geometry.

## 11. INSPECT and EXPORT (P2)

- [ ] 11.1 (INSPECT) Implement cross-sections: the cut line in 2D or 3D, the section panel in µm, the 3D clipping plane. Verify `[req:Interactive 3D inspection]` "Cross-section" on an inverter: intervals and heights match the layer stack.
- [ ] 11.2 (EXPORT) Add cgltf to `cmake/Dependencies.cmake` (pinned commit, MIT) and its entry in `THIRD_PARTY_NOTICES.md`. Verify that `scripts/check_third_party_notices.py` passes.
- [ ] 11.3 (EXPORT) Implement `export3d` and the glTF writer (design D14, J2): one node per visible layer in layer order, materials from display styles, µm, `-zscale` recorded in `asset.extras`. Verify `[req:3D export]` "glTF export" with a validator check of the output (glTF-Validator in CI), physical heights, and an export from mask-layer mode on gf180mcuD that warns `VIEW3D-STACK-001`.
- [ ] 11.4 (EXPORT) Implement the binary STL writer, per layer or merged. Make glTF and STL output byte-stable: C locale, fixed triangulation, a cross-platform digest test on Linux x86_64, Linux aarch64 and macOS arm64; pin cgltf's number formatting if the digests differ. Verify that the digests are equal on every leg.
- [ ] 11.5 (SHELL) Enable Cross-section and Export glTF on the screen with the file picker. Verify view-model tests and the conformance review.

## 12. GOLDEN (P1, P2)

- [ ] 12.1 (P1) Add the `magic-gl` capture image variant: the same Magic commit with `--with-opengl` against Mesa, run under Xvfb, used only by `MISC-3D-*` cases (design D13, Open Question 3). Verify that a dispatch capture runs `specialopen wind3d` and `.magic3d view` in it.
- [ ] 12.2 (P1) Author MISC-3D-01..04. Verify that all four pass in P1 with no deviation for the mode (the window opens in mask-layer mode as Magic, design D1).
- [ ] 12.3 (P1) Author new cases: `cutbox` forms and errors, `level`, `render name h t style`, `see`/`defaults`, the `cif` round-trip values, a `cif ostyle` change in mask-layer mode (Q-MISC-78, ON), and the Q-MISC-73 no-style errors as a recorded deviation. Verify that they pass, with the deviation listed.
- [ ] 12.4 (P1) Add the GL interception shim and the comparator (`tools/golden/gl_capture/`): per (style, z), the union of top and bottom faces and of covered side edges (design D13). Add four geometry cases (flat cell, split tiles, cut box, two abutting subcells with a multi-style contact type). Verify that Hefesto's reduced mesh matches Magic's on them, including the abutting-cells case, where a face multiset would differ.
- [ ] 12.5 (P2) Add mask-layer geometry cases for sky130 (with `render`) and gf180mcuD (without). Verify that they pass.

## 13. PERF (P1, P2)

- [ ] 13.1 (P1) Add `bench/view3d/` (synthetic 1M-rectangle region and the reference layouts) and the `perf.yml` `view3d-bench` job: build time and memory gated, frame time recorded (design D17). Verify a CI run with the numbers.
- [ ] 13.2 (P1) Measure frame rate on the M3 Pro development machine and on the `hefesto-performance` reference hardware when available; record the results with the Raven confirmation checks. Verify that ≥ 30 fps at 1M rectangles is met on the M3 Pro (the P1 exit gate, design D19); the other D6 numbers (60 fps on the M1, first chunks within 200 ms) and the reference-hardware runs are recorded, a miss being an open item.
- [ ] 13.3 (P2) Re-run 13.1 and 13.2 in mask-layer mode. Verify the recorded numbers.

## 14. INTEGRATION (each phase)

- [ ] 14.1 (P1) P1 exit check (design D19): `scripts/check_requirement_coverage.py --capability view-3d --capability hefesto-3d-visualization` with the P2/P3 deferrals listed, CI green, docs (`view3d.md`, `docs/contributing/` areas and the GL capture image), roadmap status, CHANGELOG. Verify the coverage report and the CI run.
- [ ] 14.2 (P2) P2 exit check: every `view-3d` requirement covered, export determinism green, cgltf notice present, docs and CHANGELOG. Verify the coverage report and the CI run.
- [ ] 14.3 (P3) The 3D part of the M10 exit: the common exit criteria for `view-3d` and `hefesto-3d-visualization`, the overlay scenario, roadmap and CHANGELOG. Verify the coverage report shows every requirement of both capabilities covered, with the "Touch orbit" scenario listed as deferred to M14 (design D19).

## 15. OVERLAY: parasitic and field overlays (P3; after M7)

- [ ] 15.1 Implement the overlay framework: `Dataset`, color maps, legends with units and range, thresholds, the screen's overlay dropdown (design D16). It may be built with synthetic data at the end of P2. Verify view-model tests and a headless image test with a synthetic dataset.
- [ ] 15.2 Bind node capacitance and coupling capacitance from M7 `.ext` output to net geometry. Verify `[req:Parasitic and field overlays]` "Capacitance heat map": each net colored by its `.ext` total, legend in fF.
- [ ] 15.3 Bind resistance networks from `extresist-*` output as edges with values. Verify on an extracted cell that every network edge is shown with its value.
- [ ] 15.4 Bind current density from imported M9 simulation results, and the IR-drop and EM datasets from `reliability-visualization`. Verify with a simulation result of a small cell that values are shown unrecomputed, with legend and range.
