# OASIS-006: END records of 255, 256 and 257 bytes; data after END; validation schemes 0, 1, 2 and 3; a wrong CRC32. Every file's cell is still read.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
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
probe {tech load input/oastest -noprompt}
probe {snap internal}
probe {gds datestamp 1}
probe {gds read e255}
probe {gds read e257}
probe {gds read val0}
probe {gds read val1}
probe {gds read val2}
probe {gds read val3}
probe {gds read bad}
probe {gds read trail}
probe {load anchor}
probe {box 0 0 1 1}
probe {paint metal1}
rep {lsort [cellname list allcells]}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
