# LEFDEF-336: LEFDEF-317's layout (vdd! without a port, port A, labeled internal clkint) extracted by extract all, then def write: SPECIALNETS / NETS membership and counts from the tool's own .ext
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
box 0 0 1 1
probe {extract all}
probe {def write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
