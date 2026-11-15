# MASK-RT-gf180mcu_fd_ip_sram-f03: Library round trip of gf180mcuD gf180mcu_fd_ip_sram, GDS file 3 of 4 (sorted by name): gds read, a wrapper placing every top structure with its timestamp pinned to 0, gds library yes, gds write.
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
rt::run_library input/lib 4 2 out
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
