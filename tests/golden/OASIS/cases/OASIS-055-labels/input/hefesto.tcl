# OASIS-055-labels: Labels A<0>, a%b&c and one with a control character under calma-permissive-labels: TEXTSTRING strings (Q-OASIS-60).
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
probe {box 0 0 40 40}
probe {paint metal1}
probe {box 2 2 2 2}
probe {label A<0> c metal1}
probe {box 4 4 4 4}
probe {label a%b&c c metal1}
probe {box 6 6 6 6}
probe [list label tab\x01x c metal1]
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
