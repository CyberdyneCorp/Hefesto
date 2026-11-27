# NG-SET-01 setup
permute default
set x "a"b
property default
foreach dev {nmos pmos} {
  property "-circuit1 $dev" tolerance {W 0.02}
}
property nosuchcell tolerance {w 0.01}
puts done
