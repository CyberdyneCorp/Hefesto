# MASK-OS-016: Contact type via1 with squares 10 20 20; lef read of a VIA whose via1 cut (0.1 um) differs from the contact's (cut-size warnings, the cut grown to the contact size), then lef write -tech of a via1 contact cell.
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
probe {load cont}
probe {lef read input/v.lef}
probe {box 0 0 4 4}
probe {paint via1}
probe {lef write cont -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
