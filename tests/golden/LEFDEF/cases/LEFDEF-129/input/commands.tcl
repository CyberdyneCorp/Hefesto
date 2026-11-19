# LEFDEF-129: `lef write -pinonly` and `-pinonly 10` on strip pins. Captures: PIN and OBS RECTs.
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
probe {load strip}
probe {lef write a -pinonly}
probe {lef write b -pinonly 10}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
