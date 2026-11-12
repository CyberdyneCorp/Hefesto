# SCRIPT-BOX-01: units microns, scale 0.01, box (0,0)-(1,2)µm; pushbox; set boxstack
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
scalegrid 1 2
units microns
box 0 0 1um 2um
probe {box values}
probe {pushbox}
probe {set boxstack}
probe {units}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
