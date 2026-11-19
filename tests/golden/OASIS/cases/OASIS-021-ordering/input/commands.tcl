# OASIS-021-ordering: The sky130 standard-cell library read with ordering (OASIS-020's inputs).
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
proc rep {script} {
    set code [catch {uplevel #0 $script} result]
    set f [open report.txt a]
    puts $f [list $code $result]
    close $f
}
proc specrep {value} {
    set f [open report.txt a]
    puts $f [list 0 $value]
    close $f
}
probe {drc off}
probe {gds datestamp 1}
probe {gds ordering on}
probe {gds read input/sky130_fd_sc_hd.gds.gz}
rep {llength [cellname list allcells]}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
