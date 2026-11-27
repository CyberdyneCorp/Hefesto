# EX-064: extract (incremental) twice without edits, then after painting into the child leaf: which .ext files each run rewrites (their modification time is set to 0 after a run and read back after the next)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load ex
box 0 0 1 1
probe {extract}
foreach f {ex leaf} { file mtime input/$f.ext 0 }
probe {extract}
puts "unchanged run rewrote: [lmap f {ex leaf} { list $f [expr {[file mtime input/$f.ext] != 0}] }]"
foreach f {ex leaf} { file mtime input/$f.ext 0 }
load leaf
box 20 0 24 4
paint metal1
load ex
box 0 0 1 1
probe {extract}
puts "run after the edit rewrote: [lmap f {ex leaf} { list $f [expr {[file mtime input/$f.ext] != 0}] }]"
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
