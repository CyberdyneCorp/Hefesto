# CELL-TS-07: gds readonly true: vendor cells va and vb read from GDS (content-hash stamps) placed in a new parent; the parent edited and saved: the vendor stamps in the use records.
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
probe {gds readonly true}
probe {gds read vendor}
probe {cellname list allcells}
probe {cellname timestamp va}
probe {cellname timestamp vb}
probe {load parent}
probe {box 0 0 0 0}
probe {getcell va}
probe {box 20 0 20 0}
probe {getcell vb}
probe {box 0 20 10 30}
probe {paint metal2}
probe {save parent}
probe {cellname timestamp va}
probe {cellname timestamp vb}
probe {cellname flags va}
probe {cellname flags parent}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
