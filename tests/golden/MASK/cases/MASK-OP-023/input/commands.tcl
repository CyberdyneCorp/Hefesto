# MASK-OP-023: Labels A on regions: tagged A and tagged A metal1 at scale 1 and 10.
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
probe {box 0 20 10 30}
probe {paint metal2}
probe {box 5 5 5 5}
probe {label A c metal1}
probe {box 5 25 5 25}
probe {label A c metal2}
probe {cif ostyle s1}
probe {gds write s1}
probe {cif ostyle s10}
probe {gds write s10}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
