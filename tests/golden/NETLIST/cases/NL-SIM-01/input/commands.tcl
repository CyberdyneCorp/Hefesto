# NL-SIM-01: inverter whose node out is also named y (equiv): MIT, LBL and SU with alias off (the = line in the .sim); alias on (the = line in inv.al); then labels on with alias still on (inv.nodes)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2sim format MIT}
probe {ext2sim inv}
file rename -force inv.sim inv.mit.sim
probe {ext2sim format LBL}
probe {ext2sim inv}
file rename -force inv.sim inv.lbl.sim
probe {ext2sim format SU}
probe {ext2sim inv}
file rename -force inv.sim inv.su.sim
probe {ext2sim format MIT}
probe {ext2sim alias on}
probe {ext2sim inv}
file rename -force inv.sim inv.alias.sim
file rename -force inv.al inv.alias.al
probe {ext2sim labels on}
probe {ext2sim inv}
file rename -force inv.sim inv.labels.sim
file rename -force inv.al inv.labels.al
file rename -force inv.nodes inv.labels.nodes
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
