foreach dev {nfet pfet} {
  if {[lsearch [cells list -all -circuit1] $dev] >= 0} {
    property "-circuit1 $dev" delete L
  }
}
set x [
nosuch
]
catch {format $env(X)}
proc f {} {return 1}
while {[f] && [incr i] < 10} {}
puts stderr err
