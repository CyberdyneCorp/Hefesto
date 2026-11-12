# SCRIPT-PCELL-16: pure Tcl: magic::normalize_value on the sky130 nfet default compatible list; magic::get_gencell_hash on the full sky130 nfet default dictionary
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
set d [sky130::sky130_fd_pr__nfet_01v8_defaults]
probe {magic::normalize_value [dict get $d compatible]}
probe {magic::get_gencell_hash $d}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
