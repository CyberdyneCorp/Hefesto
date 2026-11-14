# MASK-OS-008: Layer lists with *metal1, a parenthesised list, an alias and duplicate templayer names.
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
probe {gds datestamp 0}
probe {load top}
probe {box 0 0 10 4}
probe {paint metal1}
probe {box 0 6 4 10}
probe {paint via1}
probe {box 6 6 10 10}
probe {paint pdc}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
