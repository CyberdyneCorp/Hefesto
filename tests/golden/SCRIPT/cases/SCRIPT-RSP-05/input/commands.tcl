# SCRIPT-RSP-05: Spectre inline subckt inv (A Y VDD VSS); continuation after a comment line; tabs and double spaces; an unloaded cell; a parameter W=1 after the pins
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load inv
probe {readspice input/inv.scs}
foreach l {A Y VDD VSS BODY} { probe [list port $l index] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
