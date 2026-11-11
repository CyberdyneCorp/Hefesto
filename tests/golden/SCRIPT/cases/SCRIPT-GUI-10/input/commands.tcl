# SCRIPT-GUI-10: GUI: text helper new label with and without a layer, sticky, port; edit an existing label
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box 0 0 10 10
paint m1
proc trace_cmd {cmd op} { puts [list issued $cmd] }
foreach c {magic::label magic::setlabel magic::port} { trace add execution $c enter trace_cmd }
probe {magic::make_texthelper}
catch {.texthelper.text.tentry insert 0 NEW}
catch {.texthelper.buttonbar.apply invoke}
catch {.texthelper.layer.tentry insert 0 m1}
catch {.texthelper.buttonbar.apply invoke}
select area label
probe {magic::make_texthelper}
catch {.texthelper.buttonbar.apply invoke}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
