# SCRIPT-BOX-04: GUI: suspendall; suspendall; resumeall; resumeall; catch resumeall e
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
rename updatedisplay _ud
proc updatedisplay {args} { puts [list updatedisplay {*}$args]; _ud {*}$args }
probe {suspendall}
probe {suspendall}
probe {resumeall}
probe {resumeall}
probe {resumeall}
puts [lsort [array names Winopts]]
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
