# Inputs Magic crashes on (MASK)

The first inputs of MASK cases that were re-authored because Magic 8.3.694 crashed on them, kept as
`tests/golden/LEFDEF/magic-crash/` keeps the LEFDEF ones (m6-lef-def-oasis decision D4). Each directory
holds the case's `input/` as it was before the re-authoring (`commands.tcl` runs from the case
directory: `input/` paths). They are not golden cases: Hefesto's own unit tests read them from here to
check that both `magic-compat` modes run them without fault. This is the only copy.

| Directory | Magic | Cause |
|---|---|---|
| `MASK-OS-016/` | SIGSEGV (status 139) in golden-capture run 44743146734 | `lef read` of `VIA V1`, the tech section's cut alias (`cut via1 via1 V1`), so LefRead redefines the shared cut record (LefRedefined, lefRead.c:611-671, Q-LEFDEF-8), then `lef write cont -tech`. The re-authored case (`VIA VX1`) still crashes Magic (run 44860968058), so the redefinition is not shown to be the cause. Read by `tests/unit/lef/read/magic_crash_test.cpp` |
