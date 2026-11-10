# Seeds for the mask-hint golden cases

Inputs used on 2026-11-10 to check Q-MISC-12 and Q-MISC-13 against Magic 8.3.694 (copied here so the plan does not depend on a scratch directory). GOLDEN (tasks.md 10.2) turns them into case inputs and then deletes this directory.

- `t.tech`: a one-plane technology (metal1) whose drc section is `exception X` + `width m1 3 "m1 width < 3"` + `exception none`.
- `run.tcl`, `run2.tcl`: one cell per case, a 2-unit metal1 wire plus a `MASKHINTS_X` value, then `drc check; drc catchup; drc listall count`. They seed MISC-MH-08 (Q-MISC-13): hints touching the wire from the left, right, above and below, straddling hints, the L-shaped hint split at the edge (Hd, Hd2) and the unrelated far hint (Hh). Magic reported an error for Hg (`5 -5 15 5`) and none for Hh (`5 -5 15 5 -100 0 -90 10`).
- `P.mag`, `Q.mag`, `R.mag`, `run3.tcl`: a properties section with `plane`, `PLANE` and `str` type keywords before `string BAZ qux`. They seed the new case MISC-MH-09 (Q-MISC-12): `plane`/`PLANE` end the properties section with a format error and drop the rest of the file; `str` reads as `string`.
