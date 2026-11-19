# OASIS-063: A disallowed abstract cell (the write fails, a file is left) and an undefined subcell with undefined allow (the write succeeds and references it): the second file's cells and references.
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
probe {load top}
probe {gds write out1.oas}
rep {file exists out1.oas}
probe {load top2}
probe {gds undefined allow}
probe {gds write out2.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
