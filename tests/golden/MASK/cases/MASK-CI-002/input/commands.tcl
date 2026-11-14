# MASK-CI-002: variant (c) / variant * blocks: x(p) then x(c) read the same GDS.
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
probe {cif istyle x(p)}
probe {gds read in}
probe {cellname list allcells}
probe {cif istyle x(c)}
probe {gds read in2}
probe {cellname list allcells}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
