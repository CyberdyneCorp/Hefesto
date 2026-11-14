# MASK-OP-034: 2-wide and 2-high wires with solid neighbours: bridge-lim 4 6 L with and without a limiting tile.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {load top}
probe {box 0 0 2 20}
probe {paint metal1}
probe {box 10 0 30 2}
probe {paint metal1}
probe {box 10 4 30 10}
probe {paint metal1}
probe {gds write a}
probe {box 3 0 5 20}
probe {paint metal2}
probe {gds write b}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
