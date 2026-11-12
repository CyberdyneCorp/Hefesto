# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Alpha exit check, sky130A (m4-scripting design D18; golden case DESKTOP-ALPHA-SKY130).
# Run with the PDK's rc file, trusted, from a directory holding the PDK tree:
#   PDK_ROOT=<root> hefesto-cli --trust-pdk sky130A --magic -dnull -noconsole \
#       -rcfile <root>/sky130A/libs.tech/magic/sky130A.magicrc desktop_alpha_sky130.tcl
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
# A design from the standard-cell library (sky130_fd_sc_hd, found through the rc file's addpath).
box 0 0 0 0
step inv {getcell sky130_fd_sc_hd__inv_1}
box 5um 0 5um 0
step nand {set moved [getcell sky130_fd_sc_hd__nand2_1]}
# Edit: paint, label, move.
step paint {box 0um -3um 4um -2.5um; paint met1}
step label {box 0um -2.75um 0um -2.75um; label VIN c met1}
step move {select cell $moved; move e 1um; instance list abutment $moved}
# One generated device of each native family and one Tcl-only device.
step mosfet {box 20um 10um 20um 10um; magic::gencell sky130::sky130_fd_pr__nfet_01v8 M1}
step fixed {box 40um 10um 40um 10um; magic::gencell sky130::sky130_fd_pr__npn_05v5_W1p00L1p00 Q1}
step tcl-only {box 60um 10um 60um 10um; magic::gencell sky130::sky130_fd_pr__res_generic_po R1}
step children {lsort [cellname list children alpha]}
# DRC.
step drc {select top cell; drc check; drc catchup}
step why {drc listall why}
step count {drc list count total}
# Save the design and every generated cell: the modified cells only. `writeall force`
# would also write the unmodified library cells (Magic does so for any writable file).
select top cell
step save {writeall modified}
step saved {lsort [glob -nocomplain *.mag]}
quit -noprompt
