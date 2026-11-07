# Open questions: TAPEOUT

Covers capabilities `hefesto-pdk-manager`, `tapeout-fill-and-density`, `tapeout-seal-ring`, `tapeout-signoff-checklist` and `tapeout-shuttle-packages`.

Sources:
- open_pdks @ `c6df28c1`
- ciel @ `d2eb3cf1`
- volare @ `13cf10af`
- IHP-Open-PDK @ `5e6d592e`
- mpw_precheck @ `0941bdc1`
- tt-support-tools @ `9d13ff3e`
- wafer.space gf180mcu-precheck @ `9b4e7dab`
- gf180mcu-project-template @ `d99f6e1b`
- LibreLane @ `8494d4fe`

### Q-TAPEOUT-1: Support for KLayout DRC decks (Ruby DSL) in sign-off
- Kind: platform
- Finding: Magic's DRC is the gating DRC in only one place: sky130 sign-off. Even there, the sky130 prechecks also gate on KLayout decks: `sky130A_mr.drc` FEOL, BEOL and offgrid, zero-area, pin-label overlap, and metal minimum clear-area density. The other PDKs gate on KLayout decks:
  - For gf180mcuD, the Tiny Tapeout and Caravel prechecks run only KLayout decks: `gf180mcu.drc`, antenna and density. mpw_precheck's Magic DRC check lists only sky130A/B as supported PDKs.
  - wafer.space runs Magic DRC but sets `ERROR_ON_MAGIC_DRC: False`, so only its KLayout DRC, antenna, density and zero-area steps gate.
  - For IHP sg13g2 and sg13cmos5l, the Tiny Tapeout precheck runs only `ihp-sg13g2.drc` or `ihp-sg13cmos5l.drc`. IHP's density rules exist only as a KLayout deck (`rule_decks/density.drc`).
  - LibreLane's gf180mcu config uses KLayout for DRC, density, antenna, filler, seal ring and LVS.
  - So for gf180mcu and IHP, Hefesto's exact Magic DRC is necessary as a design-time checker but is not sign-off.
- Evidence: mpw_precheck `check_manager/__init__.py:240-270, 448-472, 587-610`; tt-support-tools `precheck/precheck.py:489-560`; gf180mcu-precheck `precheck.py:437-470, 524-527`; open_pdks `gf180mcu/librelane/config.tcl:150-181`; IHP `ihp-sg13g2/libs.tech/klayout/tech/drc/rule_decks/`.
- Constraints:
  - KLayout is GPL. Hefesto is Apache-2.0, so Hefesto cannot link it or embed its DRC engine.
  - The decks are Ruby programs (KLayout DRC DSL, with deep or tiled modes and `-rd` variables). Supporting them means implementing a Ruby subset plus KLayout's region algebra.
  - iPadOS has no subprocess. App Store rules also restrict running downloaded interpreted code.
- Options:
  1. External process on desktop: run the user's `klayout -b -r <deck> -rd …` and import the `.lyrdb` results. On iPadOS these items are not run (or a report produced elsewhere is attached).
  2. A native clean-room interpreter for the subset of the KLayout DRC DSL that the gf180mcu, IHP and sky130 decks use.
  3. Translate each deck into Magic DRC and cifoutput rules, with a per-PDK conformance suite. This is lossy: density, antenna and connectivity-aware rules do not map.
  4. A remote sign-off service.
- Recommendation: (1) now, as specified in `tapeout-signoff-checklist`. Make "ready" depend on the deck items for programs that gate on them. Re-evaluate (2) after measuring how much of the DSL the three PDK decks use. Do not pursue (3) as sign-off. (4) needs a product decision (privacy, cost).
- Decision needed: yes. Product owner to confirm (1) as the v1 position, and decide whether iPadOS may ever show "ready" for gf180 or IHP programs.
- Resolution: decided by P-18 (2026-11-06). External KLayout on desktop; on iPadOS such designs are not marked sign-off ready.

### Q-TAPEOUT-2: Integrity hashes for PDK release assets
- Kind: ambiguity
- Finding: ciel downloads `.tar.zst` assets and does not verify any digest. A version hash identifies the source commit, not the archive content. The static-web manifests have no `sha256` field.
- Evidence: ciel `ciel/manage.py:186-230`, `ciel/source.py:195-218`.
- Options:
  - Hefesto mirrors the assets and publishes signed SHA-256 manifests.
  - Use GitHub's asset `digest` field where available.
  - Ask ciel upstream to add `sha256` to its manifests.
- Recommendation: Hefesto-signed manifests (required by `hefesto-pdk-manager`), and propose an upstream `sha256` field.

### Q-TAPEOUT-3: Catalog hosting and signing key
- Kind: platform
- Question: Who hosts the PDK and program-profile catalog? Which signature scheme does it use (for example minisign or Ed25519), and how are keys rotated? Do mirrored PDK assets on Hefesto infrastructure meet Apache-2.0 redistribution notices?
- Recommendation: Ed25519-signed index with a pinned public key in the app and a documented rotation procedure. Store the LICENSE and NOTICE files with the mirrored assets.

