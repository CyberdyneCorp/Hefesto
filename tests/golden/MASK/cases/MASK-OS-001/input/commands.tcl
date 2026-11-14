# MASK-OS-001: Styles a and b variant x,y: the registered names and their order.
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
probe {cif listall ostyle}
probe {cif list ostyle}
probe {cif ostyle}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
