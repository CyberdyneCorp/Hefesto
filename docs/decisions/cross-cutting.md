# Cross-cutting decisions (open)

These decisions affect many capabilities. They should be resolved before implementation starts.

Per-area items are in `open-questions/<AREA>.md`. There are 604 entries in total, covering bugs, quirks, documentation mismatches and platform questions.

## X-1: How to compare output with Magic when Magic's order is unspecified

Magic writes some records in an order that comes from its internals rather than from any rule:

- **Corner-stitched tile decomposition and search order:**
  - `.mag` rect order
  - GDS/CIF element order
  - LEF RECT order
  - DEF route segments
  - `drc find` / `drc count`
  - PostScript output
- **Hash-table iteration:**
  - .ext and SPICE device/instance/port order
  - LEF `-tech` layer blocks
  - DEF vias and rules
  - `.mag` elements
- **Memory addresses:** the order of coupling capacitors and which terminal is listed first (.ext `cap`, SPICE). No other implementation can reproduce this.
- **Platform-dependent sort:** the sort Magic uses for instances and properties already gives different results on Linux and on macOS.

Options:

- (a) Replicate the canonical maximal-horizontal-strip tiling and the hash algorithms. This is specified where possible: see `layout-geometry-model` and `ext-flattening-*`.
- (b) Define "compatible" as semantic equivalence, so golden tests compare canonicalized, order-insensitive output.
- (c) Hybrid: replicate tiling order (deterministic and specified), and canonicalize hash-order and address-order output.

Recommendation: (c).

Refs: Q-LEFDEF-30, Q-LEFDEF-108, Q-MASK-14/204/218/323/336/408, Q-CELL-10/11, Q-EXTRACT-5/7, NETLIST Q-1.

## X-2: Replicate or fix Magic's bugs

The specs describe what Magic does today, including suspected bugs. Each item needs a decision: replicate it for compatibility, or fix it and record the divergence.

Recommendation:
- Fix crashes and undefined behavior (uninitialized reads, NULL dereferences, buffer overflows).
- Replicate quirks that change output values (rounding, `.res.ext` +0.5 mΩ, triangle rounding) behind a "Magic-compat" switch.
- Fix data-loss bugs (port uses lost, label text truncated, `.mag.gz` saved as `.mag.gz.mag`).

## X-3: Scripting and PDK integration (Q-SCRIPT-1)

PDKs drive Magic through Tcl: `.magicrc` startup scripts and the `toolkit.tcl` PCell generators. iPadOS cannot run arbitrary Tcl with `exec`.

Proposal:
- Embed Tcl on Linux and macOS.
- On iPadOS, use a declarative PDK profile plus native C++ generators.
- Bind both to one host-independent command API, as specified in `scripting-api`.

## X-4: Host facilities missing on iPadOS

Magic relies on these host facilities:
- `fork`/`exec` (IRSIM, git, shell)
- `fcntl` file locks
- crash-recovery backups in `/tmp` with process-ID checks
- environment variables (`CAD_ROOT`, `PDK_ROOT`)
- inode identity and symlinks

Hefesto needs a platform abstraction for each of these: a sandboxed PDK store, a document-based file model, and app-managed autosave. See items tagged `[platform]` in `CELL`, `UI`, `SCRIPT` and `LEGACY`.

## X-5: Rendering fidelity

Decisions needed:
- Which `.dstyle` variant Hefesto renders (Q-UI-8).
- Stipple bit orientation, which differs between Magic's X11 and OpenGL drivers (Q-UI-6).
- Font-label metrics.
- How the touch-gesture map replaces Magic's mouse button bindings (Q-UI-22).

## X-6: Verification against Magic (deferred)

Scenarios marked "(verify against Magic)", and every `tests/golden/*/CASES.md` inventory (about 1100 cases), need Magic run headless to capture reference outputs.

This also requires the open_pdks sources for sky130, gf180mcu and IHP sg13g2. They are needed to confirm the PDK script contract (Q-SCRIPT-9).
