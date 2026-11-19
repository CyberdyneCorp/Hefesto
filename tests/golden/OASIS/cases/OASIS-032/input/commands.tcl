# OASIS-032: oasis readonly yes on a library under PDK_ROOT (OASIS_FILE with $PDK_ROOT, OASIS_CELL, read-only, unmapped layer quiet); a unit-100 file refused in read-only mode.
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
probe {gds readonly yes}
probe {gds read $env(PDK_ROOT)/lib/cells}
specrep {$PDK_ROOT/lib/cells.oas}
specrep {sc_inv}
rep {cellname writeable sc_inv}
probe {gds read coarse}
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
