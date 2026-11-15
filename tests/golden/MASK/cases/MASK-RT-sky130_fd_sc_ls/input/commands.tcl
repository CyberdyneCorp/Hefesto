# MASK-RT-sky130_fd_sc_ls: Library round trip of sky130A sky130_fd_sc_ls, its GDS file: gds read, a wrapper placing every top structure with its timestamp pinned to 0, gds library yes, gds write.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {drc off}
source input/gds_rt.tcl
rt::run_library input/lib 1 0 out
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
