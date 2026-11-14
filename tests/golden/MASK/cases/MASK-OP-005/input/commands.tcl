# MASK-OP-005: options grow-euclidean: triangles of slopes 1, 2 and 1/3 grown and shrunk by 10.
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
probe {box 0 0 10 10}
probe {splitpaint ne metal1}
probe {box 20 0 30 20}
probe {splitpaint ne metal1}
probe {box 40 0 70 10}
probe {splitpaint ne metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
