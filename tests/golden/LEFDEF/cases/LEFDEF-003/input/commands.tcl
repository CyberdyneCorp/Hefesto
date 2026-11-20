# LEFDEF-003: same tech with cifoutput `gridlimit` forcing a 2:1 internal grid (`scalefactor 20`, `gridlimit 10`, `set-minimum-grid`); and a variant with a shrinking grid. Captures: tech-load output and `lef write -tech` WIDTH lines; DEF read default wire width of a net with no width.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {load top}
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 0 0 10 10}
probe {paint boundary}
probe {lef write grid2 -tech}
probe {def read input/nowidth}
probe {writeall force}
probe {tech lambda 1 2}
probe {tech load input/shrink.tech -noprompt}
probe {snap internal}
probe {load top2}
probe {box 0 0 10 10}
probe {paint metal1}
probe {lef write shrink -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
