# NL-HIER-04: a top without ports (subcircuits top auto) and one with ports, hierarchical, then under the lvs preset
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice hierarchy on}
probe {ext2spice topnp}
file rename -force topnp.spice topnp.hier.spice
probe {ext2spice topp}
file rename -force topp.spice topp.hier.spice
probe {ext2spice lvs}
probe {ext2spice topnp}
file rename -force topnp.spice topnp.lvs.spice
probe {ext2spice topp}
file rename -force topp.spice topp.lvs.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
