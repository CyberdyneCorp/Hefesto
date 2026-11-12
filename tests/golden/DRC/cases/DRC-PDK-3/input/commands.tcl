# DRC-PDK-3: ihp-sg13g2: two MiM capacitors of the PDK's cap_cmim generator (no cap_cmomf generator in IHP-Open-PDK) with the plates 0.4 um apart (MIM.b 0.6 um); CAP.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
load mim
box values 0 0 0 0
probe {magic::gencell sg13g2::cap_cmim C1}
box values 0 0 0 0
probe {box move e 2.4um}
probe {magic::gencell sg13g2::cap_cmim C2}
probe {cellname list childinst mim}
probe {select top cell}
probe {box grow center 100}
probe {drc check}
probe {drc catchup}
probe {drc listall why}
probe {drc list count total}
probe {save mim}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
