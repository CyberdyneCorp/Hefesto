# OASIS-012: gdstk-written files with CRC32 and CHECKSUM32 signatures, and copies with one END padding byte changed: every file's cells are read (a bad signature is a warning only).
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
probe {gds datestamp 1}
probe {gds read crc32}
probe {gds read checksum32}
probe {gds read crc32_bad}
probe {gds read checksum32_bad}
probe {load anchor}
probe {box 0 0 1 1}
probe {paint metal1}
rep {lsort [cellname list allcells]}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
