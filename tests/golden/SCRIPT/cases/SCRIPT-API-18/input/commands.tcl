# SCRIPT-API-18: box (0,0)-(30,100), scale 0.005; box values under units default, internal, lambda, microns, grid (grid 10 20), each with print and noprint
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
tech load input/pdk/sky130A/libs.tech/magic/sky130A.tech -noprompt
scalegrid 1 2
snap internal
box 0 0 30 100
grid 10 20
probe {units default}
probe {box values}
probe {units default print}
probe {box values}
probe {units default noprint}
probe {box values}
probe {units internal}
probe {box values}
probe {units internal print}
probe {box values}
probe {units internal noprint}
probe {box values}
probe {units lambda}
probe {box values}
probe {units lambda print}
probe {box values}
probe {units lambda noprint}
probe {box values}
probe {units microns}
probe {box values}
probe {units microns print}
probe {box values}
probe {units microns noprint}
probe {box values}
probe {units grid}
probe {box values}
probe {units grid print}
probe {box values}
probe {units grid noprint}
probe {box values}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
