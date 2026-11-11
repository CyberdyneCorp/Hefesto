# SCRIPT-RSP-04: readspice cells with only cells.cdl present, containing *.PININFO A:I Y:O VDD:B VSS:B and a pin already classed output
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load inv
port Y class output
cd input
probe {readspice cells}
cd ..
foreach l {A Y VDD VSS} { probe [list port $l class] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
