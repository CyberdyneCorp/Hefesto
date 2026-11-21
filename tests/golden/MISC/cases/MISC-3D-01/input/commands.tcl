# MISC-3D-01: render MET1 metal1 1.376 0.36 in the output style; specialopen wind3d; .magic3d render of MET1, of MET2 (no render statement) and of an unknown layer
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
probe {specialopen wind3d}
probe {.magic3d render MET1}
probe {.magic3d render MET2}
probe {.magic3d render NOSUCH}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
