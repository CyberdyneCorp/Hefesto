# MISC-MH-06: cifinput templayers SRAMH (81/2) and SRAMH2 (81/3), each with mask-hints SRAM: MASKHINTS_SRAM after a GDS read (Q-MISC-11: Magic keeps only the second region, Hefesto the union).
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
probe {gds read in}
probe {load top}
probe {property MASKHINTS_SRAM}
probe {property}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
