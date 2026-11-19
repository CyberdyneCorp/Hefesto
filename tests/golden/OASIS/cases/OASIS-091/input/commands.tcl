# OASIS-091: gds flatten yes, then oasis read of a file with a small placed cell: the cell is flattened; gds readonly true, then oasis read: the cells are read-only, and oasis flatten and oasis readonly report the gds values (Q-OASIS-90 (a)).
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
probe {gds flatten yes}
probe {gds read flat}
rep {gds flatten}
probe {gds readonly true}
probe {gds read vend}
rep {gds readonly}
rep {cellname writeable ro}
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
