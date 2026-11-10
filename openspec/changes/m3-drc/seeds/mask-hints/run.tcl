proc tcase {name wire hint} {
  load $name
  box {*}$wire
  paint m1
  if {$hint ne ""} { property MASKHINTS_X $hint }
  select top cell
  drc check
  drc catchup
  set n [drc listall count]
  puts "CASE $name hint=($hint) -> $n"
}
drc on
drc style default
# control: no hint
tcase H0 {0 0 20 2} ""
tcase He {0 0 20 2} "-10 -10 30 10"
tcase Ha {0 0 20 2} "-10 -10 30 1"
tcase Hb {0 0 20 2} "-10 -10 30 0"
tcase Hc {0 0 20 2} "-10 2 30 10"
tcase Hd {0 0 20 2} "-10 -10 5 0 -10 0 30 2 -10 2 31 10"
tcase Hd2 {0 0 20 2} "-10 -10 5 0 -10 0 30 10"
tcase Hf {0 0 20 2} "-10 1 30 10"
tcase Ve {0 0 2 20} "-10 -10 10 30"
tcase Va {0 0 2 20} "-10 -10 0 30"
tcase Vb {0 0 2 20} "2 -10 10 30"
tcase Vc {0 0 2 20} "1 -10 10 30"
tcase Vd {0 0 2 20} "-10 -10 1 30"
quit -noprompt
