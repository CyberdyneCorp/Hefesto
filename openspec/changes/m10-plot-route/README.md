# m10-plot-route

M10 plotting and maze routing, the next M10 workstreams after the 3D view (`openspec/changes/m10-3d-view`): `plot-output` (Magic's `plot` command with PostScript and PNM byte-identical to Magic 8.3.694, the plot styles of the technology `plot` section, and Hefesto's SVG), view export to SVG and PNG in `hefesto-rendering` (audit D-37, D-38), `router-maze` (a port of Magic's maze router that keeps its visit order, with a session-scoped interface the M11 agent can call) and `router-maze-iroute` (the `iroute` command and its parameters), plus the `mazeroute` tool deferred from M4. The plan covers the two independent streams, plot and route, with their own golden gates (MISC-PLOT-01..20, LEG-MZ-01..31, captured from Magic in CI, the SVG case from a Cairo capture variant), the desktop export dialog and maze helper, the plot and route benchmarks, and the exit check.

Collaboration and diff, the EM/IR reliability capabilities and the digital flow are later M10 changes and are not part of this one; the M12 routers stay in M12.

**Status: plan drafted 2026-11-24. No decision is approved yet.**
- `check_milestone_readiness.py --milestone M10` reports 19 blocking entries for these capabilities (Q-MISC-41..44; Q-LEGACY-6, -31..33, -85..89, -91..96).
- Audit decisions D-37 and D-38 are open.
- This plan proposes 20 new entries: Q-MISC-48 to -55 and Q-LEGACY-37 to -48.
- Judgement items J1–J6 and design Open Questions 1–3 are open.

All of these, each with a recommended option, are in `docs/decisions/m10-plot-route-decision-table.md`. Nothing in sections 2–15 of `tasks.md` starts before gate G0 (design D1).
