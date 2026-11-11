# SCRIPT-PDK-12: sky130A: magic::spice2float over {2k, 1.5, 0.42u, 10n, 1nf, 10pf, 3meg, 5a, abc}
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {magic::spice2float 2k}
probe {magic::spice2float 1.5}
probe {magic::spice2float 0.42u}
probe {magic::spice2float 10n}
probe {magic::spice2float 1nf}
probe {magic::spice2float 10pf}
probe {magic::spice2float 3meg}
probe {magic::spice2float 5a}
probe {magic::spice2float abc}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
