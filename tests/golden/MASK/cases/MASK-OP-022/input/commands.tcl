# MASK-OP-022: A hierarchy with labelled net VDD: net VDD metal1,metal2 and net VDD.
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
probe {getcell child}
probe {box 10 0 20 4}
probe {paint metal1}
probe {box 18 0 20 4}
probe {paint via1}
probe {box 18 0 30 4}
probe {paint metal2}
probe {box 12 2 12 2}
probe {label VDD c metal1}
probe {box 0 20 10 24}
probe {paint metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
