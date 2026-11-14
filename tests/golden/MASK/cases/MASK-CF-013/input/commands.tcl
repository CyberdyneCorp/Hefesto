# MASK-CF-013: A file producing more than 150 errors under cif warning limit and none.
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
probe {cif warning limit 5}
probe {cif read in}
probe {load top2}
probe {cif warning none}
probe {cif read in}
probe {cif warning}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
