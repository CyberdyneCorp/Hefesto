# MASK-GR-055: gds warning limit, none and redirect err.txt with a file producing more than 100 errors.
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
probe {gds warning limit}
probe {cellname list allcells}
probe {gds read in}
probe {gds warning none}
probe {gds read in}
probe {gds warning redirect err.txt}
probe {gds read in}
probe {gds warning}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
