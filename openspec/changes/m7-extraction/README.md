# m7-extraction

M7 Extraction and netlisting (`extract-tech-section`, `extract-nodes-and-labels`, `extract-capacitance`, `extract-resistance-lumped`, `extract-devices`, `extract-hierarchy`, `extract-substrate`, `extract-file-format`, `extract-commands`, `ext-flattening-read`, `ext-flattening-hierarchy`, `ext-flattening-names`, `netlist-spice-options`, `netlist-spice-devices`, `netlist-spice-hierarchy`, `netlist-sim`, `netlist-extcheck`, `extresist-command`, `extresist-network`, `extresist-output`, `antenna-check`: 21 capabilities, 207 requirements) covers:
- the full `extract` technology section, as one reader that extends M6's style subset and M10's 3D subset;
- hierarchical extraction to `.ext`: nodes and labels, area, perimeter, overlap, sidewall and fringe-halo capacitance, lumped resistance, device recognition, substrate nodes, interactions, arrays and path length;
- the `extract` command, with incremental extraction and unique labels;
- the completion of M6's `.ext` reader and flattener (devices, parameters, couplings, resistors, `.res.ext` overlays, name conversion);
- `ext2spice` in its five dialects, flat and hierarchical, `ext2sim`, and the standalone `extcheck`;
- `extresist`: R-C networks, `.res.ext` and FastHenry output;
- `antennacheck`, and the antenna rules LEF write shares;
- the 70 EXTRACT, 67 NETLIST and 9 MISC-ANT golden cases, PDK standard-cell cases for the exit criterion, and the two SCRIPT cases deferred to M7;
- the M7 exit check.

M6 already landed the `.ext` read-and-flatten subset (`src/core/src/extflat/`) and the style subset behind `extract::StyleView` (`src/core/src/extract/tech/`); they cover 18 requirements fully and 10 in part (design "Context"). M10's 3D P1 is extending `extract/tech/` in parallel (m10-3d-view design D5).

The 56 open questions that blocked M7 were approved on 2026-11-13 (`docs/decisions/m7-decision-table.md`). This plan was drafted on 2026-11-20. Its Open Questions 1–9 and the new entry Q-EXTRACT-27 were **approved by the user on 2026-11-21**, each with its recommended option ((a) for the nine questions, fix for Q-EXTRACT-27), and the spec edits they imply are applied (design "Open Questions", decision table "Planning questions raised by the M7 plan"). Round R0 is done; R1 can start.
