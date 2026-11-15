# MASK-PROBE-sky130A: sky130A: every output style's CIF layers (cif see of an unknown layer lists them) and every input style's 'Input style' line (cif istyle).
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {drc off}
probe {load mask_probe}
probe {box 0 0 1 1}
probe {cif listall ostyle}
probe {cif listall istyle}
foreach style [cif listall ostyle] {
    probe [list cif ostyle $style]
    probe {cif see mask_probe_no_such_layer}
}
foreach style [cif listall istyle] {
    probe [list cif istyle $style]
}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
