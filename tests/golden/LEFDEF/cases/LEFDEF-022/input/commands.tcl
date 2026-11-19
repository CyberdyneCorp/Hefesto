# LEFDEF-022: `lef read x.lef -import -anno -labels -noblockage -foo`; `lef read -annotate x.lef`. Captures: messages; cells created.
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
probe {lef read input/x.lef -import -anno -labels -noblockage -foo}
probe {cellname list allcells}
probe {lef read -annotate input/x.lef}
probe {cellname list allcells}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
