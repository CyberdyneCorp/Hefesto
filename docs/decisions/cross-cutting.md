# Cross-cutting decisions

These decisions affect many capabilities. Per-area items are tracked in `open-questions/<AREA>.md`.

## X-1: How to compare output with Magic when its record order is arbitrary

- **Status:** decided (2026-11-05). Option: **hybrid**. Amended 2026-11-06 (D-1, option a).
- **Amendment (2026-11-06, D-1):** the original text sent every hash-table order to a sorted, canonicalized order. A sort changes results where Magic's hash order decides them, such as the LEF layer-map winner and implicit SPICE port numbering. Name-keyed hash order is therefore now reproduced exactly, and the sort is kept only for memory-address order, platform-dependent sorts and name-keyed records whose order never affects results. The bullet "Hash-table or memory-address order" below replaces the original one.
- **Decision:** the rule depends on where Magic's order comes from.
  - **Tile order.** Hefesto SHALL reproduce Magic's canonical maximal-horizontal-strip decomposition and its traversal order, as specified in `layout-geometry-model`. Output whose order comes from tiles SHALL therefore be byte-identical:
    - `.mag` rects
    - GDS/CIF elements
    - LEF RECTs
    - DEF route segments
    - `drc find` order
  - **Name-keyed hash-table order.** Where Magic's order comes from a string-keyed hash table, its order depends only on the key names and the insertion history, so it is deterministic. Hefesto SHALL reproduce that order exactly, and the output SHALL be byte-identical. The table algorithm (hash, buckets, growth, iteration, signed key bytes) is defined once, in `hefesto-output-determinism` requirement "Magic string hash table". Affected records:
    - SPICE and other netlist device, subcircuit, instance and flatten order, and implicit port numbering (`ext-flattening-hierarchy`, `netlist-spice-options`)
    - LEF `-tech` layer blocks, PROPERTYDEFINITIONS, SITE blocks and the layer-map winner (`lef-write`)
    - DEF vias, non-default rules and blockages (`def-write`)
    - `.ext` subcell and array `merge` groups (`extract-file-format`)
    - `.net` netlist files (`netlist-window`)
  - **Memory-address order, and name-keyed order that never affects results.** Hefesto SHALL write these records in a documented deterministic order (sorted by name, then by coordinates). Golden comparison SHALL canonicalize both outputs before comparing. Affected records:
    - `.ext` `cap` records and coupling-capacitor terminal order (address-keyed)
    - flat coupling capacitors and resistor networks (Q-NETLIST-1, -26, -45)
    - `drc count` cell order (address-keyed)
    - `.mag` elements (Q-CELL-11): name-keyed, but the reader does not depend on their order, so the sort is kept
    - cells in crash-backup and archive files (Q-CELL-28, D-9): name-keyed, but the reader does not depend on their order, so the sort is kept
  - **Platform-dependent sorts in Magic.** Hefesto SHALL use one deterministic ordering on all platforms.
- **Refs:** Q-LEFDEF-30, Q-LEFDEF-92, Q-LEFDEF-108, Q-LEFDEF-116, Q-MASK-14/204/218/323/336/408, Q-CELL-10/11/28, Q-EXTRACT-5/7, Q-NETLIST-1/2/26/45, Q-LEGACY-70, D-1.

## X-2: Replicate or fix Magic's bugs

- **Status:** decided (2026-11-05). Option: **tiered**. Amended 2026-11-06 (D-2) and 2026-11-13 (D-21).
- **Decision:** Hefesto handles suspected Magic bugs in three tiers:
  1. **Always fix:**
     - crashes
     - undefined behavior: uninitialized reads, NULL dereferences, buffer overflows
     - data loss, for example:
       - port uses `reset`/`scan`/`tieoff` lost on save
       - label text truncated at 99 characters
       - `.mag.gz` saved as `.mag.gz.mag`
       - backup content duplicated
  2. **Replicate behind the `magic-compat` setting (default ON):** quirks that change output values. Examples:
     - rounding asymmetries
     - `.res.ext` +0.5 mΩ offset
     - triangle rounding
     - parse-argument quirks that change results

     With the setting OFF, Hefesto uses the corrected behavior and documents each difference.

     **Amendment (2026-11-06, D-2):** the same tier applies to netgen quirks, behind a separate `netgen-compat` setting (default ON), so that LVS fidelity can be toggled without changing layout output. `hefesto-magic-compat-mode` owns the set of reference-tool compatibility settings (`magic-compat`, `netgen-compat`). **Amendment (2026-11-13, D-21):** xschem quirks get the same tier behind a third setting, `xschem-compat` (default ON), owned by `hefesto-magic-compat-mode` beside `magic-compat` and `netgen-compat`. It governs the entries of `docs/decisions/open-questions/SCHEM.md` that record `Resolution: compat`, and changes no layout, extraction or LVS output. Unlike `magic-compat`, it is written into a netlist only when OFF (one `** xschem-compat=off` line), so default netlists stay byte-identical to xschem's.
  3. **Case by case:** ambiguous items. Each open-question entry records its resolution as `Resolution: fix | compat | platform | n/a`.
- **Spec impact:** requirements that cite a tier-1 open question describe the corrected behavior once that question is resolved.

## X-3: Scripting runtime

- **Status:** decided (2026-11-05). Option: **Tcl everywhere plus native PCell fast path**.
- **Decision:**
  - Hefesto SHALL embed a sandboxed Tcl interpreter on all platforms, including iPadOS. It SHALL expose the `magic::` command API (`scripting-api`) and SHALL NOT allow process execution, sockets, or loading native extensions.
  - Hefesto SHALL additionally provide native C++ implementations of the PCell generators for supported PDKs, as a performance path. These SHALL be validated against the Tcl generators for identical cell names and geometry.
- **Risk:** App Store guideline 2.5.2 restricts executing code that is not bundled with the app. Running PDK scripts that the user imported is a grey area and SHALL be confirmed with App Review early. Fallback if rejected: on iPadOS, ship curated PDK packs whose scripts are bundled, plus the native generators.

## X-4: Host facilities on iPadOS

- **Status:** decided (2026-11-05). Option: **abstracted services**.
- **Decision:** host facilities SHALL be reached only through the platform-services interfaces of `hefesto-architecture` and `hefesto-files-and-pdks`:
  - PDK store
  - file coordination
  - app autosave
  - settings instead of environment variables
- **Desktop-only exceptions:** on iPadOS, `irsim-interface` and `shell` are unavailable. Embedded ngspice provides simulation on every platform.

## X-5: Rendering fidelity

- **Status:** decided (2026-11-05).
- **Decision:**
  - **Default style variant:** the OpenGL dstyle/cmap variant (true color with translucency). The other variants SHALL be selectable in settings.
  - **Stipple bit orientation:** follows Magic's OpenGL/Cairo driver.
  - **iPad gestures:** mode-based mapping, as follows:

    | Gesture | Action |
    |---|---|
    | One-finger drag | Primary button action |
    | Two-finger drag | Pan |
    | Pinch | Zoom |
    | Long press | Context menu with secondary and tertiary actions |
    | Apple Pencil | Precise primary action |
    | Floating modifier key | Switches to middle- or right-button semantics |

- **Still open:** font-label metrics (Q-MISC font items).

## X-6: Verification against Magic

- **Status:** open, deferred by the user until the spec is complete.
- **Needed:**
  - Run Magic headless to capture golden outputs for every `tests/golden/*/CASES.md` inventory.
  - Get the open_pdks sources. (The agent working on Q-SCRIPT-9 is verifying the PDK script contract.)
