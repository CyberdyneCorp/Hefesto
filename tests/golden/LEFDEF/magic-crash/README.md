# Inputs Magic crashes on (LEFDEF)

The first inputs of the LEFDEF cases that were re-authored because Magic 8.3.694 crashed on them, so
they had no capture (m6-lef-def-oasis decision D4, 2026-11-19). Each directory holds the case's
`input/` as it was in round 3 (`commands.tcl` runs from the case directory: `input/` paths). They are
not golden cases: Hefesto's own unit tests use them to check that both `magic-compat` modes no longer
crash (DEFW and the LEFDEF workstreams). See `../CASES.md`, "Authoring notes", for each cause.

| Directory | Magic | Cause |
|---|---|---|
| `LEFDEF-003/` | SIGFPE in `tech load input/lefdef.tech` | minimum-grid style `scalefactor 20 nanometers`, `gridlimit 10`: after the style's reduction the scale factor is 2, so 1 lambda = 2 / 10 = 0 internal units, and the table rescale divides by zero (utils/tech.c:808-823, cif/CIFtech.c `CIFTechOutputScale`); no Q-row covers it yet (Q-TECH-27 covers a grid limit of 0) |
| `LEFDEF-317/`, `LEFDEF-318/` | SIGSEGV in `def write` | unlabeled net `m1_0_50#` (Q-LEFDEF-101) |
| `LEFDEF-320/` | SIGSEGV in `def write` | plain labels `a_n5#` (trailing `#`) and `x1/sig*b` (hierarchical): neither special nor a port (Q-LEFDEF-101) |
| `LEFDEF-330/` | SIGSEGV in `def write -anal` | unlabeled metal3 net `m3_0_50#` (Q-LEFDEF-101) |
| `LEFDEF-333/` | SIGSEGV in `def write` | floating unlabeled metal1 net `m1_0_0#` (Q-LEFDEF-101) |
| `LEFDEF-334/` | SIGSEGV in `def write` | unlabeled metal2 net `m2_34_40#` (Q-LEFDEF-101) |

The `.ext` files are Magic's own (golden-capture.yml mode `ext-inputs`, round 3).
