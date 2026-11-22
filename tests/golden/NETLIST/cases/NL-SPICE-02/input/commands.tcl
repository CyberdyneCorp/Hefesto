# NL-SPICE-02: an inverter in formats spice2, spice3, hspice, ngspice and cdl (inv.cdl), and a run with -o custom.spice
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice format spice2}
probe {ext2spice inv}
file rename -force inv.spice inv.spice2.spice
probe {ext2spice format spice3}
probe {ext2spice inv}
file rename -force inv.spice inv.spice3.spice
probe {ext2spice format hspice}
probe {ext2spice inv}
file rename -force inv.spice inv.hspice.spice
probe {ext2spice format ngspice}
probe {ext2spice inv}
file rename -force inv.spice inv.ngspice.spice
probe {ext2spice format cdl}
probe {ext2spice inv}
probe {ext2spice format spice3}
probe {ext2spice -o custom.spice inv}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
