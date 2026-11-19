# LEFDEF-132: `lef write -hide` with OBS_BBOX property valid and malformed. Captures: OBS cover extent; error text.
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
probe {load obsok}
probe {lef write -hide}
probe {load obsbad}
probe {lef write -hide}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
