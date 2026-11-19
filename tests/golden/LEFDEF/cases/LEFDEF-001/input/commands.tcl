# LEFDEF-001: tech with lef section using every keyword (layer on contact/non-contact, route, routing, cut, contact, obstruction with 2 types, masterslice, overlap, bound, ignore), plus bad lines (ambiguous `rout`, missing names, 3 types, duplicate name). Captures: tech-load error messages; `lef write -tech` output of a trivial cell to observe the classes and canonical names.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {load top}
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 0 0 10 10}
probe {paint boundary}
probe {lef write -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
