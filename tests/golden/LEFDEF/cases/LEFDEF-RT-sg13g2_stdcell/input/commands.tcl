# LEFDEF-RT-sg13g2_stdcell: Library round trip of ihp-sg13g2 sg13g2_stdcell: lef read of sg13g2_tech.lef and sg13g2_stdcell.lef, a wrapper placing every cell read, lef writeall -notop.
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
probe {lef read input/lef/sg13g2_tech.lef}
probe {lef read input/lef/sg13g2_stdcell.lef}
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
if {[file exists lefdef_rt.mag]} {file rename -force lefdef_rt.mag lefdef_rt.lef}
puts [list lefdef-rt written [file exists lefdef_rt.lef]]
if {![file exists lefdef_rt.lef]} {puts [list lefdef-rt path [path search]]}
if {[file exists lefdef_rt.lef]} {
    set rt_in [open lefdef_rt.lef]; set rt_lines [split [read $rt_in] \n]; close $rt_in
    set rt_out [open lefdef_rt.probe.lef w]; set rt_on 0
    foreach line $rt_lines {
        if {[regexp {^MACRO (\S+)} $line -> m]} {set rt_on [expr {[lsearch -exact {sg13g2_sdfbbp_1} $m] >= 0}]}
        if {$rt_on} {puts $rt_out $line}
        if {$rt_on && [regexp {^END (\S+)} $line -> m] && [lsearch -exact {sg13g2_sdfbbp_1} $m] >= 0} {set rt_on 0}
    }
    close $rt_out
}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
