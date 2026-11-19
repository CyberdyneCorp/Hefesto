# LEFDEF-RT-gf180mcu_fd_sc_mcu7t5v0: Library round trip of gf180mcuD gf180mcu_fd_sc_mcu7t5v0: lef read of gf180mcu_fd_sc_mcu7t5v0__nom.tlef and gf180mcu_fd_sc_mcu7t5v0.lef, a wrapper placing every cell read, lef writeall -notop.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {drc off}
probe {lef datestamp 0}
set rt_before [cellname list allcells]
probe {lef read input/lef/gf180mcu_fd_sc_mcu7t5v0__nom.tlef}
probe {lef read input/lef/gf180mcu_fd_sc_mcu7t5v0.lef}
set rt_new {}
foreach cell [lsort [cellname list allcells]] {
    if {[lsearch -exact $rt_before $cell] < 0} {lappend rt_new $cell}
}
puts [list lefdef-rt cells [llength $rt_new]]
probe {load lefdef_rt}
probe {box 0 0 0 0}
foreach cell $rt_new {
    if {[catch {getcell $cell} err]} {puts [list lefdef-rt getcell-error $cell $err]}
}
probe {select clear}
probe {lef writeall -notop}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
