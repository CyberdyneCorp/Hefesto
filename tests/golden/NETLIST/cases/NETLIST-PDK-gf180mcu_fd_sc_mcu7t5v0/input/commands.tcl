# NETLIST-PDK-gf180mcu_fd_sc_mcu7t5v0: gf180mcuD gf180mcu_fd_sc_mcu7t5v0: the .ext files captured by EXTRACT-PDK-gf180mcu_fd_sc_mcu7t5v0; per cell ext2spice lvs with hierarchy off (flat/) and ext2spice hierarchy on format ngspice (hier/); ext2sim of the wrapper pdk_lib (the whole library)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {drc off}
addpath input/ext
set cells {}
foreach ext [lsort [glob -nocomplain -directory input/ext *.ext]] {
    set cell [file rootname [file tail $ext]]
    if {$cell ne "pdk_lib"} {lappend cells $cell}
}
puts [list pdk cells [llength $cells]]
file mkdir flat hier
probe {ext2spice lvs}
probe {ext2spice hierarchy off}
foreach cell $cells {
    probe [list ext2spice $cell]
    if {[file exists $cell.spice]} {file rename -force $cell.spice flat/$cell.spice}
}
probe {ext2spice default}
probe {ext2spice hierarchy on}
probe {ext2spice format ngspice}
foreach cell $cells {
    probe [list ext2spice $cell]
    if {[file exists $cell.spice]} {file rename -force $cell.spice hier/$cell.spice}
}
probe {ext2sim pdk_lib}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
