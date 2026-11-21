# NETLIST-SMOKE-01: capture smoke (m7-extraction task 2.3): M6's smoke layout through extract all, ext2spice, ext2sim and extresist; every M7 output collected by name pattern
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/extsmoke.tech -noprompt}
probe {load top}
# extract finds its cell from the box when there is no window (-dnull).
probe {box 0 0 1 1}
probe {extract all}
probe {ext2spice}
probe {ext2sim alias on}
probe {ext2sim labels on}
probe {ext2sim}
probe {extresist lumped on}
probe {extresist all}
probe {extresist fasthenry}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
