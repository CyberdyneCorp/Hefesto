# LEG-MZ-26: routes from -sPoint to -dRect, to -dSelection and to -dBox
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute route -sPoint 2 2 -dRect 100 0 104 4}
box 100 40 104 44
select area metal1
probe {iroute route -sPoint 2 2 -dSelection}
select clear
box 100 80 104 84
probe {iroute route -sPoint 2 2 -dBox}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