### Q-TAPEOUT-4: sky130 generate_fill tile clamp compares a coordinate with a width
- Kind: bug
- Behavior: the clamp is `if {$xhi > $fullwidth} {set xhi $fullwidth}`, with `fullwidth = xmax − xmin`. When the layout origin is not at 0, tiles are clamped at the wrong place. For `xmin > 0`, the last tile is cut short or becomes degenerate. For `xmin < 0`, the clamp misses.
- Evidence: open_pdks `sky130/custom/scripts/generate_fill.py:236-255`.
- Options:
  - replicate
  - fix (clamp to `xmax`/`ymax`)
- Recommendation: replicate in the exact recipe, because golden outputs depend on it. Warn when `xbase ≠ 0` or `ybase ≠ 0`, and offer a "fixed" recipe variant.

### Q-TAPEOUT-5: sky130 check_density tile denominator
- Kind: quirk
- Behavior: each 70 µm tile is flattened into cell `tile`, and coverage is measured over that cell's bounding box. That box is the extent of the flattened content, which is not necessarily the 70 µm tile. Tiles with sparse content can therefore report inflated density. The weighting of the last tile by `xfrac` assumes the full tile area.
- Evidence: open_pdks `sky130/custom/scripts/check_density.py:232-268`; Magic `cif/CIFsee.c:425-490`.
- Options:
  - replicate (exact mode)
  - fix (use the tile box as denominator)
- Recommendation: replicate in the exact report, and also compute the true-window density in the native report. Highlight where the two differ. Verify against Magic.

### Q-TAPEOUT-6: gf180mcu `metfill` obstruction sets look accidental
- Kind: quirk
- Behavior: in the `metfill` style:
  - `difffill` has no obstruction at all.
  - `polyfill` is obstructed by `obstruct_m1`, not by `obstruct_poly`.
  - `met1fill` is obstructed by poly, m1 and m2.
- Evidence: open_pdks `gf180mcu/magic/gf180mcu-cifout.tech:826-904`.
- Options:
  - replicate
  - fix
  - disable diff and poly fill for gf180 until clarified
- Recommendation: replicate geometry generation exactly. In the fill recipe, enable only the metal layers by default until the PDK maintainer confirms. File the issue upstream.

### Q-TAPEOUT-7: Canonical fill and density for gf180mcu
- Kind: ambiguity
- Finding: there are two incompatible fill flows for gf180mcu:
  - Magic: the `metfill` style
  - KLayout: `libs.tech/klayout/tech/scripts/fill_all.rb`, used by LibreLane and the wafer.space template
- The density limits per window for gf180mcu come from the PDK's `gf180mcu.drc` (decks `density`), which was not captured here. Only the Caravel global COMP ≤ 70% rule was seen.
- Evidence: open_pdks `gf180mcu/librelane/config.tcl:165-181`; mpw_precheck `checks/drc_checks/klayout/gf180mcu_density.lydrc`.
- Recommendation: capture the gf180mcu density rule values from the PDK deck into the profile. Make the native fill target those values. Keep `metfill` as an alternative recipe.

