# OASIS-020: The sky130 standard-cell library read by Hefesto from KLayout's strm2oas, by Magic from the KLayout strm2gds of that file, with default options.
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
probe {oasis datestamp 1700000000}
probe {oasis read input/sky130_fd_sc_hd.oas}
rep {llength [cellname list allcells]}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
