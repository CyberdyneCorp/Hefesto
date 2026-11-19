# LEFDEF-119: Same, with `lef nocheck A`, then `lef nocheck *`, then `lef nocheck none`. Captures: antenna lines presence.
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
probe {load ant}
probe {lef nocheck A}
probe {lef write a}
probe {lef nocheck *}
probe {lef write b}
probe {lef nocheck none}
probe {lef write c}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
