# MASK-RT-sg13g2_sram-f20: Library round trip of ihp-sg13g2 sg13g2_sram, GDS file 20 of 30 (sorted by name): gds read, a wrapper placing every top structure with its timestamp pinned to 0, gds library yes, gds write.
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
rt::run_library input/lib 30 19 out
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
