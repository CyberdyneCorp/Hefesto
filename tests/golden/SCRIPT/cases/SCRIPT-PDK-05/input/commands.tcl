# SCRIPT-PDK-05: empty session, PDK_ROOT=/pdk (env) then as a Tcl var only; load inv from a sky130A file
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {tech name}
load input/pdk/sky130A/libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__inv_1
probe {tech name}
probe {cellname list filepath sky130_fd_sc_hd__inv_1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
