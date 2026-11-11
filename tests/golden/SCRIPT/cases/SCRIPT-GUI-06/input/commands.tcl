# SCRIPT-GUI-06: GUI: build the toolbar with and without ./<tech>_toolbars.tcl; event-generate the bindings on the poly row
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
file copy input/scmos_toolbars.tcl scmos_toolbars.tcl
proc trace_cmd {cmd op} { puts [list issued $cmd] }
foreach c {magic::paint magic::erase magic::see magic::select} { trace add execution $c enter trace_cmd }
probe {magic::maketoolbar .layout1}
update
foreach w [lsort [info commands .layout1*toolbar*]] { puts $w }
foreach ev {<ButtonPress-1> <ButtonPress-2> <ButtonPress-3>} { catch {event generate .layout1.toolbar.b_poly $ev} }
update
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
