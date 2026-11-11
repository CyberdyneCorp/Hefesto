# SCRIPT-GUI-11: stub tk_getOpenFile/tk_getSaveFile to return fixed paths; promptload gds with a single top and with multiple tops; promptsave magic with (UNNAMED)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
proc tk_getOpenFile {args} { puts [list open {*}$args]; return [file normalize input/inv.mag] }
proc tk_getSaveFile {args} { puts [list save {*}$args]; return [file normalize saved.mag] }
probe {promptload magic}
probe {cellname list window}
probe {promptsave magic}
probe {file exists saved.mag}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
