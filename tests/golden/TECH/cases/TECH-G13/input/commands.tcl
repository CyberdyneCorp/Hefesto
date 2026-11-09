# TECH-G13: tech planes, tech layers and tech layers * on tiny.tech.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech planes]
puts [tech layers]
puts [tech layers *]
quit -noprompt
