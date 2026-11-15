# MISC-FL-05: Font labels with each of the 9 justifications (FreeSans and FreeSerif, sizes 8..72, rotations 0..315) on metal1; gds write; the cell renamed; gds read of the output; the re-read cell saved.
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
probe {box 5 50 5 50}
probe {label jn FreeSans 8 0 0 0 n metal1}
probe {box 15 50 15 50}
probe {label jne FreeSerif 16 90 0 0 ne metal1}
probe {box 25 50 25 50}
probe {label je FreeSans 24 180 0 0 e metal1}
probe {box 35 50 35 50}
probe {label jse FreeSerif 32 270 0 0 se metal1}
probe {box 45 50 45 50}
probe {label js FreeSans 40 45 0 0 s metal1}
probe {box 55 50 55 50}
probe {label jsw FreeSerif 48 0 0 0 sw metal1}
probe {box 65 50 65 50}
probe {label jw FreeSans 56 90 0 0 w metal1}
probe {box 75 50 75 50}
probe {label jnw FreeSerif 64 315 0 0 nw metal1}
probe {box 85 50 85 50}
probe {label jcenter FreeSans 72 0 0 0 center metal1}
probe {gds write out}
probe {cellname rename top orig}
probe {gds read out}
probe {load top}
probe {save top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
