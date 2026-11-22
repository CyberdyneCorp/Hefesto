# EX-023: extract no aliases, then extract do aliases; a node labeled a, b
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
probe {extract no aliases}
probe {extract all}
file rename -force input/ex.ext input/ex.noaliases.ext
probe {extract do aliases}
probe {extract all}
file rename -force input/ex.ext input/ex.aliases.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
