# LEFDEF-333: floating labeled metal1 net (special); obsm1, obsm2 and obsv1 obstruction paint. Captures: SPECIALNETS; BLOCKAGES per route and via bucket.
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
probe {def write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
