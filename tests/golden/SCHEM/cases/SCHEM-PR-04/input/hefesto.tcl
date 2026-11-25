file mkdir out
# SCHEM-PR-04: xschem translate on chosen instances; the instance is addressed by its index.
xschem load input/pr04.sch
proc index_of {name} {
  for {set i 0} {$i < [xschem get instances]} {incr i} {
    if {[xschem getprop instance $i name] eq $name} { return $i }
  }
  return -1
}
proc show {name text} {
  if {[catch {set r [xschem translate [index_of $name] $text]} err]} { set r "error: $err" }
  puts "[list $name $text] -> \[$r\]"
}
foreach {name text} {
  R7 {@name=@value}
  X1 {a\@b @name}
  M1 {@W/@L}
  M1 {@W @L}
  R7 {%undef}
  R7 {@undef}
  R7 {@symname}
  R7 {@symname_ext}
  R7 {@#0:pinnumber}
  R7 {@#1:name}
  R7 {@@P}
} { show $name $text }
puts "no instance: [list [xschem translate -1 {@name %undef}]]"
