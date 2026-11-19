# LEFDEF-240: full OpenROAD-style small DEF (sky130 inverter chain) after tech LEF + cell LEF. Captures: complete written .mag of design cell.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {drc off}
probe {lef datestamp 100}
probe {lef read input/lef/sky130_fd_sc_hd__nom.tlef}
probe {lef read input/lef/sky130_fd_sc_hd.lef}
probe {def read input/inv_chain}
probe {writeall force inv_chain}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
