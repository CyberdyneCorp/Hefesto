# EX-069: sidewall metal1 space space metal1 40, sidehalo 4, extract no fringehalo; two wires 6 apart: no cap with the style's halo, the cap after extract halo 8, none again after extract style exttest reloads the style
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
probe {extract no fringehalo}
probe {extract halo}
probe {extract all}
file rename -force input/ex.ext input/ex.default.ext
probe {extract halo 8}
probe {extract halo}
probe {extract all}
file rename -force input/ex.ext input/ex.halo8.ext
probe {extract style exttest}
probe {extract no fringehalo}
probe {extract halo}
probe {extract all}
file rename -force input/ex.ext input/ex.style.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
