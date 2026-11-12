# SCRIPT-RSP-01: inv with ports Y=1 A=2 VDD=3 VSS=4; readspice inv.spice with .subckt inv A Y VDD VSS
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
