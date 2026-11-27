# EXTRACT-PDK-sg13g2_stdcell: ihp-sg13g2 sg13g2_stdcell (84 cells, sg13g2_stdcell.gds of the ihp-gds-sc set (gds read)): every cell placed in a wrapper pdk_lib, extract path ext, extract all with the PDK's default style: one .ext per cell
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {drc off}
set pdk_before [cellname list allcells]
probe {gds read input/lib/sg13g2_stdcell.gds}
set cells {}
foreach cell [lsort [cellname list allcells]] {
    if {[lsearch -exact $pdk_before $cell] < 0} {lappend cells $cell}
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
