# SCRIPT-BOX-03: top with children a, b: pushstack a; pushstack b; popstack; popstack; popstack
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {pushstack a_0}
probe {cellname list window}
probe {view bbox}
probe {set editstack}
probe {pushstack b_0}
probe {cellname list window}
probe {popstack}
probe {cellname list window}
probe {popstack}
probe {cellname list window}
probe {popstack}
probe {set editstack}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
