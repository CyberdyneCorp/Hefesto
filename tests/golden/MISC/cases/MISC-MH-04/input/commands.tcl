# MISC-MH-04: A cifoutput layer SRAMID with only mask-hints SRAM; a cell with MASKHINTS_SRAM 0 0 10 10 and no paint; gds write.
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
probe {property MASKHINTS_SRAM "0 0 10 10"}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
