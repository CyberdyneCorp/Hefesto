# MISC-3D-04: specialopen wind3d nosuchcell with no 3D window; open c3d; a second specialopen wind3d, with and without a cell name; the open window still answers
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/v3d -noprompt
load c3d
box 0 0 100 50
probe {specialopen wind3d nosuchcell}
probe {specialopen wind3d c3d}
probe {specialopen wind3d}
probe {specialopen wind3d nosuchcell}
probe {.magic3d view}
probe {.magic3d level}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
