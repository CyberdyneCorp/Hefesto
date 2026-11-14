# MASK-HG-003: Cell bbox (0,0)-(10,10) with FIXED_BBOX -20 -20 30 30 and a bbox layer: GDS and CIF.
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
probe {load top}
probe {gds write out}
probe {cif write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
