# MASK-CI-001: A style gds and style x variants (p),(c): cif listall istyle and the default style.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {load top}
probe {cif listall istyle}
probe {cif list istyle}
probe {cif istyle}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
