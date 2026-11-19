# OASIS-064-triangles: Round trip of layout triangles: oasis write then oasis read (Magic: gds write, gds read); the cells read back.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
proc rep {script} {
    set code [catch {uplevel #0 $script} result]
    set f [open report.txt a]
    puts $f [list $code $result]
    close $f
}
proc specrep {value} {
    set f [open report.txt a]
    puts $f [list 0 $value]
    close $f
}
probe {drc off}
probe {tech load input/oastest -noprompt}
probe {snap internal}
probe {load top}
probe {box 0 0 8 8}
probe {splitpaint ne metal1}
probe {box 10 0 18 8}
probe {splitpaint nw metal1}
probe {box 20 0 28 8}
probe {splitpaint se metal1}
probe {box 30 0 38 8}
probe {splitpaint sw metal1}
probe {oasis write rt.oas}
probe {oasis read rt.oas}
rep {lsort [cellname list allcells]}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
