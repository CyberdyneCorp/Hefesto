# TECH-G16: Aliases diff, poly (shadowing), allm1 twice and a types-section alias with '*'.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech layers]
puts [tech layers diff]
quit -noprompt
