# MASK-OP-010: nfet + ndiff + pdc chain: bloat-all with and without clip 5, and the CIF-layer form.
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
probe {box 0 0 4 10}
probe {paint nfet}
probe {box 4 0 14 10}
probe {paint ndiff}
probe {box 14 0 18 4}
probe {paint pdc}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
