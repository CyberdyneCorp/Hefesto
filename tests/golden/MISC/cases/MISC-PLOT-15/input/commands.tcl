# MISC-PLOT-15: gf180mcuD gf180mcu_fd_sc_mcu7t5v0__inv_1; plot pnm inv 400
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load gf180mcu_fd_sc_mcu7t5v0__inv_1
probe {plot pnm inv 400}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
