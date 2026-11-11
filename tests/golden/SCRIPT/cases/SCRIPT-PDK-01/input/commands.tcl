# SCRIPT-PDK-01: PDK_ROOT=/pdk magic -dnull -noconsole -rcfile /pdk/<tech>/libs.tech/magic/<tech>.magicrc t.tcl for sky130A, gf180mcuD and ihp-sg13g2 (IHP with PDK_ROOT unset), where t.tcl prints $VDD $GND $SUB [te...
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
puts [list $VDD $GND $SUB [tech name] [tech lambda] [snap] [path search] $PDKPATH [info exists PDKNAMESPACE] [array get Opts callback] [tech locked]]
tech load input/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.tech -noprompt
source input/pdk/gf180mcuD/libs.tech/magic/gf180mcuD.magicrc
random seed 1
puts [list $VDD $GND $SUB [tech name] [tech lambda] [snap] [path search] $PDKPATH [info exists PDKNAMESPACE] [array get Opts callback] [tech locked]]
unset env(PDK_ROOT)
tech load input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tech -noprompt
source input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.magicrc
random seed 1
puts [list $VDD $GND $SUB [tech name] [tech lambda] [snap] [path search] $PDKPATH [info exists PDKNAMESPACE] [array get Opts callback] [tech locked]]
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
