# MISC-PLOT-14: sky130A sky130_fd_sc_hd__inv_1 (pnm `default`, `draw ... no_color_at_all`); plot pnm inv (1000 pixels)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load sky130_fd_sc_hd__inv_1
probe {plot pnm inv}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
