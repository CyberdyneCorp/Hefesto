# EX-P03: an nfet and a pfet touching in one device region (Round-3 follow-up probe): records, terminals and whether the ID-layer walk ends
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load abut
box 0 0 1 1
probe {extract all}
load abutrev
box 0 0 1 1
probe {extract all}
probe {extract style probeid}
load abutid
box 0 0 1 1
puts {extract abutid: start}
flush stdout
probe {extract all}
puts {extract abutid: returned}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
