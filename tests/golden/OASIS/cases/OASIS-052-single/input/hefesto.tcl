# OASIS-052-single: repetitions no: every BOUNDARY written as its own RECTANGLE or POLYGON, in gds-write's element order (stream-elements). Layout single (the MASK gds-write layouts).
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
probe {box 0 0 10 4}
probe {paint metal1}
probe {box 0 6 10 10}
probe {paint metal2}
probe {oasis repetitions no}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
