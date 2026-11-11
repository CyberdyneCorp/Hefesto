# SCRIPT-PCELL-13: GUI: open the dialog for a new toy device; dump widget tree names, magic::gencell_getparams; change an entry with a dependency callback
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
source input/toy.tcl
rename toy::nfet_dep toy::_dep
proc toy::nfet_dep {parameters {target {}}} { puts [list dep $target]; return $parameters }
magic::gencell toy::nfet
proc tree {w} { puts $w; foreach c [winfo children $w] { tree $c } }
tree .params
probe {magic::gencell_getparams}
catch {.params.body.area.w.entry delete 0 end; .params.body.area.w.entry insert 0 3}
catch {event generate .params.body.area.w.entry <Return>}
probe {magic::gencell_getparams}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
