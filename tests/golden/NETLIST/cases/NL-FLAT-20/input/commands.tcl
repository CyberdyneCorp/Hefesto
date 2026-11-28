# NL-FLAT-20: ext2spice -c 0.5 and -C (thresholds per run), ext2sim -R with -s L=4 and a -S file holding W=7 and a line without '=', ext2spice -T (unrecognized), then ext2spice -p input:input/sub on top2, whose leaf is only under input/sub (last: Magic keeps the -p path for later runs)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice -c 0.5 top}
file rename -force top.spice top.c05.spice
probe {ext2spice -C top}
file rename -force top.spice top.cinf.spice
probe {ext2sim -R -s L=4 -S input/syms top}
file rename -force top.sim top.syms.sim
probe {ext2spice -T nltest top}
probe {ext2spice -p input:input/sub top2}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
