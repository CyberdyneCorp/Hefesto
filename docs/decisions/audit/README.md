# Spec consistency audit

In November 2026 the 169 OpenSpec capabilities in `openspec/specs/` were audited in six groups, G1 to G6. Each group checked its specs for conformance with `docs/spec-conventions.md`: one `Compatibility:` line, a trace and a scenario per requirement, and no Magic identifiers in requirement text. Each group also checked that every Q-ID, capability name and requirement reference resolves, and looked for duplicated or contradictory requirements, within the group and against decisions X-1 to X-6 and P-1 to P-23. Each group fixed what was clearly wrong in its own specs, and reported cross-group findings, open decisions and terminology drift.

A final consolidation pass then:
- applied the cross-group fixes that needed no product decision;
- added the missing `undo`/`redo` command requirements to `layout-undo`, verified against Magic's `windows/windCmdSZ.c`, `windows/windCmdNR.c` and `utils/undo.c`;
- reconciled "Accept is a single undo step" (`agent-safety-and-approval`) and the testbench undo of `simulation-command` with `layout-undo`, through a new native requirement;
- merged duplicate open questions;
- collected every open decision in one list, and the terminology in one glossary.

## Totals

| Group | Scope | Fixed items |
|---|---|---|
| [G1](G1.md) | app, cell, display, layout, tech, ui | 17 |
| [G2](G2.md) | cmd, drc, mask, GDS/CIF/OASIS, antenna, font labels | 22 |
| [G3](G3.md) | extraction, netlisting, extresist, LEF/DEF | 19 |
| [G4](G4.md) | LVS, schematic, simulation | 17 |
| [G5](G5.md) | digital flow, IRSIM, PCells, plot, plow, routers, scripting, 3D | 12 |
| [G6](G6.md) | agent, hefesto, plugin, reliability, tapeout | 14 |
| Consolidation | cross-group items | 28 |
| **Total** | | **129** |

Cross-group findings:
- **28 applied** during consolidation. This includes the `undo`/`redo` commands, the native undo events, Q-MODEL-30 (new), regenerating `docs/specs-index.md`, and the M2 requirement count in the roadmap.
- **6 already fixed** by another group: the `feedback` and `archive`/locking errors in `cmd-misc-edit` and `cmd-hierarchy`, the `scalegrid` deferral, the `hefesto-security` consent fallback, and Q-NETLIST-38.
- **16 moved** to the decision list.

**Open decisions: 46.** 13 block M0–M2, 29 block a later milestone, and 4 are cosmetic. 818 of the 834 open-question entries still have no `Resolution:` line (D-5).

After consolidation:
- `openspec validate --all --strict` passes for all 169 specs.
- `scripts/check_roadmap_coverage.py` passes.
- `docs/specs-index.md` has been regenerated.

## Documents

- Group reports: [G1](G1.md), [G2](G2.md), [G3](G3.md), [G4](G4.md), [G5](G5.md), [G6](G6.md)
- Open decisions, numbered and prioritized: [DECISIONS.md](DECISIONS.md)
- Canonical terms: [docs/glossary.md](../../glossary.md)
