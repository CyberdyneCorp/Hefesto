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
tcase Vf {0 0 2 20} "0 -10 1 30"
tcase Vh {0 0 2 20} "0 5 1 10"
tcase Hg {0 0 20 2} "5 -5 15 5"
tcase Hh {0 0 20 2} "5 -5 15 5 -100 0 -90 10"
tcase Hi {0 0 20 2} "5 -5 15 5 100 0 110 10"
tcase Hj {0 0 20 2} "-10 -5 30 5 -100 0 -90 10"
quit -noprompt
