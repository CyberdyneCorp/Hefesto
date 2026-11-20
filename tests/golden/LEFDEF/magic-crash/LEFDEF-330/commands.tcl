# LEFDEF-330: -anal with metal1..metal3 and one generated rule. Captures: NONDEFAULTRULES lines.
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
probe {load top}
probe {def write -anal}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
