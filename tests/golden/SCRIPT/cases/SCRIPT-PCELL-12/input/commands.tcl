# SCRIPT-PCELL-12: magic::netlist_to_layout top.spice toy, where top.spice has 2 subckts (one existing on disk), ideal C/R/L elements, bus pins vout[0], continuation lines, .command block, M=2 subcircuit call
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
source input/toy.tcl
probe {magic::netlist_to_layout input/top.spice toy}
probe {cellname list allcells}
foreach c [lsort [cellname list allcells]] { load $c; select top cell; probe {port first}; probe {port last}; probe {box values} }
probe {save sub1}
probe {save top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
