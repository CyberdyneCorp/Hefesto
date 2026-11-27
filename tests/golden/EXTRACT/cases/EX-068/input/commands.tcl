# EX-068: extract do length, drivers d d2 nosuch and receivers r c1/r: a straight metal1 wire (ex), then a metal1 ring through a metal2 bridge with a receiver in a subcell (exl): the distance records and the missing drivers' messages
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
probe {extract do length}
probe {extract length driver d d2 nosuch}
probe {extract length receiver r c1/r}
probe {extract all}
load exl
box 0 0 1 1
probe {extract do length}
probe {extract length driver d d2 nosuch}
probe {extract length receiver r c1/r}
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
