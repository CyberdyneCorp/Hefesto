# MASK-CF-023: cif limit, cif limit 500, cif limit 0 and cif rescale no.
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
probe {cif limit}
probe {cif limit 500}
probe {cif limit}
probe {cif limit 0}
probe {cif limit}
probe {cif rescale}
probe {cif rescale no}
probe {cif rescale}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
