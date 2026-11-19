# OASIS-051-labels: Magic gds write against Hefesto oasis write. Layout labels (the MASK gds-write layouts).
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
probe {box 0 0 100 100}
probe {paint metal1}
probe {box 5 5 5 5}
probe {label plain c metal1}
probe {box 10 10 10 10}
probe {label big FreeSans 80 90 0 0 n metal1}
probe {box 60 60 80 80}
probe {label area c metal1}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
