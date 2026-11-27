foreach c {-circuit1 -circuit2} {
  property "$c r1" add {w double} {l double}
  property "$c r1" series enable
  property "$c r1" series {w critical}
  property "$c r1" series {l add}
  property "$c r1" parallel {w par}
  property "$c r1" merge {l series}
  puts [property "$c r1" series]
}
property parallel none
property parallel all
property parallel connected
property parallel open
property series none
property series all
property parallel bogus
