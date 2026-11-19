# LEFDEF-137: `lef write -nomaster` vs default on cell with nwell. Captures: presence of nwell geometry/cover.
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
probe {load tl}
probe {lef write a -nomaster}
probe {lef write b}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
