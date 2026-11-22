# EX-017: resist metal1 80; metal1 10x2 and 4x4: R = 400 and 80 with rscale 1, 0 in style exttest1000 (rscale 1000)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load ex
box 0 0 1 1
probe {extract all}
file rename -force input/ex.ext input/ex.r1.ext
probe {extract style exttest1000}
probe {extract all}
file rename -force input/ex.ext input/ex.r1000.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
