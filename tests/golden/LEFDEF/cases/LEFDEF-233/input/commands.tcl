# LEFDEF-233: BLOCKAGES with LAYER met1 RECT, a PLACEMENT blockage, read with/without -noblockage. Captures: painted obstruction types.
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
probe {def read input/blk1}
probe {def read input/blk2 -noblockage}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
