# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Helpers of the Desktop Beta layout scripts (tests/e2e/beta/<pdk>/beta_inv.tcl). Coordinates are
# in microns. Only existing commands are used: box, paint, label, port, drc, gds, save.

# Paint `layer` over the rectangle (x1, y1)-(x2, y2), in microns.
proc rect {layer x1 y1 x2 y2} {
    box ${x1}um ${y1}um ${x2}um ${y2}um
    paint $layer
}

# A point label on `layer` at (x, y) made a port of the given use and class.
proc pin {name layer x y use class} {
    box ${x}um ${y}um ${x}um ${y}um
    label $name c $layer
    port make
    port use $use
    port class $class
}

# DRC of the whole cell, then the GDS file and the .mag file; prints the results for run_beta.py.
proc finish {cell} {
    select top cell
    drc check
    drc catchup
    puts "BETA drc [drc list count total]"
    puts "BETA why [drc listall why]"
    gds write $cell.gds
    puts "BETA gds [file normalize $cell.gds]"
    save $cell
    quit -noprompt
}
