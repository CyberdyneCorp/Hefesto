# MISC-PLOT-16: ihp-sg13g2 sg13g2_inv_1 (read from the sg13g2_stdcell LEF: pins and obstructions); plot pnm inv 400
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {lef read input/lef/sg13g2_stdcell.lef}
load sg13g2_inv_1
probe {plot pnm inv 400}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
