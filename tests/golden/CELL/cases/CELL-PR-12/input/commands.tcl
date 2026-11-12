# CELL-PR-12: relative use path ../b from input/a, then Tcl cd ../b: cellname filepath before and after.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
cd input/a
load top
probe {cellname list filepath top}
probe {cellname list filepath leaf}
cd ../b
probe {cellname list filepath top}
probe {cellname list filepath leaf}
probe {pwd}
cd ../..
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
