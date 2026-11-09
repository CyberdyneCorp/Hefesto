# TECH-G42: tech lambda, tech lambda 2, tech lambda 3 2.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech lambda]
puts [tech lambda 2]
puts [tech lambda 3 2]
quit -noprompt
