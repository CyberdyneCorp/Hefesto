# OASIS-051-mergeyes: Magic gds write against Hefesto oasis write. Layout mergeyes (the MASK gds-write layouts).
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
probe {box 0 0 4 2}
probe {paint metal1}
probe {box 0 2 2 4}
probe {paint metal1}
probe {oasis merge yes}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
