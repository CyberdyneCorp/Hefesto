# LEFDEF-100: Minimal tech (scmos-like) with lef section; cell `inv` (edit root) with ports A(0,in,signal), Y(1,out), metal1 obstruction; `lef write`. Captures: inv.lef bytes.
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
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
