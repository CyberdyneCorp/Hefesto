# EX-016: substrate *psd,space,pwell well and defaultareacap metal1 metal1 well 30; metal1 over a pwell of the substrate node: no cap record names the substrate, metal1's node C holds the value
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
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
