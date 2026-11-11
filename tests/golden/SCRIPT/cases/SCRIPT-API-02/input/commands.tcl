# SCRIPT-API-02: catch {magic::bo values} e
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {magic::bo values}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
