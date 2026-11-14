# MASK-GW-038: gds write ./dir/top, gds write top and a bare gds: the files created.
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
probe {box 0 0 4 2}
probe {paint metal1}
probe {file mkdir dir}
probe {gds write ./dir/top}
probe {gds write top}
probe {gds}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
