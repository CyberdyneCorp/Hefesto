# Cross-cutting decisions

These decisions affect many capabilities. Per-area items are tracked in `open-questions/<AREA>.md`.

## X-1: How to compare output with Magic when its record order is arbitrary

- **Status:** decided (2026-11-05). Option: **hybrid**.
- **Decision:** the rule depends on where Magic's order comes from.
  - **Tile order.** Hefesto SHALL reproduce Magic's canonical maximal-horizontal-strip decomposition and its traversal order, as specified in `layout-geometry-model`. Output whose order comes from tiles SHALL therefore be byte-identical:
    - `.mag` rects
    - GDS/CIF elements
    - LEF RECTs
    - DEF route segments
    - `drc find` order
  - **Hash-table or memory-address order.** Hefesto SHALL write these records in a documented deterministic order (sorted by name, then by coordinates). Golden comparison SHALL canonicalize both outputs before comparing. Affected records:
    - `.ext` cap and merge records
    - SPICE device, instance and port order wherever Magic uses hash order
    - LEF `-tech` layer blocks
    - DEF vias and rules
    - `.mag` elements
    - coupling-capacitor terminal order
  - **Platform-dependent sorts in Magic.** Hefesto SHALL use one deterministic ordering on all platforms.
- **Refs:** Q-LEFDEF-30, Q-LEFDEF-108, Q-MASK-14/204/218/323/336/408, Q-CELL-10/11, Q-EXTRACT-5/7, NETLIST Q-1.

## X-2: Replicate or fix Magic's bugs

- **Status:** decided (2026-11-05). Option: **tiered**.
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