### Q-TAPEOUT-8: IHP fill has no Magic reference
- Kind: platform
- Finding: IHP fill exists only as KLayout macros and scripts (`sg13g2_filler_ActGatP.lym`, `sg13g2_filler_Metal.lym`, `sg13g2_filler_TopMetal.lym`, `scripts/filler.py`). The IHP Magic tech has fill layers but no fill style.
- Recommendation: native algorithm (spec'd) validated by IHP density and `*Fil.*` rules. Optionally port `filler.py`'s algorithm clean-room (Apache-2.0) to reach equal output. Decide whether output equality is required.

### Q-TAPEOUT-9: sky130 fill output GDS layers
- Kind: ambiguity
- Behavior: the fill style in `sky130A-GDS.tech` writes fill to the alternate layers 65/99 to 72/99. The legacy style in `sky130A.tech` writes 23/28, 28/28, 56/28, 36/28, 41/28, 34/28, 51/28 and 59/28. The Tiny Tapeout valid-layer list and the Caravel density deck use the x/28 numbers.
- Evidence: open_pdks `sky130/magic/sky130gds.tech:795-830, 1100-1460`; `sky130/magic/sky130-cifout.tech:1395-1500`; mpw_precheck `met_min_ca_density.lydrc:28-45`.
- Question: Does the foundry or the shuttle accept x/99? Is a remap step expected at merge time?
- Recommendation: verify with a precheck run. Until confirmed, the sky130 recipe uses `sky130A-GDS.tech`, with a profile option to remap to x/28.

### Q-TAPEOUT-10: Unmerged area in sky130 precheck density
- Kind: quirk
- Behavior: the KLayout clear-area density check sums `polygons(...).area` in deep mode without merging. Overlapping drawn and fill shapes are therefore counted twice.
- Evidence: mpw_precheck `checks/drc_checks/klayout/met_min_ca_density.lydrc:22-131`.
- Recommendation: Hefesto reports merged density as authoritative. It also reports the unmerged figure when that figure would change the precheck verdict.

### Q-TAPEOUT-11: Native gf180mcu seal ring
- Kind: platform
- Finding: the only gf180mcu seal ring generator is the PDK's KLayout `sealring.py` (with `pymacros/sealring_cells`). Its source was not examined here.
- Recommendation: review it as Apache-2.0 source and implement natively. Generate golden GDS with KLayout outside Hefesto for several sizes, and XOR-test against it.

### Q-TAPEOUT-12: sky130 seal ring rounding mismatch
- Kind: quirk
- Behavior: half sizes use `round(fwidth·100)` (Python banker's rounding), while full sizes use `int(fwidth·200)` (truncation). For some decimal inputs, `dwidth ≠ 2·swidth` by one unit. An example is a value where `fwidth·200` evaluates to `n − ε`. The mirrored quadrants then misalign by 5 nm.
- Evidence: open_pdks `sky130/custom/scripts/seal_ring_generator/sky130_gen_sealring.py:131-140`.
- Options:
  - replicate
  - fix (snap inputs to the 5 nm grid first, so that both agree)
- Recommendation: replicate. Input validation already snaps W and H to the grid, which avoids the mismatch for valid inputs.

### Q-TAPEOUT-13: IHP seal ring Magic cells versus KLayout PCell
- Kind: ambiguity
- Finding: IHP's Magic scripts define only `sealring_side` and `sealring_corner` cells. No assembly is given, and the side length defaults to 21600. The flow uses IHP's KLayout `sealring` PCell instead, called by LibreLane `KLayout.SealRing`. LibreLane passes `width = DIE_AREA[3]` and `height = DIE_AREA[2]`, which looks swapped. Whether the Magic cells reproduce the PCell geometry is unknown.
- Evidence: IHP `libs.tech/magic/sealring_side.tcl`, `libs.tech/klayout/python/sg13g2_pycell_lib/ihp/sealring_code.py`; LibreLane `librelane/steps/klayout.py:971-1010`.
- Recommendation: XOR the Magic-cell assembly against the PCell for square and non-square dies. If they differ, follow the PCell.

### Q-TAPEOUT-14: Project pin versus global `current` for child processes
- Kind: platform
- Question: When a project pins a version that is not ciel's `current`, how do child tools see it? This covers desktop child tools such as KLayout decks and LibreLane, which read `PDK_ROOT/<variant>`.
- Options:
  - temporarily re-point the symlinks (affects other tools)
  - a private per-run PDK root containing symlinks to the pinned version
  - refuse
- Recommendation: a private per-run root under the project's cache.

### Q-TAPEOUT-15: Unconfirmed program requirements
- Kind: ambiguity
- Findings that could not be confirmed from primary sources:
  - **Caravel/Efabless.** Efabless ceased operations in 2025. ChipFoundry continues Caravel shuttles. The current precheck fork, its required files and its PDK list were not confirmed.
  - **IHP open MPW.** Submission goes through per-run `IHP-GmbH/TO_<run>` repositories, with documentation and a recommended KLayout DRC run. The exact deck level (minimal or maximal), the filler requirement, and the naming and size rules were not confirmed.
  - **wafer.space.** Runs 1 and 2 are documented, and run 3 was not announced as of 2026-11-06. The slot sizes and checks come from `gf180mcu-precheck`. Whether the platform adds fill or a seal ring itself was not confirmed. The template inserts the seal ring with `KLayout.SealRing`.
  - **Tiny Tapeout.** Tile tables change per shuttle and per PDK.
- Recommendation: ship these profiles as catalog data marked "unverified" until a maintainer checks each one against the program's current documentation. Re-verify before each run.

### Q-TAPEOUT-16: sky130 POLY density is measured but not limited
- Kind: quirk
- Behavior: `check_density.py` prints POLY window and global densities but applies no limit.
- Evidence: open_pdks `sky130/custom/scripts/check_density.py:455-481`.
- Recommendation: replicate. Show POLY as informational.

### Q-TAPEOUT-17: PDK size on iPadOS and App Review
- Kind: platform
- Question: A full sky130A install is several GB. Should iPadOS default to a reduced library set, for example without `sky130_fd_io` GDS? Does App Review accept downloading PDK script content? (See the App Store fallback in `hefesto-scripting`.)
- Recommendation: offer a "layout essentials" set on iPadOS (`libs.tech`, `sky130_fd_pr`, one standard-cell library), and get pre-submission App Review guidance.
