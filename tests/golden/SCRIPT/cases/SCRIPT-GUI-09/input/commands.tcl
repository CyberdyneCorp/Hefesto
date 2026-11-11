# SCRIPT-GUI-09: scmos layout with known DRC errors: drc_save_report; drc_load_report; feedback find
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load err
select top cell
drc check
drc catchup
probe {drc listall why}
probe {drc_save_report err drc.out}
probe {feedback clear}
probe {drc_load_report drc.out}
probe {feedback count}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
