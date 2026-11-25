# MISC-ANT-09: lef write of a macro whose input pin drives two 2x10 nfet gates and whose output pin reaches an ndiff tie-down: ANTENNAGATEAREA and ANTENNADIFFAREA from the antenna check's gate and tie-down sets
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/anttest -noprompt
drc off
probe {snap internal}
load top
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
