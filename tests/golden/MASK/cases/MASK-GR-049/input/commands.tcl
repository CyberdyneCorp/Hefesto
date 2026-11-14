# MASK-GR-049: A readonly read of a two-level library, an empty structure, duplicates and a reference cycle.
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
probe {gds readonly yes}
probe {gds read in}
probe {cellname list allcells}
probe {cellname timestamp a}
probe {cellname timestamp b}
probe {cellname timestamp e}
probe {cellname timestamp cyc1}
probe {cellname timestamp cyc2}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
