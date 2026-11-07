# D-5 review: open-question resolutions for M0 and M1

This pass applies D-5 option (c). First, a `Resolution:` line was added wherever an existing decision already settles an entry, across all open-question files. Then resolutions were drafted for every remaining entry that an M0 or M1 capability cites. Drafted lines are marked `D-5 draft` so they can be found with `grep`.

`scripts/check_milestone_readiness.py --through M1` now exits 0. CI runs it as the gate for starting development.

## Counts

**Before and after:** 818 entries had no `Resolution:` line before this pass (D-5 table); 531 still have none. 834 entries in total.

**Mechanical (171 lines), by reason**

| Reason | Lines | Value |
|---|---|---|
| X-2 tier 1 (crash, undefined behaviour, data loss) | 87 | `fix` |
| Doc-mismatch where the spec follows Magic's code | 41 | `n/a` |
| Platform question answered by X-3, X-4, P-11, P-13, P-14, P-18, `hefesto-files-and-pdks` or `hefesto-scripting` | 22 | `platform` |
| X-1 ordering | 17 | `compat` (Magic's order reproduced) or `fix` (sorted, deterministic order) |
| Other existing decision: D-9 (Q-MODEL-9), D-11 (Q-UI-36), D-7 (Q-UI-11, Q-UI-27) | 4 | as decided |

**Drafted (97 lines, all cited by M0 or M1), by value**

| Value | Lines |
|---|---|
| `compat` | 43 |
| `fix` | 35 |
| `platform` | 13 |
| `n/a` | 6 |

Where a draft changes what a requirement must say, the citing M0 or M1 requirement was updated. A `fix` now states Hefesto's corrected behaviour next to Magic's. A `compat` now states the behaviour with `magic-compat` OFF.

## Judgment calls to review

### Drafts that change visible behaviour against Magic

- Q-TECH-2 — Missing include file — `fix` — the load fails after the warning. Magic carries on and reads the `include` line as an ordinary line.
- Q-TECH-8 — Stale name, version and description — `fix` — reset on every full load. A tech file with no name line now fails to load.
- Q-TECH-24 — Styles section and headless loading — `fix` — every `styles` section clears the style map. Missing display files never fail a load (X-4).
- Q-CELL-1 — Blank lines in `.mag` — `fix` — they are skipped. Magic abandons the rest of the file.
- Q-CELL-4 — Which duplicate instance id is renamed — `fix` — file order instead of Magic's subcell-plane order. Only malformed files are affected.
- Q-CELL-20 — PDK substitution prefix — `fix` — this changes written bytes, but only for directories like `/pdk2` next to `PDK_ROOT=/pdk`.
- Q-CELL-23 — Git equivalence check — `fix` — the intended HEAD comparison is used. Magic declares any two directories equivalent when it runs inside a git work tree.
- Q-CELL-37 — `save` of a read-only root — `fix` — an in-place overwrite is refused; "save as" still works.
- Q-CELL-41 — Whitespace in names, ids and paths — `fix` — refused when the item is created. Magic accepts them and writes files it cannot read back.
- Q-MODEL-12, Q-MODEL-13, Q-MODEL-15 — Undo replay, lock changes and `cellname rename` do not mark cells modified — `fix` (tier 1, silent data loss) — Hefesto now prompts on exit where Magic does not.
- Q-MODEL-24 — Geometry beyond the plane limits — `fix` — rejected with an error. The spec previously allowed "reject or clip".
- Q-UI-2 — Dstyle section count and order — `fix` — the file is rejected. Magic continues with shifted style indices, or exits the process.
- Q-UI-21 — `grid multiple` — `fix` — implemented as documented (every m-th line). In Magic it has no effect.
- Q-UI-25 — Command-line file arguments — `fix` — loaded in command-line order, and CIF and GDS are read. Magic loads in reverse order and only announces CIF and GDS.

### Drafts that depart from the entry's Recommendation

- Q-TECH-9 — Format line edge cases — `compat` — the Recommendation sets version 27 on a bad value. That changes the fringe-halo default, so it applies only with `magic-compat` OFF.
- Q-TECH-14, Q-TECH-16, Q-TECH-17 — Name, plane and keyword resolution in tech files — `compat` — the Recommendations fix these always. They change how rules resolve, so the fixes apply only with `magic-compat` OFF.
- Q-TECH-26 — `tech lambda` set — `compat` — the Recommendation put the raw set behind an expert option. Here Magic's behaviour is kept with the setting ON, and the raw set is refused with it OFF.
- Q-TECH-31 — Layout remap on tech reload — `compat` — remap by name is tied to `magic-compat` OFF instead of being a separate opt-in.
- Q-TECH-32 — Fringe-halo default and section order — `compat` — the Recommendation was `fix`, but it changes extracted values.
- Q-MODEL-8 — Generated ids use the full child name — `compat` — the Recommendation was `fix`, but the ids reach `.mag` files and netlists.
- Q-MODEL-20 and Q-MISC-21 — Flatten copies font rotation and offset untransformed — `compat` — the Recommendation was `fix`, but the values are written to the flattened cell.
- Q-CELL-36 — Explicit timestamp is not fixed — `compat` — the Recommendation asked the team. The library-preparation workflow should confirm.

### Drafts that pre-empt open audit decisions

- Q-SIM-1 — LGPL linking on iPadOS — `platform` — follows D-28's Recommendation (legal review, then an embedded framework). D-28 is still open and must confirm before M9.
- Q-RELIABILITY-9 — IR-drop solver — `platform` — follows D-39's Recommendation (Eigen with fixed threading). D-39 is still open and must confirm before M10, and P-21 needs amending.

### Mechanical lines that went beyond the tier-1 fix

- Q-LVS-3 — netgen case folding — `fix` — also folds brackets in the case-insensitive hash. This changes lookups that netgen fails.
- Q-LEFDEF-113 — Fixed-size name buffers — `fix` — unbounded names also lift Magic's 23-character via-name truncation, so the output changes.
- Q-LEFDEF-89 — Pin label sizing loops forever — `fix` — the fallback size (the rectangle height) was chosen in this pass, not by the entry.
- Q-SCRIPT-24 — PCell create redraws into an existing cell — `fix` — tier 1 as data loss is arguable.
- Q-LEFDEF-82 and Q-LEFDEF-92 — Layer lookup by hash order — `compat` — X-1 as amended by D-1 requires Magic's hash order, against both entries' Recommendations.

## Not resolved

- **Q-DRC-10** (diagonal DRC checks) is left for M3. It needs golden cases DRC-NM-1..3. It was cited by M0 only as the example in the `hefesto-magic-compat-mode` scenario "Provisional requirement", and that scenario now uses a generic entry.

## Follow-up

- **Out-of-milestone tier-1 fixes.** X-2's "spec impact" requires each requirement that cites a tier-1 entry to state the corrected behaviour. The mechanical tier-1 lines outside M0 and M1 were not carried into their specs, for example `router-*`, `wiring-tool`, `pcell-toolkit`, `cmd-*`, `antenna-check`, `mask-*` and `lef-*`/`def-*`. Do this when each milestone's open questions are prepared (`--milestone M<n>`).
- **Citation forms.** Mechanical lines cite their decision in slightly different forms, for example "X-2 tier 1" or "X-1 as amended by D-1". Doc-mismatch lines cite D-5, X-2 tier 3 or the spec conventions.

## Review outcome (2026-11-07)

The user approved all drafted resolutions, with one change: Q-UI-2 is now `compat`. A wrong `display_styles` count warns and continues like Magic when `magic-compat` is ON, and the process never exits. Q-SIM-1 and Q-RELIABILITY-9 stay as drafts until D-28 and D-39 are decided.
