# SCRIPT-GUI-03: GUI: invoke each menu item via $m invoke $i with a layout loaded
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load inv
rename magic::tkcon _tkcon
proc magic::tkcon {args} {}
foreach mb [lsort [winfo children .layout1.titlebar.mbuttons]] {
    set m $mb.toolmenu
    if {[catch {$m index end} n] || $n eq {none}} continue
    for {set i 0} {$i <= $n} {incr i} {
        if {[catch {$m entrycget $i -command} c] || $c eq {}} continue
        if {[string match *quit* $c] || [string match *closewrapper* $c] || [string match *tk_get* $c]} continue
        puts [list $mb $i [$m entrycget $i -label] $c]
        puts [list [catch {$m invoke $i} r] $r]
    }
}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
