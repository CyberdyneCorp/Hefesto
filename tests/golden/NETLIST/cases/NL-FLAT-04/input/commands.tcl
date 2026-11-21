# NL-FLAT-04: one device of each class the reader parses (mosfet, asymmetric, fet, bjt, devres None and modeled, devcap None and modeled, devcaprev, diode, pdiode, ndiode, subckt, rsubckt, msubckt, csubckt, dsubckt, veriloga); ext2spice: the full netlist
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
