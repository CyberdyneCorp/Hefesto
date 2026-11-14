# MASK-OP-021: Regions touching, overlapping and diagonal to metal2: all four forms, paint and templayer.
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
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 20 0 30 10}
probe {paint metal1}
probe {box 40 0 50 10}
probe {paint metal1}
probe {box 60 0 70 10}
probe {paint metal1}
probe {box 10 0 14 10}
probe {paint metal2}
probe {box 25 5 35 15}
probe {paint metal2}
probe {box 50 10 54 14}
probe {paint metal2}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
