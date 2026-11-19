# LEFDEF-116: Pins with every direction/use/shape value incl. reset/scan/tieoff. Captures: PIN header lines.
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
probe {load pins}
probe {lef write pins}
probe {def read input/resetpins}
probe {lef write resetpins}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
