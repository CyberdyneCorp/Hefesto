# EX-071: before any cell: extract all (no window or box) and extract style (needs none); after load: extract nosuch, extract s and extract st (ambiguous), extract STY, extract list style, extract listall style, extract list path, extract path; then extract all
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
probe {extract all}
probe {extract style}
load ex
box 0 0 1 1
probe {extract nosuch}
probe {extract s}
probe {extract st}
probe {extract STY}
probe {extract list style}
probe {extract listall style}
probe {extract list path}
probe {extract path}
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
