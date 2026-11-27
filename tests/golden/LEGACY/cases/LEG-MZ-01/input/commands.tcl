# LEG-MZ-01: scmos; iroute saveParameters p.ir (the irouter style's widths and spacings derived from DRC)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {iroute saveParameters p.ir}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
