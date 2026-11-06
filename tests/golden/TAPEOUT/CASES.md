# TAPEOUT golden test-case inventory

These cases cover capabilities `hefesto-pdk-manager`, `tapeout-fill-and-density`, `tapeout-seal-ring`, `tapeout-signoff-checklist` and `tapeout-shuttle-packages`.

Reference outputs are captured later. None of the reference tools has been run yet. The references are:
- Magic 8.3.694 @ `fc5daa9e`, driven by the unmodified open_pdks scripts @ `c6df28c1`: `generate_fill.py`, `check_density.py`, `sky130_gen_sealring.py`
- KLayout, for deck-based cases, running the decks of the pinned PDK
- ciel @ `d2eb3cf1`, for install layout
- the program prechecks: tt-support-tools @ `9d13ff3e`, mpw_precheck @ `0941bdc1`, gf180mcu-precheck @ `9b4e7dab`

The PDKs are:
- S: sky130A
- G: gf180mcuD
- I: ihp-sg13g2

Each PDK is pinned to a recorded version hash.

| ID | capability | input description (tech, layout, commands) | what to capture | requirement(s) covered |
|---|---|---|---|---|
| PDK-01 | hefesto-pdk-manager | selectors `gf180mcu`, `sky130B`, `ihp_sg13g2`, `sky90` | resolved family and variant, or the error | PDK families and variants |
| PDK-02 | hefesto-pdk-manager | library requests `default`, `all`, `sky130_fd_sc_xx` for sky130A and sky130B | resolved library sets, or the error | Library sets per variant |
| PDK-03 | hefesto-pdk-manager | static-web manifest with 3 versions, one without a date, one prerelease | listed order and update offer | Version identity and ordering; PDK catalog; Updates |
| PDK-04 | hefesto-pdk-manager | GitHub releases JSON fixture with `ihp-sg13g2-<hash>` tag only | resolved release and asset list | GitHub releases source compatibility |
| PDK-05 | hefesto-pdk-manager | asset with a wrong sha256; truncated asset; resumed asset | error text; no files under the version directory | Download integrity verification |
| PDK-06 | hefesto-pdk-manager | malicious tar.zst fixtures: `../x`, absolute path, symlink, 10× size bomb | abort, with no residue | Safe archive unpacking |
| PDK-07 | hefesto-pdk-manager | install sky130 at hash H with the default libraries; compare against a ciel install of H | directory tree listing, `SOURCES`, `current`, symlink targets (must be identical to ciel) | Install layout compatible with ciel; Enabling a version |
| PDK-08 | hefesto-pdk-manager | add `sky130_fd_sc_hs` to an installed version | assets fetched (only that library) | Incremental library downloads |
| PDK-09 | hefesto-pdk-manager | `<root>/sky130A` as a real directory, then enable | error; `current` unchanged | Enabling a version |
| PDK-10 | hefesto-pdk-manager | project pins A while `current` = B; project pins a missing version | tech path loaded; install prompt | Per-project version pinning |
| PDK-11 | hefesto-pdk-manager | uninstall the current version; uninstall a pinned version | filesystem after the operation; confirmation prompt | Uninstall and cleanup |
| PDK-12 | hefesto-pdk-manager | dedup on a version with identical sky130A and sky130B files | symlinks created; file contents unchanged | Optional file deduplication |
| PDK-13 | hefesto-pdk-manager | export a pack, then import on an offline host; tamper with one asset | install result; failing asset | Offline PDK packs |
| PDK-14 | hefesto-pdk-manager | iPadOS simulator: install with low free space; background the app mid-download | refusal message; completed install | iPadOS sandbox storage |
| PDK-15 | hefesto-pdk-manager | open a layout using a ciel-installed (external) version | consent prompt shown once per version | Script trust for managed PDKs |
| FILL-01 | tapeout-fill-and-density | S: `generate_fill.py chip.gds` on a 2920 × 3520 µm Caravel-like test chip with `FIXED_BBOX` | `chip_fill_pattern.gds.gz` (structures, XOR per layer), tile names, tile `COMMENT` boxes | sky130 tiled fill generation; sky130 wafflefill style semantics |
| FILL-02 | tapeout-fill-and-density | S: same as FILL-01 with origin at (100 µm, 100 µm) | output, showing the clamp quirk | sky130 tiled fill generation (Q-TAPEOUT-4) |
| FILL-03 | tapeout-fill-and-density | S: 100 × 100 µm tile with met1 obstructions leaving 1.5 µm, 2 µm and 4 µm channels; `wafflefill` variant `()` | fill geometry per layer | sky130 wafflefill style semantics |
| FILL-04 | tapeout-fill-and-density | S: `generate_fill_mag.py` on a `.mag` top with a vendor abstract | fill output; warning text | sky130 tiled fill generation |
| FILL-05 | tapeout-fill-and-density | G: empty 100 × 100 µm top; `cif ostyle metfill`; `cif paint` each fill layer | painted fill types per layer | gf180mcu metfill style (Q-TAPEOUT-6) |
| FILL-06 | tapeout-fill-and-density | G: drawn met1/met2 block, then metfill | fill geometry, showing obstructions | gf180mcu metfill style |
| FILL-07 | tapeout-fill-and-density | I: 1000 × 1000 µm chip with 5% Metal2; native fill; then the IHP KLayout density deck | per-window density; deck result | Native fill for KLayout-only PDKs; Density rule sets |
| FILL-08 | tapeout-fill-and-density | S: instance marked "no fill over"; `fillblock` paint | fill absent over both regions | Fill blockage and protected regions |
| FILL-09 | tapeout-fill-and-density | S: regenerate after an edit; undo | fill cell replaced; stale flag; undo restores | Fill output and regeneration |
| DEN-01 | tapeout-fill-and-density | S: `cif list coverage m1_all box` on known 70 µm boxes (0%, 50%, 100%, empty) | returned float | Window coverage measurement |
| DEN-02 | tapeout-fill-and-density | S: `check_density.py` on a 700 × 700 µm uniform-density layout | full stdout | sky130 stepped density check |
| DEN-03 | tapeout-fill-and-density | S: `check_density.py` on a 735 × 700 µm layout | full stdout (xfrac 0.500, A = 95, A_g = 105) | sky130 stepped density check |
| DEN-04 | tapeout-fill-and-density | S: `check_density.py` on a 500 × 900 µm layout | error output | sky130 stepped density check |
| DEN-05 | tapeout-fill-and-density | S: tile with sparse content in one corner | per-tile value vs. true-window value | sky130 stepped density check (Q-TAPEOUT-5) |
| DEN-06 | tapeout-fill-and-density | S: mpw_precheck `met_min_ca_density.lydrc` on a chip with overlapping met2 and met2fill | KLayout result vs. Hefesto merged and unmerged densities | Density rule sets (Q-TAPEOUT-10) |
| DEN-07 | tapeout-fill-and-density | I: IHP `density.drc` on 1000 × 1000 µm and 1700 × 900 µm chips | `.lyrdb` windows, including backup edge windows | Density rule sets; Density boundary selection |
| DEN-08 | tapeout-fill-and-density | S/I: top with no boundary layer; IHP with differing prBoundary and EdgeSeal | boundary source reported; warning | Density boundary selection |
| DEN-09 | tapeout-fill-and-density | S: DEN-03 input; Hefesto JSON report | window order and values | Density report |
| SEAL-01 | tapeout-seal-ring | S: `sky130_gen_sealring.py 1000 1000 out` | `advSeal_6um_gen.gds` (XOR), `advSeal_6um_gen.mag` text, slot array lines | sky130 seal ring dimensions; sky130 seal ring assembly |
| SEAL-02 | tapeout-seal-ring | S: `sky130_gen_sealring.py 2932 3532 out -outer` | same as SEAL-01 | sky130 seal ring dimensions |
| SEAL-03 | tapeout-seal-ring | S: W = 1000.0049 (off-grid) | swidth and dwidth values; quadrant overlap | sky130 seal ring dimensions (Q-TAPEOUT-12) |
| SEAL-04 | tapeout-seal-ring | S: rerun into a folder with existing outputs, with and without force | files unchanged or replaced; messages | sky130 seal ring assembly |
| SEAL-05 | tapeout-seal-ring | S: "wrap chip" on a 1000 × 1000 µm top | resulting hierarchy and extent | sky130 seal ring placement |
| SEAL-06 | tapeout-seal-ring | I: source `sealring_side.tcl` (default and `sealring_width 50000`, suffix `_long`) and `sealring_corner.tcl` in Magic | `.mag` output of each cell | IHP seal ring cells |
| SEAL-07 | tapeout-seal-ring | I: Hefesto assembly vs. KLayout `sealring` PCell, for a 2 × 2 mm and a 1 × 3 mm die | XOR per layer | IHP seal ring cells (Q-TAPEOUT-13) |
| SEAL-08 | tapeout-seal-ring | G: KLayout `sealring.py` for 3932 × 5122 µm and 1936 × 5122 µm | XOR against native output; 167/5 present | Native gf180mcu seal ring |
| SEAL-09 | tapeout-seal-ring | S: W = 20 | error with minimum width | Seal ring input validation |
| SO-01 | tapeout-signoff-checklist | GDS with two top cells; GDS with no top | item 1 result; later items not-run | Top-cell rules; Default check order |
| SO-02 | tapeout-signoff-checklist | TT profile, top `counter` vs. required `tt_um_alice_counter` | message | Top-cell rules |
| SO-03 | tapeout-signoff-checklist | GDS with structures `a#b`, `top/inv` and a duplicate STRNAME | messages | Cell-name legality and uniqueness |
| SO-04 | tapeout-signoff-checklist | S: GDS with 68/99 and met5 on a TT profile; G: MetalTop on wafer.space | listed pairs; forbidden-layer messages | Layer-map sanity |
| SO-05 | tapeout-signoff-checklist | TT sky130 1x1: shape at x = 161.20 µm; missing prBoundary; correct tile | messages | Boundary and size |
| SO-06 | tapeout-signoff-checklist | wafer.space all four slot sizes | required width and height | Boundary and size |
| SO-07 | tapeout-signoff-checklist | vendor GDS with a zero-area box, a degenerate path and an off-grid vertex | offenders listed | Zero-area and off-grid geometry |
| SO-08 | tapeout-signoff-checklist | TT template DEF and LEF with a missing pin, a moved pin and a missing `USE GROUND` | messages | Pin and power-pin checks |
| SO-09 | tapeout-signoff-checklist | S: Tiny Tapeout `magic_drc.tcl` on a clean tile and on a tile with 3 known errors | report file text (COUNT, coordinates `%.3f`) | Magic DRC sign-off run |
| SO-10 | tapeout-signoff-checklist | G (wafer.space): Magic DRC errors with a clean KLayout run | item severities; ready state | Magic DRC sign-off run; Position on KLayout DRC decks |
| SO-11 | tapeout-signoff-checklist | I: `ihp-sg13g2.drc` external run on a design with an M2.b violation; same on iPadOS | imported markers; not-run state | Position on KLayout DRC decks |
| SO-12 | tapeout-signoff-checklist | S: antenna violation; G: advisory antenna | item results | Antenna check item |
| SO-13 | tapeout-signoff-checklist | stale fill; missing seal ring for wafer.space | item results | Density, fill and seal ring items |
| SO-14 | tapeout-signoff-checklist | LVS with one missing net | item result and report link | LVS item |
| SO-15 | tapeout-signoff-checklist | waive a DRC marker, then move the geometry; try to waive a TT boundary item | waiver lapse; refusal | Waivers |
| SO-16 | tapeout-signoff-checklist | headless run | `results.json`, `results.md`, `results.xml`; exit code | Sign-off reports; Checklist model |
| PKG-01 | tapeout-shuttle-packages | profile JSON with an unknown field, a bad schema and a non-manufacturable PDK | accept or reject | Program profile format |
| PKG-02 | tapeout-shuttle-packages | TT sky130 `info.yaml` with `tiles: 1x1`; `uses_vapwr` without analog pins | die area; error | Built-in program profiles; Tiny Tapeout metadata |
| PKG-03 | tapeout-shuttle-packages | TT template selection for analog with and without vapwr, IHP digital and sky130 digital | chosen DEF path | Tiny Tapeout pin template selection |
| PKG-04 | tapeout-shuttle-packages | export into a TT analog template clone | file list; `MANIFEST.sha256`; untouched `.github/` | Package layout templates; Generated companion views |
| PKG-05 | tapeout-shuttle-packages | Caravel export, plus XOR vs. the golden empty wrapper outside the user area | file list; XOR result | Built-in program profiles; Reproduced program prechecks |
| PKG-06 | tapeout-shuttle-packages | wafer.space CoB with a missing marker cell; complete package | error; assembled hierarchy | Final layout assembly |
| PKG-07 | tapeout-shuttle-packages | TT sky130 package; compare the check list to the tt-support-tools precheck output | check names and order | Reproduced program prechecks |
| PKG-08 | tapeout-shuttle-packages | iPadOS export with a KLayout item not-run | summary text | Submission readiness summary |
