# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Alpha exit check, gf180mcuD (m4-scripting design D18; golden case DESKTOP-ALPHA-GF180MCU).
# Run with the PDK's rc file, trusted, from a directory holding the PDK tree:
#   PDK_ROOT=<root> hefesto-cli --trust-pdk gf180mcuD --magic -dnull -noconsole \
#       -rcfile <root>/gf180mcuD/libs.tech/magic/gf180mcuD.magicrc desktop_alpha_gf180mcu.tcl
# Magic 8.3.694 runs the same file with the same options (tools/golden/authoring/
# desktop_cases.py copies it into the case); the saved cells and `drc listall why` are
# compared. Each step prints [list <step> <code> <result>].

proc step {name script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $name $code $result]
    flush stdout
}

random seed 1
step tech {tech name}
load alpha
# A design from the standard-cell library (gf180mcu_fd_sc_mcu7t5v0, found through addpath).
box 0 0 0 0
step inv {getcell gf180mcu_fd_sc_mcu7t5v0__inv_1}
box 5um 0 5um 0
step nand {set moved [getcell gf180mcu_fd_sc_mcu7t5v0__nand2_1]}
# Edit: paint, label, move.
step paint {box 0um -3um 4um -2.5um; paint metal1}
step label {box 0um -2.75um 0um -2.75um; label VIN c metal1}
step move {select cell $moved; move e 1um; instance list abutment $moved}
# One generated device of each native family and one Tcl-only device.
step mosfet {box 20um 10um 20um 10um; magic::gencell gf180mcu::nfet_03v3 M1}
step fixed {box 40um 10um 40um 10um; magic::gencell gf180mcu::npn_10p00x10p00 Q1}
step tcl-only {box 60um 10um 60um 10um; magic::gencell gf180mcu::ppolyf_u R1}
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
