# NL-RES-02: the same layout; threshold 0 and mindelay 0, then all, force, ignore and include: which nets each run writes
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/restest -noprompt
drc off
load wire
box 0 0 1 1
probe {extract all}
probe {extresist threshold 0}
probe {extresist mindelay 0}
probe {extresist threshold}
probe {extresist mindelay}
probe {extresist}
file rename -force wire.res.ext wire.t0.res.ext
probe {extresist all}
file rename -force wire.res.ext wire.all.res.ext
probe {extresist threshold 10000}
probe {extresist mindelay 1}
probe {extresist force in}
probe {extresist force}
probe {extresist}
file rename -force wire.res.ext wire.force.res.ext
probe {extresist ignore out}
probe {extresist}
file rename -force wire.res.ext wire.ignore.res.ext
probe {extresist include out}
probe {extresist}
file rename -force wire.res.ext wire.include.res.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
