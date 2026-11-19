# OASIS-056: The same layout written with compress 0 and compress 9: both give Magic's geometry; the CBLOCK bytes are compared across platforms by the replay (Q-OASIS-64).
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
probe {box 0 0 2 30}
probe {paint metal1}
probe {box 4 0 6 30}
probe {paint metal1}
probe {box 8 0 10 30}
probe {paint metal1}
probe {box 12 0 14 30}
probe {paint metal1}
probe {box 16 0 18 30}
probe {paint metal1}
probe {box 20 0 22 30}
probe {paint metal1}
probe {box 24 0 26 30}
probe {paint metal1}
probe {box 28 0 30 30}
probe {paint metal1}
probe {box 32 0 34 30}
probe {paint metal1}
probe {box 36 0 38 30}
probe {paint metal1}
probe {box 40 0 42 30}
probe {paint metal1}
probe {box 44 0 46 30}
probe {paint metal1}
probe {box 48 0 50 30}
probe {paint metal1}
probe {box 52 0 54 30}
probe {paint metal1}
probe {box 56 0 58 30}
probe {paint metal1}
probe {box 60 0 62 30}
probe {paint metal1}
probe {box 64 0 66 30}
probe {paint metal1}
probe {box 68 0 70 30}
probe {paint metal1}
probe {box 72 0 74 30}
probe {paint metal1}
probe {box 76 0 78 30}
probe {paint metal1}
probe {box 80 0 82 30}
probe {paint metal1}
probe {box 84 0 86 30}
probe {paint metal1}
probe {box 88 0 90 30}
probe {paint metal1}
probe {box 92 0 94 30}
probe {paint metal1}
probe {box 96 0 98 30}
probe {paint metal1}
probe {box 100 0 102 30}
probe {paint metal1}
probe {box 104 0 106 30}
probe {paint metal1}
probe {box 108 0 110 30}
probe {paint metal1}
probe {box 112 0 114 30}
probe {paint metal1}
probe {box 116 0 118 30}
probe {paint metal1}
probe {box 120 0 122 30}
probe {paint metal1}
probe {box 124 0 126 30}
probe {paint metal1}
probe {box 128 0 130 30}
probe {paint metal1}
probe {box 132 0 134 30}
probe {paint metal1}
probe {box 136 0 138 30}
probe {paint metal1}
probe {box 140 0 142 30}
probe {paint metal1}
probe {box 144 0 146 30}
probe {paint metal1}
probe {box 148 0 150 30}
probe {paint metal1}
probe {box 152 0 154 30}
probe {paint metal1}
probe {box 156 0 158 30}
probe {paint metal1}
probe {box 0 40 200 44}
probe {paint metal2}
probe {oasis compress 0}
probe {oasis write c0.oas}
probe {oasis compress 9}
probe {oasis write c9.oas}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
