# MASK-OS-011: options with each flag and an unknown word; set-minimum-grid with gridlimit: lambda after load.
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
probe {cif scale out}
probe {cif lambda out}
probe {tech lambda}
probe {box 0 0 3 3}
probe {paint metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
