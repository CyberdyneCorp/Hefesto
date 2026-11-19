# LEFDEF-135: `lef write -hide 60` on 100x200 cell. Captures: equality with plain write.
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
probe {load inv}
probe {lef write a -hide 60}
probe {lef write b}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
