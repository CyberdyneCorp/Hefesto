# MASK-GW-007: gds lower no with a mixed-case cell and labels.
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
probe {load MixedTop}
probe {box 0 0 4 4}
probe {paint metal1}
probe {box 1 1 1 1}
probe {label MixedCase c metal1}
probe {gds lower}
probe {gds lower no}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
