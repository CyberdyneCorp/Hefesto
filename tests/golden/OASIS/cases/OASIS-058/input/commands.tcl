# OASIS-058: The reference cell with validation none, crc32 and checksum: END bytes and signatures.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
proc rep {script} {
    set code [catch {uplevel #0 $script} result]
    set f [open report.txt a]
    puts $f [list $code $result]
    close $f
}
proc specrep {value} {
    set f [open report.txt a]
    puts $f [list 0 $value]
    close $f
}
probe {drc off}
probe {tech load input/oastest -noprompt}
probe {snap internal}
probe {load top}
probe {box 0 0 4 2}
probe {paint metal1}
probe {oasis compress 0}
probe {oasis validation none}
probe {oasis write none.oas}
probe {oasis validation crc32}
probe {oasis write crc32.oas}
probe {oasis validation checksum}
probe {oasis write checksum.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
