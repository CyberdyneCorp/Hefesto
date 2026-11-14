# MASK-OP-011: A contact guard ring, strip shapes and a contact exactly one cut wide: the cuts.
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
probe {box 0 0 20 4}
probe {paint pdc}
probe {box 0 16 20 20}
probe {paint pdc}
probe {box 0 4 4 16}
probe {paint pdc}
probe {box 16 4 20 16}
probe {paint pdc}
probe {box 30 0 34 40}
probe {paint pdc}
probe {box 40 0 44 4}
probe {paint pdc}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
