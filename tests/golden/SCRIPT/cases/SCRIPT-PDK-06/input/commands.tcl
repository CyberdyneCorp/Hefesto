# SCRIPT-PDK-06: (a) set PDKPATH /pdk/sky130A; (b) start with the sky130A rc file and PDK_ROOT=/pdk; in each, place sky130_fd_sc_hd__inv_1 from /pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag and save the parent
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load parent
box 0 0 0 0
probe {getcell sky130_fd_sc_hd__inv_1}
probe {save parent}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
