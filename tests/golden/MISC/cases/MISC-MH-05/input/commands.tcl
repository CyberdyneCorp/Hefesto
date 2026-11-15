# MISC-MH-05: gds maskhints MET1; a GDS MET1 polygon (0,0)-(600,400) nm whose metal1 reads back as (0,0)-(4,4) only (and M1A); MASKHINTS_MET1 and the GDS written again.
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
probe {gds maskhints MET1}
probe {gds maskhints}
probe {gds read in}
probe {load top}
probe {property MASKHINTS_MET1}
probe {gds write out}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
