# EX-057: style exttest variants (),(si) with variants-conditional areacap lines: extract in exttest(), then in exttest(si)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load ex
box 0 0 1 1
probe {extract style}
probe {extract all}
file rename -force input/ex.ext input/ex.default.ext
probe {extract style exttest(si)}
probe {extract style}
probe {extract all}
file rename -force input/ex.ext input/ex.si.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
