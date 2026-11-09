# TECH-G14: tech layers <x> for prefixes and qualified names.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech layers p]
puts [tech layers po]
puts [tech layers poly]
puts [tech layers polys]
puts [tech layers ndif]
puts [tech layers m]
puts [tech layers cp]
puts [tech layers CP]
puts [tech layers EP]
puts [tech layers f]
puts [tech layers pc/metal1]
puts [tech layers poly/metal1]
quit -noprompt
