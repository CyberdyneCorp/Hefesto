# TECH-G32: A tech file beginning with version.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
catch {tech load input/late -noprompt} result; puts $result
puts [tech name]
quit -noprompt
