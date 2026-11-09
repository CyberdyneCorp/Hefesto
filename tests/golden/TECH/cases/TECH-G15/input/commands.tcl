# TECH-G15: tech layers on type lists.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech layers m1]
puts [tech layers pc]
puts [tech layers *m1]
puts [tech layers ~(pc,ndc)/active]
puts [tech layers 0]
puts [tech layers 0,poly]
puts [tech layers (poly,ndiff]
puts [tech layers allm1]
quit -noprompt
