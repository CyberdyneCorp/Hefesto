# NL-FLAT-15: node w with area/perimeter in the poly and metal1 classes; ext2sim rthresh 0: the R line (25100 mOhm)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2sim rthresh 0}
probe {ext2sim top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
