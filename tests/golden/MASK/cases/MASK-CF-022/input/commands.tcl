# MASK-CF-022: cif scale in, cif scale out and cif lambda out.
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
probe {cif scale in}
probe {cif scale out}
probe {cif lambda in}
probe {cif lambda out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
