# LEFDEF-006: LEF producing more than 100 errors (unknown layers). Captures: stdout/stderr transcript (cap notice and totals).
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
probe {lef read input/errs.lef}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
