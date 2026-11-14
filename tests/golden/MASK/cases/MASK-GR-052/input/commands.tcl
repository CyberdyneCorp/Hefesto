# MASK-GR-052: Edit a vendor cell then save; change GDS_START/END through property, then save.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {gds readonly yes}
probe {gds read in}
probe {cellname list allcells}
probe {load a}
probe {cellname writeable a true}
probe {box 0 0 1 1}
probe {paint metal2}
probe {save a}
probe {load b}
probe {property GDS_START "0"}
probe {save b}
probe {cellname timestamp a}
probe {cellname timestamp b}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
