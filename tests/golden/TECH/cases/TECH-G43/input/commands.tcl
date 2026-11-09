# TECH-G43: Lock listing, lock/unlock/revert.
# Reference script (Magic, Tcl): results printed with puts.
catch {tech load input/tiny -noprompt} result; puts $result
puts [tech locked]
puts [tech locked metal1]
puts [tech locked]
puts [tech unlocked pwell]
puts [tech locked]
puts [tech revert *]
puts [tech locked]
puts [tech revert]
puts [tech locked]
quit -noprompt
