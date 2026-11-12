# SCRIPT-GUI-08: GUI: library manager over a dir with mixed-tech and malformed .mag files, Filter on and off
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {libmanager create}
catch {.libmgr.title.path delete 0 end; .libmgr.title.path insert 0 input/lib}
update
catch {magic::libmanager update}
catch {foreach c [.libmgr.box.view children {}] { puts [.libmgr.box.view item $c -text] }}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
