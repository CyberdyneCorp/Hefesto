# DESKTOP-ALPHA-SG13G2: Desktop Alpha exit check on ihp-sg13g2 (design D18).
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Alpha exit check, ihp-sg13g2 (m4-scripting design D18; golden case DESKTOP-ALPHA-SG13G2).
# Run with the PDK's rc file, trusted, from a directory holding the PDK tree:
#   PDK_ROOT=<root> hefesto-cli --trust-pdk ihp-sg13g2 --magic -dnull -noconsole \
#       -rcfile <root>/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.magicrc desktop_alpha_sg13g2.tcl
# Magic 8.3.694 runs the same file with the same options (tools/golden/authoring/
# desktop_cases.py copies it into the case); the saved cells and `drc listall why` are
# compared. Each step prints [list <step> <code> <result>].
#
# The pinned IHP data set (scripts/fetch_pdk.py ihp) holds libs.tech/magic only, so the
# design is built from generated devices instead of standard cells, and the sg13g2 fixed
# devices wait for SCRIPT-PDK-14 (design D10): the native family here is the MOSFET.

proc step {name script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $name $code $result]
    flush stdout
}

random seed 1
step tech {tech name}
load alpha
# A design: two generated MOSFETs stand in for library cells (see above).
box 0 0 0 0
step inv {magic::gencell sg13g2::sg13_lv_nmos N0}
box 5um 0 5um 0
step nand {magic::gencell sg13g2::sg13_lv_pmos P0; set moved P0}
# Edit: paint, label, move.
step paint {box 0um -3um 4um -2.5um; paint metal1}
step label {box 0um -2.75um 0um -2.75um; label VIN c metal1}
step move {select cell $moved; move e 1um; instance list abutment $moved}
# One generated device of each native family and one Tcl-only device.
step mosfet {box 20um 10um 20um 10um; magic::gencell sg13g2::sg13_lv_nmos M1}
step tcl-only {box 40um 10um 40um 10um; magic::gencell sg13g2::rsil R1}
step tcl-only-2 {box 60um 10um 60um 10um; magic::gencell sg13g2::cap_cmim C1}
step children {lsort [cellname list children alpha]}
# DRC.
step drc {select top cell; drc check; drc catchup}
step why {drc listall why}
step count {drc list count total}
# Save the design and every generated cell: all modified cells (the library cells are not).
select top cell
step save {writeall force}
step saved {lsort [glob -nocomplain *.mag]}
quit -noprompt
