# OASIS-033: One library read read-only from a plain file and from a CBLOCK-compressed copy with other names: equal stamps across files, a parent's stamp differs from its child's, and an edit and save keep the stamp.
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
probe {gds readonly yes}
probe {gds read plain}
probe {gds read packed}
rep {expr {[cellname timestamp va] == [cellname timestamp wa]}}
rep {expr {[cellname timestamp vt] == [cellname timestamp wt]}}
rep {expr {[cellname timestamp vt] != [cellname timestamp va]}}
set before [cellname timestamp vt]
probe {load vt}
probe {box 40 0 50 10}
probe {paint metal1}
probe {save vt}
rep {expr {$before == [cellname timestamp vt]}}
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
