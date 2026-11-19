# OASIS-050: The reference case of oasis-write "Complete annotated example": top with metal1 (0,0)-(4,2), layer M1 on 49/0, compress 0: the 419-byte file.
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
probe {oasis compress 0}
probe {oasis write out.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
