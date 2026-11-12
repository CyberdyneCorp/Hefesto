# SCRIPT-API-04: catch {load /nonexistent/libfoo.so} e; load ../lib.v2/inv; load cells/inv.mag.gz
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {load /nonexistent/libfoo.so}
probe {load input/lib.v2/inv}
probe {cellname list top}
probe {load input/cells/inv2.mag.gz}
probe {cellname list top}
probe {info commands tcl_load}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
