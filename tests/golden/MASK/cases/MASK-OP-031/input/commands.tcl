# MASK-OP-031: SE-catecorner pair with different bottom neighbours: bridge 5 4 and bridge-lim 5 4 metal2.
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
probe {box 0 10 10 20}
probe {paint metal1}
probe {box 12 0 22 8}
probe {paint metal1}
probe {box 0 0 10 8}
probe {paint metal2}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
