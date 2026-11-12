# SCRIPT-API-07: tag select; magic::tag add select {puts A}; magic::tag add select {puts B}; tag select; magic::tag select {}
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {tag select}
probe {magic::tag add select {puts A}}
probe {magic::tag add select {puts B}}
probe {tag select}
probe {select top cell}
probe {magic::tag select {}}
probe {tag select}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
