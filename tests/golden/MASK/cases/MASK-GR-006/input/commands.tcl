# MASK-GR-006: BGNSTR dates of year 124, a 12-byte BGNSTR; datestamp yes/no/1700000000 and nodatestamp yes.
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
probe {gds datestamp yes}
probe {cellname list allcells}
probe {gds read in0}
probe {gds datestamp no}
probe {gds read in1}
probe {gds datestamp 1700000000}
probe {gds read in2}
probe {gds datestamp yes}
probe {gds nodatestamp yes}
probe {gds read in3}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
