# LEFDEF-142: `lef nocheck` listing with empty and non-empty list. Captures: printed/returned list.
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
probe {lef nocheck}
probe {lef nocheck A B}
probe {lef nocheck}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
