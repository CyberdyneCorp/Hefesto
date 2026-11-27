format 30
format 0
format 50 20
puts [format]
cells list -all -circuit1
cells -top
ports {inv 0}
nodes {inv 0}
contents {top 1}
describe {cell 0}
leaves {top 0}
canonical {inv 1}
help
log start
readnet x.spice
