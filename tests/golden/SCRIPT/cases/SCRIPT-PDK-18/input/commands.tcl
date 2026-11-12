# SCRIPT-PDK-18: older rc generation (design D15): source the 0fe599b2 sky130A .magicrc and the ddb601a4 ihp-sg13g2.magicrc (copies in the case input, Apache-2.0) against the new-pin tech files; print $VDD $GND $SU...
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
source input/old/sky130A.magicrc
puts [list $VDD $GND $SUB [tech name] [tech lambda] [snap] [path search] $PDKPATH [info exists PDKNAMESPACE] [array get Opts callback] [tech locked]]
tech load input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tech -noprompt
source input/old/ihp-sg13g2.magicrc
random seed 1
puts [list $VDD $GND $SUB [tech name] [tech lambda] [snap] [path search] $PDKPATH [info exists PDKNAMESPACE] [array get Opts callback] [tech locked]]
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
