# EXTRACT-PDK-gf180mcu_fd_sc_mcu7t5v0: gf180mcuD gf180mcu_fd_sc_mcu7t5v0 (229 cells, the .mag files of libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag): every cell placed in a wrapper pdk_lib, extract path ext, extract all with the PDK's default style: one .ext per cell
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {drc off}
set cells {}
foreach mag [lsort [glob -nocomplain -directory input/pdk/gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag *.mag]] {
    lappend cells [file rootname [file tail $mag]]
}
puts [list pdk cells [llength $cells]]
probe {load pdk_lib}
set row 0
foreach cell $cells {
    box 0 $row 0 $row
    if {[catch {getcell $cell} err]} {puts [list pdk getcell-error $cell $err]}
    incr row 20000
}
probe {select clear}
probe {box 0 0 1 1}
file mkdir ext
probe {extract path ext}
probe {extract all}
puts [list pdk ext-files [llength [glob -nocomplain -directory ext *.ext]]]
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
