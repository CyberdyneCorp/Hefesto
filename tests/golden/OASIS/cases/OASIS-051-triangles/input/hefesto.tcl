# OASIS-051-triangles: Magic gds write against Hefesto oasis write. Layout triangles (the MASK gds-write layouts).
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
probe {box 0 0 8 8}
probe {splitpaint ne metal1}
probe {box 10 0 18 8}
probe {splitpaint nw metal1}
probe {box 20 0 28 8}
probe {splitpaint se metal1}
probe {box 30 0 38 8}
probe {splitpaint sw metal1}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
