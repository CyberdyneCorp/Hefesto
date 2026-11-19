# OASIS-054: Equal 2x2 squares on metal1 (two rows) and metal2 (one column): repetition grouping and the fields modal omission leaves out (layer, datatype, width, height, x, y, repetition).
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
probe {box 0 0 2 2}
probe {paint metal1}
probe {box 3 0 5 2}
probe {paint metal1}
probe {box 7 0 9 2}
probe {paint metal1}
probe {box 0 5 2 7}
probe {paint metal1}
probe {box 3 5 5 7}
probe {paint metal1}
probe {box 20 0 22 2}
probe {paint metal2}
probe {box 20 4 22 6}
probe {paint metal2}
probe {oasis compress 0}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
