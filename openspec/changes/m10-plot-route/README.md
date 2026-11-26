# m10-plot-route

M10 plotting and maze routing, the next M10 workstreams after the 3D view (`openspec/changes/m10-3d-view`): `plot-output` (Magic's `plot` command with PostScript and PNM byte-identical to Magic 8.3.694, the plot styles of the technology `plot` section, and Hefesto's SVG), view export to SVG and PNG in `hefesto-rendering` (audit D-37, D-38), `router-maze` (a port of Magic's maze router that keeps its visit order, with a session-scoped interface the M11 agent can call) and `router-maze-iroute` (the `iroute` command and its parameters), plus the `mazeroute` tool deferred from M4. The plan covers the two independent streams, plot and route, with their own golden gates (MISC-PLOT-01..20, LEG-MZ-01..31, captured from Magic in CI, the SVG case from a Cairo capture variant), the desktop export dialog and maze helper, the plot and route benchmarks, and the exit check.

Collaboration and diff, the EM/IR reliability capabilities and the digital flow are later M10 changes and are not part of this one; the M12 routers stay in M12.

**Status: plan drafted 2026-11-24; decisions approved 2026-11-25; gate G0 passed 2026-11-26.**
- The user approved every row of `docs/decisions/m10-plot-route-decision-table.md` with the recommended option on 2026-11-25: the 19 entries that blocked these capabilities (Q-MISC-41..44; Q-LEGACY-6, -31..33, -85..89, -91..96), the 22 new entries (Q-MISC-48 to -55, Q-LEGACY-37 to -49 and Q-LEGACY-54), Q-MISC-45 and -47, audit decisions D-37 (option (c)) and D-38, judgement items J1–J6 and design Open Questions 1–3.
- Every entry has a `Resolution:` line; Q-LEGACY-40 and -44 are conditional rules whose `Verified` lines come from the LEG-MZ-28 and LEG-MZ-11 captures (task 2.7).
- The spec deltas are synced into the living specs and `skip_specs: true` is set; `check_milestone_readiness.py --milestone M10` reports no `plot-output`, `router-maze` or `router-maze-iroute` entry.

Sections 2–15 of `tasks.md` may start (design D1).
