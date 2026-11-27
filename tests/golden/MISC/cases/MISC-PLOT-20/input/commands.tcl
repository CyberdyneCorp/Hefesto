# MISC-PLOT-20: tech plot section errors: a line before `style`, a three-field style, an unknown style, a four-field ps line, a non-integer second field, a bad third field; a well-formed versatec block (2- and 17-field lines) and a gremlin block
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plotbad -noprompt
probe {tech name}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
