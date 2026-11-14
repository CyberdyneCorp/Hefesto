# MASK-HG-005: Parent metal (104,40)-(130,60) beside a child (0,0)-(100,100) near the origin, and moved far away.
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
probe {box 0 0 140 100}
probe {cif *hier M1X}
probe {feedback save fb_near.txt}
probe {feedback clear}
probe {gds write near}
probe {select cell c0}
probe {move 1000 0}
probe {box 1000 0 1140 100}
probe {cif *hier M1X}
probe {feedback save fb_far.txt}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
