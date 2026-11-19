# OASIS-053: A contact array (squares on a 20x12 pdc area) and two irregular rows, repetitions yes: the cuts written as repeated RECTANGLEs, the same geometry as Magic's GDS.
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
probe {box 0 0 20 12}
probe {paint pdc}
probe {box 30 0 36 4}
probe {paint pdc}
probe {box 30 8 40 12}
probe {paint pdc}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
