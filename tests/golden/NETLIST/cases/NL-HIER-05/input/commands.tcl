# NL-HIER-05: a 1-D and a 2-D array of a cell with ports; renumber off and on: the X lines
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice hierarchy on}
probe {ext2spice renumber off}
probe {ext2spice top}
file rename -force top.spice top.renumoff.spice
probe {ext2spice renumber on}
probe {ext2spice top}
file rename -force top.spice top.renumon.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
