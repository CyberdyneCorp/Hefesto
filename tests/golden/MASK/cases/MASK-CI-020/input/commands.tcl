# MASK-CI-020: CIF DS 1 1 2 with odd coordinates under cif limit 1 and cif limit 100.
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
probe {cif limit 1}
probe {cif read in}
probe {cellname list allcells}
probe {cif limit 100}
probe {cif limit}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
