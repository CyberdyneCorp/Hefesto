puts [equate classes "-circuit1 XY_inv" "-circuit2 inv"]
puts [equate -unique classes "-circuit1 XY_inv" "-circuit2 inv"]
puts [equate classes "-circuit1 nosuch" "-circuit2 inv"]
equate classes "-circuit1 XY_inv" "-circuit1 blk"
equate classes "-circuit1 blk" {p q} "-circuit2 blk2" {x}
equate classes "-circuit1 blk" {p q r} "-circuit2 blk2" {x y r}
equate classes "-circuit1 blk" {{p 1 2} q} "-circuit2 blk2" {x y}
equate classes "-circuit1 blk" {{p 1} q} "-circuit2 blk2" {x y}
equate classes "-circuit1 blk" {p q} "-circuit2 blk2" {y x}
