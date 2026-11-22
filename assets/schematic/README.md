# Schematic system library

`xschem_library/devices/` is Hefesto's clean-room system symbol library (capability
`schematic-symbols-and-libraries`, Requirement "System library"; `openspec/changes/m8-schematic` design D8).
It is reference-compatible with xschem's `devices` library: the same file names, pin names, order and
positions, and netlisting properties, taken only from the spec's interface tables
(`tests/data/schem/devices_interface.json`). The graphics are drawn for Hefesto.

- Licence: Apache-2.0; each file carries the notice in its `v` record. Shipping the library in a release waits
  for the counsel review of D8 step 4 (an M13 item).
- Layout: this folder is `XSCHEM_SHAREDIR` (`<CAD_ROOT>/assets/schematic`). The PDK `xschemrc` files append
  `${XSCHEM_SHAREDIR}/xschem_library/devices` and `${XSCHEM_SHAREDIR}/xschem_library` to the library path, so a
  symbol resolves as both `<name>` and `devices/<name>`.
- The files are generated: edit `scripts/gen_devlib.py` and run it; `scripts/gen_devlib.py --check` fails when
  a committed file differs.
