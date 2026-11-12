# SCRIPT-PCELL-02: pure Tcl: magic::murmur3_32 over {"", a, ab, abc, abcd, hello, 0.15;1;1;, "é;"}
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {magic::murmur3_32 {}}
probe {magic::murmur3_32 a}
probe {magic::murmur3_32 ab}
probe {magic::murmur3_32 abc}
probe {magic::murmur3_32 abcd}
probe {magic::murmur3_32 hello}
probe {magic::murmur3_32 {0.15;1;1;}}
probe {magic::murmur3_32 é;}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
