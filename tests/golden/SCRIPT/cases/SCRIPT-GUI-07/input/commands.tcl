# SCRIPT-GUI-07: GUI: cell manager with a 3-level hierarchy; press each button
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {cellmanager create}
update
proc walk {t n} { foreach c [$t children $n] { puts [list $c [$t item $c -text]]; walk $t $c } }
catch {walk .cellmgr.box.view {}}
proc trace_cmd {cmd op} { puts [list issued $cmd] }
foreach c {magic::load magic::select magic::see magic::expand} { trace add execution $c enter trace_cmd }
foreach b [winfo children .cellmgr.target] { catch {$b invoke} }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
