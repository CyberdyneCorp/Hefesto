# SCRIPT-RSP-03: reg with ports D<0>, d<1> and bare label CLK; netlist pins D[0] D[1] CLK
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load reg
probe {readspice input/reg.spice}
foreach l {D<0> d<1> CLK} { probe [list port $l index] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
