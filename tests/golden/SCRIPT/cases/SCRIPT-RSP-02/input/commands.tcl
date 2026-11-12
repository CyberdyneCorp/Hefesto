# SCRIPT-RSP-02: inv already ordered; plus an extra BODY port
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load inv
probe {readspice input/inv.spice}
foreach l {A Y VDD VSS BODY} { probe [list port $l index] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
