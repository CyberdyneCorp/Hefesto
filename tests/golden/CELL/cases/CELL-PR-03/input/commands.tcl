# CELL-PR-03: use path $PDK_ROOT/lib with the Tcl variable PDK_ROOT set (top1), then unset (top2).
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
catch {unset env(PDK_ROOT)}
set PDK_ROOT [pwd]/input
load top1
probe {cellname list filepath leaf1}
probe {cellname list flags leaf1}
unset PDK_ROOT
load top2
probe {cellname list filepath leaf2}
probe {cellname list flags leaf2}
probe {cellname list allcells}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
