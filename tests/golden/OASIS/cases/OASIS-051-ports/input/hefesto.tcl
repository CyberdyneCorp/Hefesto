# OASIS-051-ports: Magic gds write against Hefesto oasis write. Layout ports (the MASK gds-write layouts).
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
probe {box 0 0 40 10}
probe {paint metal1}
probe {box 0 0 2 2}
probe {label p0 c metal1}
probe {port make 2}
probe {box 10 0 12 2}
probe {label p1 c metal1}
probe {port make 1}
probe {box 20 0 22 2}
probe {label p2 c metal1}
probe {port make 1}
probe {box 35 5 35 5}
probe {label plain c metal1}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
