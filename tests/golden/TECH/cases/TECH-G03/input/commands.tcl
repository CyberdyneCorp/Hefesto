# TECH-G03: version 1.0 beta and description "a b".
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech version]
quit -noprompt
