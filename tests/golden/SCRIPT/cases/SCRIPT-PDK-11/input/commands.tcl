# SCRIPT-PDK-11: sky130A and ihp-sg13g2: magic::u2i 0.17, magic::i2u 34, magic::u2l 0.17, magic::l2u 1, magic::float2spice 4.2e-7, magic::3digitpastdecimal 0.12345
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {magic::u2i 0.17}
probe {magic::i2u 34}
probe {magic::u2l 0.17}
probe {magic::l2u 1}
probe {magic::float2spice 4.2e-7}
probe {magic::3digitpastdecimal 0.12345}
tech load input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.tech -noprompt
source input/pdk/ihp-sg13g2/libs.tech/magic/ihp-sg13g2.magicrc
random seed 1
probe {magic::u2i 0.17}
probe {magic::i2u 34}
probe {magic::u2l 0.17}
probe {magic::l2u 1}
probe {magic::float2spice 4.2e-7}
probe {magic::3digitpastdecimal 0.12345}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
