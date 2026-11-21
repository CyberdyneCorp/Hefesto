# NL-SPICE-04: HSPICE with deep names (> 15 characters), flat and hierarchical: x<n>/ names, z@ names and the dictionary
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice format hspice}
probe {ext2spice top}
file rename -force top.spice top.flat.spice
probe {ext2spice hierarchy on}
probe {ext2spice top}
file rename -force top.spice top.hier.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
