# TECH-G17: A types line with an unknown plane followed by a valid line.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech layers m3]
puts [tech layers foo]
quit -noprompt
