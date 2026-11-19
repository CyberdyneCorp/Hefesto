# OASIS-007: CELLNAME tables: implicit only, explicit only, mixed (error), a duplicate refnum (error), a name defined twice and used by two CELLs, forward references, undefined refnums (#<n>).
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
probe {oasis datestamp 1}
probe {oasis read imp}
probe {oasis read exp}
probe {oasis read mixed}
probe {oasis read dupref}
probe {oasis read dupname}
probe {oasis read fwd}
probe {oasis read undef}
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
