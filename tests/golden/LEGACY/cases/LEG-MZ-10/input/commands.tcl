# LEG-MZ-10: iroute route -foo; -slabel nosuch; start already connected to the destination; Magic's exit status is recorded because its first capture ended in SIGSEGV (exit 139)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute route -foo}
probe {iroute route -slabel nosuch -dlabel B}
probe {iroute route -slabel A -dlabel B}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
