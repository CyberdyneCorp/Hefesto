# LEFDEF-315: port labels with every class/use combination incl. default, reset, tieoff. Captures: PINS section.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {def read input/p}
probe {def write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
