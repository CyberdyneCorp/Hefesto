# SCRIPT-GUI-05: GUI: techmanager create with PDK_ROOT set; dump menu entries
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {techmanager create}
set m .techmgr.title.tname.menu
catch {for {set i 0} {$i <= [$m index end]} {incr i} { puts [list $i [$m entrycget $i -label] [$m entrycget $i -command]] }}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
