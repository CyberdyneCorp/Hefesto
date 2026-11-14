# MASK-OP-036: scalefactor 10, a mask-layer trigger [-17,23]x[0,10]: bloat-all M ndiff and bloat-all M ndiff 5.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {load top}
probe {box -2 0 3 1}
probe {paint metal1}
probe {box -10 0 10 4}
probe {paint ndiff}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
