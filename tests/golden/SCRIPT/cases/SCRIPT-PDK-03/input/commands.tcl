# SCRIPT-PDK-03: command line a.gds b.lef c.def d.cif e.tcl f.mag g g.mag.gz h.xyz -foo in wrapper mode and NULL mode
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {set argv}
probe {cellname list allcells}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
