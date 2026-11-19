# OASIS-062: A hierarchy using a read-only OASIS cell with a repeated CIRCLE, a CTRAPEZOID, a PATH, an XGEOMETRY, a RECTANGLE array and a PLACEMENT repetition, written with library yes: the re-encoded cells have the vendor file's geometry (Q-OASIS-40).
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
probe {oasis readonly yes}
probe {oasis read vendor}
probe {load top}
probe {oasis library yes}
probe {oasis compress 0}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
