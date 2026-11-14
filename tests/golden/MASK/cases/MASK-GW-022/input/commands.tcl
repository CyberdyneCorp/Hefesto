# MASK-GW-022: A default font point label; font 0 size 80 N rot 90; all 9 justifications; a rect label.
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
probe {box 0 0 100 100}
probe {paint metal1}
probe {box 5 5 5 5}
probe {label plain c metal1}
probe {box 10 10 10 10}
probe {label big FreeSans 80 90 0 0 n metal1}
probe {box 0 50 0 50}
probe {label jn n metal1}
probe {box 10 50 10 50}
probe {label jne ne metal1}
probe {box 20 50 20 50}
probe {label je e metal1}
probe {box 30 50 30 50}
probe {label jse se metal1}
probe {box 40 50 40 50}
probe {label js s metal1}
probe {box 50 50 50 50}
probe {label jsw sw metal1}
probe {box 60 50 60 50}
probe {label jw w metal1}
probe {box 70 50 70 50}
probe {label jnw nw metal1}
probe {box 80 50 80 50}
probe {label jc c metal1}
probe {box 60 60 80 80}
probe {label area c metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
