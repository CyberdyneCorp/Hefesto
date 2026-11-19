# OASIS-090: oasis nodup t, oasis flatten 5 and their queries (as gds), oasis compress 12 and circlepoints 4 (refused, values kept), oasis write a b (nothing written) and a bare oasis (top.oas).
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
probe {oasis nodup t}
rep {oasis noduplicates}
probe {oasis flatten 5}
rep {oasis flatten}
probe {oasis compress 12}
rep {oasis compress}
probe {oasis circlepoints 4}
rep {oasis circlepoints}
probe {oasis write a b}
rep {expr {[file exists a] || [file exists a.oas]}}
probe {oasis}
rep {file exists top.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
