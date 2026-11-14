# MASK-CI-025: Tech load errors: an operator before any layer, grow 0 and an unknown statement.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {load top}
probe {cif listall istyle}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
