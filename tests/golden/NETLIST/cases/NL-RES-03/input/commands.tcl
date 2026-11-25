# NL-RES-03: lumped on with mindelay 0 and with mindelay 1: the .res.lump values
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/restest -noprompt
drc off
load wire
box 0 0 1 1
probe {extract all}
probe {extresist lumped on}
probe {extresist mindelay 0}
probe {extresist}
file rename -force wire.res.lump wire.d0.res.lump
probe {extresist mindelay 1}
probe {extresist}
file rename -force wire.res.lump wire.d1.res.lump
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
