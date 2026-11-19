# LEFDEF-104: Cell with negative lower-left bbox (-20,-10). Captures: ORIGIN/SIZE lines.
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
probe {load neg}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
