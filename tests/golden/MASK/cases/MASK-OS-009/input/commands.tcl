# MASK-OS-009: labels with none/port/noport/text: a plain label and a port label on metal1.
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
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 2 2 2 2}
probe {label plain c metal1}
probe {box 5 5 8 8}
probe {label pin c metal1}
probe {port make}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
