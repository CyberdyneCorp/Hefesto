file mkdir out
xschem load pdk/sky130A/libs.tech/xschem/sky130_tests/test_inv.sch
for {set i 0} {$i < [xschem get instances]} {incr i} {
  set name [xschem getprop instance $i name]
  puts "$name: [list [xschem translate $i {@spice_get_voltage}]] [list [xschem translate $i {@spice_get_current}]]"
}
