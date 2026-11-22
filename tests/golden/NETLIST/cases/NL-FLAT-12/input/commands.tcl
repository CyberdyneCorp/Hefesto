# NL-FLAT-12: two instances each with a vdd! node, not connected: global on without and with the Tcl array globals(vdd!), then global off: the node names and the not-fully-connected text
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice top}
file rename -force top.spice top.globalon.spice
set globals(vdd!) 1
probe {ext2spice top}
file rename -force top.spice top.globalsarray.spice
probe {ext2spice global off}
probe {ext2spice top}
file rename -force top.spice top.globaloff.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
