# MISC-MH-03: property compat false: a cell with MASKHINTS_SRAM saved (a dimension line), flushed and read back; the hint queried; gds write with a mask-hints SRAM layer (Q-MISC-10).
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
probe {load a}
probe {box 0 0 4 4}
probe {paint metal1}
probe {property MASKHINTS_SRAM "0 0 10 10"}
probe {property compat false}
probe {save a}
probe {flush a}
probe {property MASKHINTS_SRAM}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
