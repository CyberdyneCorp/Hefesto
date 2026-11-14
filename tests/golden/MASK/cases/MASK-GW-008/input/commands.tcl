# MASK-GW-008: gds datestamp 1700000000, yes, no and nodatestamp yes, a cell stamped 1600000000; TZ UTC and New York.
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
probe {gds datestamp 1700000000}
probe {gds write fixed}
probe {gds datestamp no}
probe {gds write zero}
probe {gds nodatestamp yes}
probe {gds write nostamp}
probe {cellname timestamp top}
probe {gds datestamp yes}
probe {gds write default}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
