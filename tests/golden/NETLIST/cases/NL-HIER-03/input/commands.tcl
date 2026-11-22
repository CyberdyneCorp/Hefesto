# NL-HIER-03: an abstract cell (blackbox off, then on), a primitive cell and a wire-only cell: stubs and omissions
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
probe {ext2spice top}
file rename -force top.spice top.bboff.spice
probe {ext2spice blackbox on}
probe {ext2spice top}
file rename -force top.spice top.bbon.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
