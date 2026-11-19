# LEFDEF-103: Cell `inv` instance selected inside edit root `top`; `lef write`. Captures: inv.lef (no header).
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
probe {box 150 100 150 100}
probe {select cell inv_0}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
