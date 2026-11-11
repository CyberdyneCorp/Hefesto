# SCRIPT-AUX-01: run strip_reflibs.tcl in a directory with 2 .mag files containing GDS_START, GDS_END and GDS_FILE lines
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
cd input/lib
probe {source $CAD_ROOT/magic/tcl/strip_reflibs.tcl}
cd ../..
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
