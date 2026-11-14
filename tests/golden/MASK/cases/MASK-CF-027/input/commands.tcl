# MASK-CF-027: cif see CMF, cif paint CMF metal1 newcell, cif coverage CMF box, cif list coverage, cif statistics.
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
probe {load top}
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 20 0 30 4}
probe {paint metal1}
probe {box 0 0 30 10}
probe {cif see CMF}
probe {feedback save fb.txt}
probe {cif paint CMF metal2 newcell}
probe {cif coverage CMF box}
probe {cif coverage CMF}
probe {cif list coverage CMF}
probe {cif statistics}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
